#ifdef CLAVEL_DRIVER_MYSQL

#include <mysql/mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mysql_driver.h"
#include "../db.h"
#include "../schema.h"
#include "../strbuf.h"

typedef struct {
    MYSQL  *conn;
    char    last_error[1024];
    int64_t last_id;
} MySQLPriv;

/* ── Parsear DSN "mysql://user:pass@host:port/db" ─────────────────── */
static void _parse_dsn(const char *dsn, char *user, char *pass,
                        char *host, int *port, char *db) {
    /* Formato: mysql://user:pass@host:3306/dbname */
    const char *p = dsn;
    if (strncmp(p, "mysql://", 8) == 0) p += 8;

    sscanf(p, "%255[^:]:%255[^@]@%255[^:]:%d/%255s",
           user, pass, host, port, db);
}

static int _my_connect(DBDriver *d, const char *dsn) {
    MySQLPriv *p = (MySQLPriv *)d->priv;
    p->conn = mysql_init(NULL);
    if (!p->conn) return -1;

    char user[256]="root", pass[256]="", host[256]="127.0.0.1", db[256]="";
    int  port = 3306;
    _parse_dsn(dsn, user, pass, host, &port, db);

    if (!mysql_real_connect(p->conn, host, user, pass, db,
                             (unsigned int)port, NULL, 0)) {
        snprintf(p->last_error, sizeof(p->last_error),
                 "%s", mysql_error(p->conn));
        mysql_close(p->conn);
        p->conn = NULL;
        return -1;
    }
    /* UTF-8 por defecto */
    mysql_set_character_set(p->conn, "utf8mb4");
    return 0;
}

static void _my_close(DBDriver *d) {
    MySQLPriv *p = (MySQLPriv *)d->priv;
    if (p->conn) { mysql_close(p->conn); p->conn = NULL; }
}

static int _my_exec(DBDriver *d, const char *sql,
                     const char **params, int n) {
    MySQLPriv    *p    = (MySQLPriv *)d->priv;
    MYSQL_STMT   *stmt = mysql_stmt_init(p->conn);
    if (!stmt) return -1;

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        snprintf(p->last_error, sizeof(p->last_error),
                 "%s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }

    MYSQL_BIND binds[64];
    memset(binds, 0, sizeof(binds));
    unsigned long lens[64];

    for (int i = 0; i < n && i < 64; i++) {
        lens[i] = (unsigned long)strlen(params[i] ? params[i] : "");
        binds[i].buffer_type   = MYSQL_TYPE_STRING;
        binds[i].buffer        = (void *)(params[i] ? params[i] : "");
        binds[i].buffer_length = lens[i];
        binds[i].length        = &lens[i];
    }
    if (n > 0) mysql_stmt_bind_param(stmt, binds);

    int rc = mysql_stmt_execute(stmt);
    if (rc != 0) {
        snprintf(p->last_error, sizeof(p->last_error),
                 "%s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return -1;
    }
    p->last_id = (int64_t)mysql_stmt_insert_id(stmt);
    mysql_stmt_close(stmt);
    return 0;
}

static DBResult *_my_query(DBDriver *d, const char *sql,
                             const char **params, int n, Arena *a) {
    MySQLPriv  *p    = (MySQLPriv *)d->priv;
    MYSQL_STMT *stmt = mysql_stmt_init(p->conn);
    if (!stmt) return NULL;

    if (mysql_stmt_prepare(stmt, sql, (unsigned long)strlen(sql)) != 0) {
        snprintf(p->last_error, sizeof(p->last_error),
                 "%s", mysql_stmt_error(stmt));
        mysql_stmt_close(stmt);
        return NULL;
    }

    MYSQL_BIND param_binds[64];
    memset(param_binds, 0, sizeof(param_binds));
    unsigned long param_lens[64];
    for (int i = 0; i < n && i < 64; i++) {
        param_lens[i] = (unsigned long)strlen(params[i] ? params[i] : "");
        param_binds[i].buffer_type   = MYSQL_TYPE_STRING;
        param_binds[i].buffer        = (void *)(params[i] ? params[i] : "");
        param_binds[i].buffer_length = param_lens[i];
        param_binds[i].length        = &param_lens[i];
    }
    if (n > 0) mysql_stmt_bind_param(stmt, param_binds);
    mysql_stmt_execute(stmt);

    MYSQL_RES *meta = mysql_stmt_result_metadata(stmt);
    if (!meta) { mysql_stmt_close(stmt); return DBResult_new(a); }

    int num_fields = (int)mysql_num_fields(meta);
    MYSQL_FIELD *fields = mysql_fetch_fields(meta);

    /* Resultado bind */
#define MAX_COL_LEN 4096
    char  **col_bufs = Arena_alloc(a, sizeof(char *) * num_fields);
    unsigned long *col_lens = Arena_alloc(a, sizeof(unsigned long) * num_fields);
    my_bool *col_nulls = Arena_alloc(a, sizeof(my_bool) * num_fields);
    MYSQL_BIND *res_binds = Arena_alloc(a, sizeof(MYSQL_BIND) * num_fields);
    memset(res_binds, 0, sizeof(MYSQL_BIND) * num_fields);

    for (int i = 0; i < num_fields; i++) {
        col_bufs[i] = Arena_alloc(a, MAX_COL_LEN);
        res_binds[i].buffer_type   = MYSQL_TYPE_STRING;
        res_binds[i].buffer        = col_bufs[i];
        res_binds[i].buffer_length = MAX_COL_LEN - 1;
        res_binds[i].length        = &col_lens[i];
        res_binds[i].is_null       = &col_nulls[i];
    }
    mysql_stmt_bind_result(stmt, res_binds);
    mysql_stmt_store_result(stmt);

    DBResult *result = DBResult_new(a);
    while (mysql_stmt_fetch(stmt) == 0) {
        DBRow *row = DBRow_new(a);
        for (int i = 0; i < num_fields; i++) {
            const char *name = Arena_strdup(a, fields[i].name);
            const char *val  = col_nulls[i]
                               ? ""
                               : Arena_strndup(a, col_bufs[i], col_lens[i]);
            DBRow_set(row, name, val);
        }
        DBResult_add(result, row);
    }

    mysql_free_result(meta);
    mysql_stmt_close(stmt);
    return result;
}

static int64_t     _my_last_id(DBDriver *d) { return ((MySQLPriv *)d->priv)->last_id; }
static const char *_my_error(DBDriver *d)   { return ((MySQLPriv *)d->priv)->last_error; }

/* ── schema ──────────────────────────────────────────────────────────  */
static int _my_schema_create(DBDriver *d, const char *table, SchemaColumn *cols) {
    Arena *tmp = Arena_new(8192);
    StrBuf *sb = StrBuf_new(tmp);
    StrBuf_addf(sb, "CREATE TABLE IF NOT EXISTS `%s` (\n", table);
    
    SchemaColumn *c = cols;
    while (c) {
        if (c != cols) StrBuf_add(sb, ",\n");
        StrBuf_addf(sb, "  `%s` ", c->name);
        
        if (strcmp(c->type, "id") == 0) {
            StrBuf_add(sb, "BIGINT AUTO_INCREMENT PRIMARY KEY");
        } else if (strcmp(c->type, "string") == 0) {
            StrBuf_add(sb, "VARCHAR(255)");
        } else if (strcmp(c->type, "text") == 0) {
            StrBuf_add(sb, "TEXT");
        } else if (strcmp(c->type, "integer") == 0) {
            StrBuf_add(sb, "INT");
        } else if (strcmp(c->type, "decimal") == 0) {
            StrBuf_add(sb, "DECIMAL(10,2)"); /* Simplified for demo */
        } else if (strcmp(c->type, "timestamp") == 0) {
            StrBuf_add(sb, "DATETIME");
        }
        
        if (c->is_unique) StrBuf_add(sb, " UNIQUE");
        if (!c->is_nullable && strcmp(c->type, "id") != 0) StrBuf_add(sb, " NOT NULL");
        if (c->default_val) {
            if (strcmp(c->type, "string") == 0 || strcmp(c->type, "text") == 0)
                StrBuf_addf(sb, " DEFAULT '%s'", c->default_val);
            else if (strcmp(c->type, "timestamp") == 0 && strcmp(c->default_val, "CURRENT_TIMESTAMP") == 0)
                StrBuf_addf(sb, " DEFAULT CURRENT_TIMESTAMP");
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

static int _my_schema_drop(DBDriver *d, const char *table) {
    char sql[256];
    snprintf(sql, sizeof(sql), "DROP TABLE IF EXISTS `%s`;", table);
    return d->exec(d, sql, NULL, 0);
}

DBDriver *MySQLDriver_new(void) {
    DBDriver  *d = malloc(sizeof(DBDriver));
    MySQLPriv *p = calloc(1, sizeof(MySQLPriv));
    d->name           = "mysql";
    d->connect        = _my_connect;
    d->close          = _my_close;
    d->exec           = _my_exec;
    d->query          = _my_query;
    d->last_insert_id = _my_last_id;
    d->error          = _my_error;
    d->schema_create  = _my_schema_create;
    d->schema_drop    = _my_schema_drop;
    d->priv           = p;
    return d;
}

#else /* Sin libmysqlclient */

#include "../db_driver.h"
#include <stdio.h>
#include <stdlib.h>

DBDriver *MySQLDriver_new(void) {
    fprintf(stderr,
        "[db] Driver MySQL no compilado.\n"
        "     Instala libmysqlclient-dev y recompila con -DCLAVEL_DRIVER_MYSQL=1.\n");
    return NULL;
}

#endif /* CLAVEL_DRIVER_MYSQL */
