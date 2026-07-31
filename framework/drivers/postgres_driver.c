#ifdef CLAVEL_DRIVER_POSTGRES

#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "postgres_driver.h"
#include "../db.h"
#include "../schema.h"
#include "../strbuf.h"

typedef struct {
    PGconn *conn;
    char    last_error[1024];
    int64_t last_id;
} PGPriv;

static int _pg_connect(DBDriver *d, const char *dsn) {
    PGPriv *p = (PGPriv *)d->priv;
    p->conn = PQconnectdb(dsn);
    if (PQstatus(p->conn) != CONNECTION_OK) {
        snprintf(p->last_error, sizeof(p->last_error),
                 "%s", PQerrorMessage(p->conn));
        PQfinish(p->conn);
        p->conn = NULL;
        return -1;
    }
    return 0;
}

static void _pg_close(DBDriver *d) {
    PGPriv *p = (PGPriv *)d->priv;
    if (p->conn) { PQfinish(p->conn); p->conn = NULL; }
}

static int _pg_exec(DBDriver *d, const char *sql,
                     const char **params, int n) {
    PGPriv   *p   = (PGPriv *)d->priv;
    PGresult *res = PQexecParams(p->conn, sql, n, NULL,
                                  params, NULL, NULL, 0);
    ExecStatusType st = PQresultStatus(res);
    if (st != PGRES_COMMAND_OK && st != PGRES_TUPLES_OK) {
        snprintf(p->last_error, sizeof(p->last_error),
                 "%s", PQerrorMessage(p->conn));
        PQclear(res);
        return -1;
    }
    /* Obtener last insert id si el SQL lo retorna */
    const char *oid_str = PQcmdTuples(res);
    p->last_id = oid_str ? (int64_t)atoll(oid_str) : 0;
    PQclear(res);
    return 0;
}

static DBResult *_pg_query(DBDriver *d, const char *sql,
                             const char **params, int n, Arena *a) {
    PGPriv   *p   = (PGPriv *)d->priv;
    PGresult *res = PQexecParams(p->conn, sql, n, NULL,
                                  params, NULL, NULL, 0);
    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        snprintf(p->last_error, sizeof(p->last_error),
                 "%s", PQerrorMessage(p->conn));
        PQclear(res);
        return NULL;
    }
    DBResult *result  = DBResult_new(a);
    int nrows = PQntuples(res);
    int ncols = PQnfields(res);

    for (int r = 0; r < nrows; r++) {
        DBRow *row = DBRow_new(a);
        for (int c = 0; c < ncols; c++) {
            const char *name = Arena_strdup(a, PQfname(res, c));
            const char *val  = PQgetisnull(res, r, c)
                               ? "" : Arena_strdup(a, PQgetvalue(res, r, c));
            DBRow_set(row, name, val);
        }
        DBResult_add(result, row);
    }
    PQclear(res);
    return result;
}

/* PostgreSQL: usa RETURNING id para obtener el last ID */
static int64_t  _pg_last_id(DBDriver *d) { return ((PGPriv *)d->priv)->last_id; }
static const char *_pg_error(DBDriver *d) { return ((PGPriv *)d->priv)->last_error; }

/* ── schema ──────────────────────────────────────────────────────────  */
static int _pg_schema_create(DBDriver *d, const char *table, SchemaColumn *cols) {
    Arena *tmp = Arena_new(8192);
    StrBuf *sb = StrBuf_new(tmp);
    StrBuf_addf(sb, "CREATE TABLE IF NOT EXISTS \"%s\" (\n", table);
    
    SchemaColumn *c = cols;
    while (c) {
        if (c != cols) StrBuf_add(sb, ",\n");
        StrBuf_addf(sb, "  \"%s\" ", c->name);
        
        if (strcmp(c->type, "id") == 0) {
            StrBuf_add(sb, "BIGSERIAL PRIMARY KEY");
        } else if (strcmp(c->type, "string") == 0) {
            StrBuf_add(sb, "VARCHAR(255)");
        } else if (strcmp(c->type, "text") == 0) {
            StrBuf_add(sb, "TEXT");
        } else if (strcmp(c->type, "integer") == 0) {
            StrBuf_add(sb, "INTEGER");
        } else if (strcmp(c->type, "decimal") == 0) {
            StrBuf_add(sb, "DECIMAL");
        } else if (strcmp(c->type, "timestamp") == 0) {
            StrBuf_add(sb, "TIMESTAMP");
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

static int _pg_schema_drop(DBDriver *d, const char *table) {
    char sql[256];
    snprintf(sql, sizeof(sql), "DROP TABLE IF EXISTS \"%s\" CASCADE;", table);
    return d->exec(d, sql, NULL, 0);
}

DBDriver *PostgresDriver_new(void) {
    DBDriver *d = malloc(sizeof(DBDriver));
    PGPriv   *p = calloc(1, sizeof(PGPriv));
    d->name           = "postgres";
    d->connect        = _pg_connect;
    d->close          = _pg_close;
    d->exec           = _pg_exec;
    d->query          = _pg_query;
    d->last_insert_id = _pg_last_id;
    d->error          = _pg_error;
    d->schema_create  = _pg_schema_create;
    d->schema_drop    = _pg_schema_drop;
    d->priv           = p;
    return d;
}

#else /* Sin libpq */

#include "../db_driver.h"
#include <stdio.h>
#include <stdlib.h>

DBDriver *PostgresDriver_new(void) {
    fprintf(stderr,
        "[db] Driver PostgreSQL no compilado.\n"
        "     Instala libpq-dev y recompila con -DCLAVEL_DRIVER_POSTGRES=1.\n");
    return NULL;
}

#endif /* CLAVEL_DRIVER_POSTGRES */
