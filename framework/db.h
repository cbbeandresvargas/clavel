/**
 * ClaVel Framework — ORM y Query Builder
 *
 * API encadenable agnóstica de base de datos, inspirada en Laravel.
 *
 * Uso:
 *   // Consultar uno
 *   DBRow *u = DB_table("users")->where("id","=","1")->first();
 *
 *   // Consultar varios
 *   DBResult *posts = DB_table("posts")
 *                       ->where("active","=","1")
 *                       ->orderBy("created_at","DESC")
 *                       ->limit(10)
 *                       ->get();
 *
 *   // Insertar
 *   DBRow *data = DBRow_new(req->arena);
 *   DBRow_set(data, "nombre", "Laptop");
 *   DBRow_set(data, "precio", "999.99");
 *   int64_t id = DB_table("products")->insert(data);
 *
 *   // Actualizar
 *   DB_table("users")->where("id","=","42")->update(data);
 *
 *   // Eliminar
 *   DB_table("users")->where("id","=","42")->delete_rows();
 *
 *   // Contar
 *   int64_t n = DB_table("users")->where("active","=","1")->count();
 *
 * SEGURIDAD: el Query Builder usa prepared statements internamente.
 * Los valores de los parámetros NUNCA se interpolan en el SQL.
 */
#ifndef CLAVEL_DB_H
#define CLAVEL_DB_H

#include <stdint.h>
#include "arena.h"
#include "view.h"       /* TemplateData sirve como DBRow (mismo struct) */
#include "db_driver.h"

extern DBDriver *g_driver;

/* ═══════════════════════════════════════════════════════════════════════
 * DBRow — fila de base de datos
 * Es exactamente TemplateData: un map string→string.
 * Se puede pasar directamente a View_render() sin conversión.
 * ═══════════════════════════════════════════════════════════════════════ */
typedef TemplateData DBRow;

#define DBRow_new(arena)          TemplateData_new(arena)
#define DBRow_set(row, key, val)  TemplateData_set(row, key, val)
#define DBRow_get(row, key)       TemplateData_get(row, key)

/* ═══════════════════════════════════════════════════════════════════════
 * DBResult — conjunto de filas
 * ═══════════════════════════════════════════════════════════════════════ */
#define DBRESULT_INIT_CAP 16

typedef struct DBResult {
    Arena   *arena;
    DBRow  **rows;
    int      count;
    int      cap;
} DBResult;

DBResult *DBResult_new(Arena *a);
void      DBResult_add(DBResult *r, DBRow *row);

/* ═══════════════════════════════════════════════════════════════════════
 * QueryBuilder — constructor de consultas encadenable
 * ═══════════════════════════════════════════════════════════════════════ */
typedef struct QueryBuilder QueryBuilder;

struct QueryBuilder {
    Arena      *arena;
    const char *table;

    /* Condiciones WHERE (se acumulan con AND) */
    char        where_buf[2048];
    int         where_count;

    /* Valores ligados para prepared statements */
    const char *binds[64];
    int         bind_count;

    /* ORDER BY */
    const char *order_col;
    const char *order_dir;  /* "ASC" | "DESC" */

    /* LIMIT */
    int         limit_n;
    int         has_limit;

    /* Columnas SELECT (NULL = *) */
    const char *select_cols;

    /* ── Métodos encadenables ──────────────────────────────────── */

    /** where("col", "=", "val")  — AND WHERE col = ? */
    QueryBuilder *(*where)(QueryBuilder *qb,
                            const char *col, const char *op,
                            const char *val);

    /** orWhere("col", "=", "val") — OR WHERE col = ? */
    QueryBuilder *(*orWhere)(QueryBuilder *qb,
                              const char *col, const char *op,
                              const char *val);

    /** orderBy("col", "ASC"|"DESC") */
    QueryBuilder *(*orderBy)(QueryBuilder *qb,
                              const char *col, const char *dir);

    /** limit(n) — máximo n filas */
    QueryBuilder *(*limit)(QueryBuilder *qb, int n);

    /** select("col1, col2") — columnas específicas */
    QueryBuilder *(*select)(QueryBuilder *qb, const char *cols);

    /** first() — primera fila o NULL */
    DBRow    *(*first)(QueryBuilder *qb);

    /** get() — todas las filas */
    DBResult *(*get)(QueryBuilder *qb);

    /** find(id) — atajo rápido para buscar por ID principal */
    DBRow    *(*find)(QueryBuilder *qb, const char *id);

    /** all() — atajo rápido para traer toda la tabla sin filtros */
    DBResult *(*all)(QueryBuilder *qb);

    /** count() — número de filas */
    int64_t   (*count)(QueryBuilder *qb);

    /** insert(data) — insertar fila, retorna ID o -1 */
    int64_t   (*insert)(QueryBuilder *qb, DBRow *data);

    /** create(data) — alias semántico de insert */
    int64_t   (*create)(QueryBuilder *qb, DBRow *data);

    /** update(data) — actualizar filas que cumplan WHERE */
    int       (*update)(QueryBuilder *qb, DBRow *data);

    /** update_id(id, data) — atajo para where("id", "=", id)->update(data) */
    int       (*update_id)(QueryBuilder *qb, const char *id, DBRow *data);

    /** delete_rows() — eliminar filas que cumplan WHERE */
    int       (*delete_rows)(QueryBuilder *qb);

    /** delete_id(id) — atajo para where("id", "=", id)->delete_rows() */
    int       (*delete_id)(QueryBuilder *qb, const char *id);

    /** save(registro) — detecta si es nuevo (insert) o existente (update) */
    int64_t   (*save)(QueryBuilder *qb, DBRow *data);
};

/* ═══════════════════════════════════════════════════════════════════════
 * API Global
 * ═══════════════════════════════════════════════════════════════════════ */

/** Registra el driver a usar (SQLiteDriver_new(), etc.) */
void DB_set_driver(DBDriver *driver);

/** Arena para la petición actual (llamar al inicio de cada request) */
void DB_set_arena(Arena *a);

/** Abrir conexión con el DSN del driver activo */
int  DB_connect(const char *dsn);

/** Cerrar conexión */
void DB_close(void);

/** Ejecutar SQL raw (para migraciones) */
int  DB_exec_raw(const char *sql);

/* ── Migraciones en C ──────────────────────────────────────────────── */
typedef struct {
    const char *name;
    void (*up)(void);
    void (*down)(void);
} Migration;

/*
 * DB_migrate() — Recibe un array de migraciones en C y las aplica en orden
 * si no existen ya en _clavel_migrations.
 */
void DB_migrate(Migration *migrations, int count, const char *tag);

/** Punto de entrada del ORM */
QueryBuilder *DB_table(const char *table);

#endif /* CLAVEL_DB_H */
