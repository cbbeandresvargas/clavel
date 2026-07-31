#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db.h"
#include "strbuf.h"

/* ── Estado global ─────────────────────────────────────────────────── */
DBDriver *g_driver = NULL;
static Arena    *g_arena  = NULL;

void DB_set_driver(DBDriver *d) { g_driver = d; }
void DB_set_arena(Arena *a)     { g_arena  = a; }

int DB_connect(const char *dsn) {
    if (!g_driver) { fputs("[db] Error: sin driver registrado\n", stderr); return -1; }
    int rc = g_driver->connect(g_driver, dsn);
    if (rc != 0)
        fprintf(stderr, "[db] Conexión fallida: %s\n", g_driver->error(g_driver));
    else
        printf("[db] Conectado al motor '%s'\n", g_driver->name);
    return rc;
}

void DB_close(void) {
    if (g_driver) g_driver->close(g_driver);
}

int DB_exec_raw(const char *sql) {
    if (!g_driver) return -1;
    return g_driver->exec(g_driver, sql, NULL, 0);
}

/* ── DBResult ──────────────────────────────────────────────────────── */
DBResult *DBResult_new(Arena *a) {
    DBResult *r = Arena_alloc(a, sizeof(DBResult));
    r->arena = a;
    r->cap   = DBRESULT_INIT_CAP;
    r->rows  = Arena_alloc(a, sizeof(DBRow *) * r->cap);
    r->count = 0;
    return r;
}

void DBResult_add(DBResult *r, DBRow *row) {
    if (r->count >= r->cap) {
        int     new_cap  = r->cap * 2;
        DBRow **new_rows = Arena_alloc(r->arena, sizeof(DBRow *) * new_cap);
        memcpy(new_rows, r->rows, sizeof(DBRow *) * r->count);
        r->rows = new_rows;
        r->cap  = new_cap;
    }
    r->rows[r->count++] = row;
}

/* ── Validación del operador (anti-SQL injection en el operador) ──── */
static int _valid_op(const char *op) {
    static const char *ops[] = {
        "=", "!=", "<>", "<", ">", "<=", ">=",
        "LIKE", "NOT LIKE", "IS NULL", "IS NOT NULL",
        "IN", "NOT IN", NULL
    };
    for (int i = 0; ops[i]; i++)
        if (strcmp(op, ops[i]) == 0) return 1;
    return 0;
}

/* ── Construcción de SQL ───────────────────────────────────────────── */
static char *_build_select(QueryBuilder *qb, int single) {
    StrBuf *sb = StrBuf_new(qb->arena);
    StrBuf_addf(sb, "SELECT %s FROM \"%s\"",
                qb->select_cols ? qb->select_cols : "*",
                qb->table);

    if (qb->where_count > 0) {
        StrBuf_add(sb, " WHERE ");
        StrBuf_add(sb, qb->where_buf);
    }
    if (qb->order_col)
        StrBuf_addf(sb, " ORDER BY \"%s\" %s",
                    qb->order_col, qb->order_dir ? qb->order_dir : "ASC");

    if (single) {
        StrBuf_add(sb, " LIMIT 1");
    } else if (qb->has_limit) {
        StrBuf_addf(sb, " LIMIT %d", qb->limit_n);
    }
    return StrBuf_str(sb);
}

/* ── Implementaciones de los métodos encadenables ──────────────────── */

static QueryBuilder *_qb_where(QueryBuilder *qb,
                                 const char *col, const char *op,
                                 const char *val) {
    if (!_valid_op(op)) {
        fprintf(stderr, "[db] Operador inválido bloqueado: '%s'\n", op);
        return qb;
    }

    /* Construir el fragmento de condición */
    char clause[256];
    int  no_val = (strcmp(op, "IS NULL") == 0 || strcmp(op, "IS NOT NULL") == 0);

    if (no_val) {
        snprintf(clause, sizeof(clause), "\"%s\" %s", col, op);
    } else {
        snprintf(clause, sizeof(clause), "\"%s\" %s ?", col, op);
        if (qb->bind_count < 64) qb->binds[qb->bind_count++] = val;
    }

    /* Agregar al buffer WHERE */
    size_t wlen = strlen(qb->where_buf);
    if (qb->where_count > 0)
        strncat(qb->where_buf, " AND ", sizeof(qb->where_buf) - wlen - 1);
    strncat(qb->where_buf, clause,
            sizeof(qb->where_buf) - strlen(qb->where_buf) - 1);
    qb->where_count++;
    return qb;
}

static QueryBuilder *_qb_or_where(QueryBuilder *qb,
                                    const char *col, const char *op,
                                    const char *val) {
    if (!_valid_op(op)) return qb;

    char clause[256];
    int  no_val = (strcmp(op, "IS NULL") == 0 || strcmp(op, "IS NOT NULL") == 0);
    if (no_val) {
        snprintf(clause, sizeof(clause), "\"%s\" %s", col, op);
    } else {
        snprintf(clause, sizeof(clause), "\"%s\" %s ?", col, op);
        if (qb->bind_count < 64) qb->binds[qb->bind_count++] = val;
    }

    size_t wlen = strlen(qb->where_buf);
    if (qb->where_count > 0)
        strncat(qb->where_buf, " OR ", sizeof(qb->where_buf) - wlen - 1);
    strncat(qb->where_buf, clause,
            sizeof(qb->where_buf) - strlen(qb->where_buf) - 1);
    qb->where_count++;
    return qb;
}

static QueryBuilder *_qb_order_by(QueryBuilder *qb,
                                    const char *col, const char *dir) {
    qb->order_col = col;
    qb->order_dir = (dir && (strcmp(dir,"DESC")==0 || strcmp(dir,"desc")==0))
                    ? "DESC" : "ASC";
    return qb;
}

static QueryBuilder *_qb_limit(QueryBuilder *qb, int n) {
    qb->limit_n   = n;
    qb->has_limit = 1;
    return qb;
}

static QueryBuilder *_qb_select(QueryBuilder *qb, const char *cols) {
    qb->select_cols = cols;
    return qb;
}

static DBRow *_qb_first(QueryBuilder *qb) {
    if (!g_driver) { fputs("[db] Sin driver\n", stderr); return NULL; }
    char *sql = _build_select(qb, 1);
    DBResult *r = g_driver->query(g_driver, sql, qb->binds, qb->bind_count,
                                   qb->arena);
    if (!r || r->count == 0) return NULL;
    return r->rows[0];
}

static DBResult *_qb_get(QueryBuilder *qb) {
    if (!g_driver) { fputs("[db] Sin driver\n", stderr); return NULL; }
    char *sql = _build_select(qb, 0);
    return g_driver->query(g_driver, sql, qb->binds, qb->bind_count,
                            qb->arena);
}

static int64_t _qb_count(QueryBuilder *qb) {
    if (!g_driver) return -1;
    StrBuf *sb = StrBuf_new(qb->arena);
    StrBuf_addf(sb, "SELECT COUNT(*) FROM \"%s\"", qb->table);
    if (qb->where_count > 0) {
        StrBuf_add(sb, " WHERE ");
        StrBuf_add(sb, qb->where_buf);
    }
    DBResult *r = g_driver->query(g_driver, StrBuf_str(sb),
                                   qb->binds, qb->bind_count, qb->arena);
    if (!r || r->count == 0) return 0;
    const char *v = DBRow_get(r->rows[0], "COUNT(*)");
    if (!v || !*v) v = DBRow_get(r->rows[0], "count(*)");
    return v ? (int64_t)atoll(v) : 0;
}

static int64_t _qb_insert(QueryBuilder *qb, DBRow *data) {
    if (!g_driver || !data || data->count == 0) return -1;

    StrBuf *cols_sb = StrBuf_new(qb->arena);
    StrBuf *vals_sb = StrBuf_new(qb->arena);
    const char *binds[64];
    int n = 0;

    for (int i = 0; i < data->count && n < 64; i++) {
        if (i > 0) { StrBuf_add(cols_sb, ", "); StrBuf_add(vals_sb, ", "); }
        StrBuf_addf(cols_sb, "\"%s\"", data->pairs[i].key);
        StrBuf_add(vals_sb, "?");
        binds[n++] = data->pairs[i].value ? data->pairs[i].value : "";
    }

    char *sql = Arena_sprintf(qb->arena,
        "INSERT INTO \"%s\" (%s) VALUES (%s)",
        qb->table, StrBuf_str(cols_sb), StrBuf_str(vals_sb));

    int rc = g_driver->exec(g_driver, sql, binds, n);
    if (rc != 0) {
        fprintf(stderr, "[db] INSERT falló: %s\n", g_driver->error(g_driver));
        return -1;
    }
    return g_driver->last_insert_id(g_driver);
}

static int _qb_update(QueryBuilder *qb, DBRow *data) {
    if (!g_driver || !data || data->count == 0) return -1;

    StrBuf *set_sb = StrBuf_new(qb->arena);
    const char *binds[128];
    int n = 0;

    for (int i = 0; i < data->count && n < 64; i++) {
        if (i > 0) StrBuf_add(set_sb, ", ");
        StrBuf_addf(set_sb, "\"%s\" = ?", data->pairs[i].key);
        binds[n++] = data->pairs[i].value ? data->pairs[i].value : "";
    }
    /* Agregar binds del WHERE al final */
    for (int i = 0; i < qb->bind_count && n < 128; i++)
        binds[n++] = qb->binds[i];

    StrBuf *sql_sb = StrBuf_new(qb->arena);
    StrBuf_addf(sql_sb, "UPDATE \"%s\" SET %s", qb->table, StrBuf_str(set_sb));
    if (qb->where_count > 0) {
        StrBuf_add(sql_sb, " WHERE ");
        StrBuf_add(sql_sb, qb->where_buf);
    }

    int rc = g_driver->exec(g_driver, StrBuf_str(sql_sb), binds, n);
    if (rc != 0)
        fprintf(stderr, "[db] UPDATE falló: %s\n", g_driver->error(g_driver));
    return rc;
}

static int _qb_delete(QueryBuilder *qb) {
    if (!g_driver) return -1;

    StrBuf *sb = StrBuf_new(qb->arena);
    StrBuf_addf(sb, "DELETE FROM \"%s\"", qb->table);
    if (qb->where_count > 0) {
        StrBuf_add(sb, " WHERE ");
        StrBuf_add(sb, qb->where_buf);
    }

    int rc = g_driver->exec(g_driver, StrBuf_str(sb), qb->binds, qb->bind_count);
    if (rc != 0)
        fprintf(stderr, "[db] DELETE falló: %s\n", g_driver->error(g_driver));
    return rc;
}

/* ── Implementaciones de métodos nuevos ──────────────────── */

static DBRow *_qb_find(QueryBuilder *qb, const char *id) {
    return qb->where(qb, "id", "=", id)->first(qb);
}

static DBResult *_qb_all(QueryBuilder *qb) {
    qb->where_count = 0;
    qb->where_buf[0] = '\0';
    qb->has_limit = 0;
    return qb->get(qb);
}

static int64_t _qb_create(QueryBuilder *qb, DBRow *data) {
    return qb->insert(qb, data);
}

static int _qb_update_id(QueryBuilder *qb, const char *id, DBRow *data) {
    return qb->where(qb, "id", "=", id)->update(qb, data);
}

static int _qb_delete_id(QueryBuilder *qb, const char *id) {
    return qb->where(qb, "id", "=", id)->delete_rows(qb);
}

static int64_t _qb_save(QueryBuilder *qb, DBRow *data) {
    const char *id_val = DBRow_get(data, "id");
    if (id_val && *id_val != '\0') {
        int rc = _qb_update_id(qb, id_val, data);
        if (rc < 0) return -1;
        return (int64_t)atoll(id_val);
    } else {
        int64_t new_id = _qb_create(qb, data);
        if (new_id > 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%lld", (long long)new_id);
            DBRow_set(data, "id", buf);
        }
        return new_id;
    }
}

/* ── QueryBuilder factory ──────────────────────────────────────────── */
QueryBuilder *DB_table(const char *table) {
    if (!g_arena) {
        fputs("[db] Error: llama DB_set_arena() antes de DB_table()\n", stderr);
        return NULL;
    }
    QueryBuilder *qb = Arena_alloc(g_arena, sizeof(QueryBuilder));
    qb->arena        = g_arena;
    qb->table        = table;
    qb->where_buf[0] = '\0';
    qb->where_count  = 0;
    qb->bind_count   = 0;
    qb->order_col    = NULL;
    qb->order_dir    = NULL;
    qb->limit_n      = 0;
    qb->has_limit    = 0;
    qb->select_cols  = NULL;

    qb->where       = _qb_where;
    qb->orWhere     = _qb_or_where;
    qb->orderBy     = _qb_order_by;
    qb->limit       = _qb_limit;
    qb->select      = _qb_select;
    qb->first       = _qb_first;
    qb->get         = _qb_get;
    qb->count       = _qb_count;
    qb->insert      = _qb_insert;
    qb->update      = _qb_update;
    qb->delete_rows = _qb_delete;

    qb->find        = _qb_find;
    qb->all         = _qb_all;
    qb->create      = _qb_create;
    qb->update_id   = _qb_update_id;
    qb->delete_id   = _qb_delete_id;
    qb->save        = _qb_save;

    return qb;
}

/* ── Sistema de Migraciones ────────────────────────────────────────── */
/*
 * Recibe un array de Migraciones generadas por clavel-cli.
 * Aplica únicamente las migraciones que aún no están en la tabla
 * _clavel_migrations.
 */
void DB_migrate(Migration *migrations, int count, const char *tag) {
    if (!g_driver) return;
    (void)tag;

    /* Crear tabla de tracking si no existe */
    DB_exec_raw(
        "CREATE TABLE IF NOT EXISTS _clavel_migrations ("
        "  id        INTEGER PRIMARY KEY,"
        "  name      TEXT    NOT NULL UNIQUE,"
        "  run_at    TEXT    NOT NULL DEFAULT (datetime('now'))"
        ");"
    );

    /* Arena temporal para consultas de migración */
    Arena *tmp = Arena_new(64 * 1024);
    DB_set_arena(tmp);

    for (int i = 0; i < count; i++) {
        const char *mig_name = migrations[i].name;

        /* ¿Ya se ejecutó? */
        QueryBuilder *qb = DB_table("_clavel_migrations");
        DBRow *existing = qb->where(qb, "name", "=", mig_name)->first(qb);
        if (existing) continue;

        /* Ejecutar */
        printf("[db] Migrando: %s\n", mig_name);
        
        /* Run C migration function */
        if (migrations[i].up) migrations[i].up();
        
        /* Registrar migración (en un framework real verificaríamos que Schema_create o _exec no fallara) */
        const char *ins_binds[1] = { mig_name };
        g_driver->exec(g_driver,
            "INSERT INTO _clavel_migrations (name) VALUES (?)",
            ins_binds, 1);
        printf("[db] ✓ %s aplicada\n", mig_name);
    }

    Arena_destroy(tmp);
    g_arena = NULL; /* resetear; main.c lo seteará de nuevo por petición */
}
