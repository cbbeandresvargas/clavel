/**
 * ClaVel Framework — Interfaz abstracta de drivers de base de datos
 *
 * Todo driver (SQLite, PostgreSQL, MySQL…) implementa esta interfaz.
 * El Query Builder llama únicamente a estas funciones, lo que hace el
 * código del usuario 100% agnóstico del motor de base de datos.
 */
#ifndef CLAVEL_DB_DRIVER_H
#define CLAVEL_DB_DRIVER_H

#include <stdint.h>
#include "arena.h"

/* Forward declarations */
struct DBResult;
struct SchemaColumn;

/**
 * DBDriver — tabla virtual de funciones de un driver.
 *
 * Para crear un driver nuevo:
 *   1. Crea un struct con DBDriver como primer miembro (o puntero)
 *   2. Implementa todas las funciones de la vtable
 *   3. Registra el driver con DB_set_driver()
 */
typedef struct DBDriver {
    const char *name;       /* "sqlite", "postgres", "mysql", … */

    /**
     * connect — abrir la conexión con el DSN dado.
     * DSN format:
     *   sqlite:./data/app.db
     *   sqlite::memory:
     *   postgresql://user:pass@host:5432/dbname
     *   mysql://user:pass@host:3306/dbname
     * Retorna 0 en éxito, -1 en error.
     */
    int      (*connect)(struct DBDriver *d, const char *dsn);

    /** close — cerrar la conexión */
    void     (*close)(struct DBDriver *d);

    /**
     * exec — ejecutar SQL sin resultado (INSERT, UPDATE, DELETE, CREATE…)
     * params: array de strings para los ? placeholders
     * n: número de parámetros
     * Retorna 0 en éxito, -1 en error.
     * SEGURIDAD: usa siempre prepared statements internamente.
     */
    int      (*exec)(struct DBDriver *d,
                     const char *sql,
                     const char **params, int n);

    /**
     * query — ejecutar SELECT, retorna un DBResult.
     * La memoria del resultado se aloja en el Arena dado.
     */
    struct DBResult *(*query)(struct DBDriver *d,
                               const char *sql,
                               const char **params, int n,
                               Arena *arena);

    /** last_insert_id — ID de la última fila insertada */
    int64_t  (*last_insert_id)(struct DBDriver *d);

    /** schema_create — Genera y ejecuta el SQL de creación de tabla */
    int      (*schema_create)(struct DBDriver *d, const char *table, struct SchemaColumn *cols);

    /** schema_drop — Genera y ejecuta DROP TABLE */
    int      (*schema_drop)(struct DBDriver *d, const char *table);

    /** error — último mensaje de error */
    const char *(*error)(struct DBDriver *d);

    /** Datos privados del driver (puntero opaco) */
    void *priv;
} DBDriver;

#endif /* CLAVEL_DB_DRIVER_H */
