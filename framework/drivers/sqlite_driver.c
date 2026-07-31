#ifdef CLAVEL_DRIVER_SQLITE

#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sqlite_driver.h"
#include "../view.h"   /* TemplateData = DBRow */
#include "../db.h"
#include "../schema.h"
#include "../strbuf.h"

/* ── Datos privados del driver ─────────────────────────────────────── */
typedef struct {
    sqlite3 *db;
    char     last_error[512];
    int64_t  last_id;
} SQLitePriv;

/* ── connect ─────────────────────────────────────────────────────────
 * DSN: "sqlite:./data/app.db"  |  "sqlite::memory:"
 */
static int _connect(DBDriver *d, const char *dsn) {
    SQLitePriv *p = (SQLitePriv *)d->priv;
    const char *path = (strncmp(dsn, "sqlite:", 7) == 0) ? dsn + 7 : dsn;

    int rc = sqlite3_open(path, &p->db);
    if (rc != SQLITE_OK) {
        snprintf(p->last_error, sizeof(p->last_error),
                 "sqlite3_open(%s): %s", path, sqlite3_errmsg(p->db));
        return -1;
    }
    /* Configuración de rendimiento y seguridad */
    sqlite3_exec(p->db, "PRAGMA journal_mode=WAL;",      NULL, NULL, NULL);
    sqlite3_exec(p->db, "PRAGMA foreign_keys=ON;",       NULL, NULL, NULL);
    sqlite3_exec(p->db, "PRAGMA synchronous=NORMAL;",    NULL, NULL, NULL);
    sqlite3_exec(p->db, "PRAGMA cache_size=-8000;",      NULL, NULL, NULL);
    return 0;
}

/* ── close ───────────────────────────────────────────────────────────  */
static void _close(DBDriver *d) {
    SQLitePriv *p = (SQLitePriv *)d->priv;
    if (p->db) { sqlite3_close(p->db); p->db = NULL; }
}

/* ── exec ────────────────────────────────────────────────────────────
 * Ejecuta SQL sin resultado (INSERT/UPDATE/DELETE/CREATE…)
 * Usa prepared statements: los params[] se ligan como TEXT.
 */
static int _exec(DBDriver *d, const char *sql,
                  const char **params, int n) {
    SQLitePriv   *p    = (SQLitePriv *)d->priv;
    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2(p->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        snprintf(p->last_error, sizeof(p->last_error),
                 "prepare: %s", sqlite3_errmsg(p->db));
        return -1;
    }
    for (int i = 0; i < n; i++)
        sqlite3_bind_text(stmt, i + 1, params[i] ? params[i] : "",
                          -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        snprintf(p->last_error, sizeof(p->last_error),
                 "step: %s", sqlite3_errmsg(p->db));
        sqlite3_finalize(stmt);
        return -1;
    }
    p->last_id = sqlite3_last_insert_rowid(p->db);
    sqlite3_finalize(stmt);
    return 0;
}

/* ── query ───────────────────────────────────────────────────────────
 * Ejecuta SELECT y retorna un DBResult con filas como TemplateData maps.
 * Todos los strings se copian al Arena para que sobrevivan al finalize.
 */
static DBResult *_query(DBDriver *d, const char *sql,
                          const char **params, int n, Arena *a) {
    SQLitePriv   *p    = (SQLitePriv *)d->priv;
    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_prepare_v2(p->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        snprintf(p->last_error, sizeof(p->last_error),
                 "prepare: %s", sqlite3_errmsg(p->db));
        return NULL;
    }
    for (int i = 0; i < n; i++)
        sqlite3_bind_text(stmt, i + 1, params[i] ? params[i] : "",
                          -1, SQLITE_STATIC);

    DBResult *result   = DBResult_new(a);
    int       col_count = sqlite3_column_count(stmt);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        DBRow *row = DBRow_new(a);
        for (int c = 0; c < col_count; c++) {
            /* Copiar nombre y valor al arena — sqlite3 los libera en finalize */
            const char *name = Arena_strdup(a, sqlite3_column_name(stmt, c));
            const char *raw  = (const char *)sqlite3_column_text(stmt, c);
            const char *val  = raw ? Arena_strdup(a, raw) : "";
            DBRow_set(row, name, val);
        }
        DBResult_add(result, row);
    }
    sqlite3_finalize(stmt);
    return result;
}

static int64_t  _last_id(DBDriver *d) { return ((SQLitePriv *)d->priv)->last_id; }
static const char *_error(DBDriver *d) { return ((SQLitePriv *)d->priv)->last_error; }

/* ── schema ──────────────────────────────────────────────────────────  */
static int _schema_create(DBDriver *d, const char *table, SchemaColumn *cols) {
    Arena *tmp = Arena_new(8192);
    StrBuf *sb = StrBuf_new(tmp);
    StrBuf_addf(sb, "CREATE TABLE IF NOT EXISTS \"%s\" (\n", table);
    
    SchemaColumn *c = cols;
    while (c) {
        if (c != cols) StrBuf_add(sb, ",\n");
        StrBuf_addf(sb, "  \"%s\" ", c->name);
        
        if (strcmp(c->type, "id") == 0) {
            StrBuf_add(sb, "INTEGER PRIMARY KEY AUTOINCREMENT");
        } else if (strcmp(c->type, "string") == 0 || strcmp(c->type, "text") == 0 || strcmp(c->type, "timestamp") == 0) {
            StrBuf_add(sb, "TEXT");
        } else if (strcmp(c->type, "integer") == 0) {
            StrBuf_add(sb, "INTEGER");
        } else if (strcmp(c->type, "decimal") == 0) {
            StrBuf_add(sb, "REAL");
        }
        
        if (c->is_unique) StrBuf_add(sb, " UNIQUE");
        if (!c->is_nullable && strcmp(c->type, "id") != 0) StrBuf_add(sb, " NOT NULL");
        if (c->default_val) {
            /* SQLite needs string defaults quoted unless it's a function like CURRENT_TIMESTAMP */
            if (strcmp(c->type, "string") == 0 || strcmp(c->type, "text") == 0)
                StrBuf_addf(sb, " DEFAULT '%s'", c->default_val);
            else if (strcmp(c->type, "timestamp") == 0 && strcmp(c->default_val, "CURRENT_TIMESTAMP") == 0)
                StrBuf_addf(sb, " DEFAULT (datetime('now'))");
            else
                StrBuf_addf(sb, " DEFAULT %s", c->default_val);
        }
        c = c->next;
    }
    StrBuf_add(sb, "\n);");
    
    int rc = d->exec(d, StrBuf_str(sb), NULL, 0);
    Arena_destroy(tmp);
    return rc;
}

static int _schema_drop(DBDriver *d, const char *table) {
    char sql[256];
    snprintf(sql, sizeof(sql), "DROP TABLE IF EXISTS \"%s\";", table);
    return d->exec(d, sql, NULL, 0);
}

/* ── Constructor ─────────────────────────────────────────────────────  */
DBDriver *SQLiteDriver_new(void) {
    DBDriver   *d = malloc(sizeof(DBDriver));
    SQLitePriv *p = calloc(1, sizeof(SQLitePriv));
    if (!d || !p) { free(d); free(p); return NULL; }

    d->name           = "sqlite";
    d->connect        = _connect;
    d->close          = _close;
    d->exec           = _exec;
    d->query          = _query;
    d->last_insert_id = _last_id;
    d->error          = _error;
    d->schema_create  = _schema_create;
    d->schema_drop    = _schema_drop;
    d->priv           = p;
    return d;
}

#else /* CLAVEL_DRIVER_SQLITE no definido */

#include "../db_driver.h"
#include <stdio.h>
#include <stdlib.h>

DBDriver *SQLiteDriver_new(void) {
    fprintf(stderr,
        "[db] Driver SQLite no compilado.\n"
        "     Instala libsqlite3 y añade -DCLAVEL_DRIVER_SQLITE=1 al cmake.\n");
    return NULL;
}

#endif /* CLAVEL_DRIVER_SQLITE */
