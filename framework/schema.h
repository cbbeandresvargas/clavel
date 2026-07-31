/**
 * ClaVel Framework — Schema Builder
 *
 * Escribe las migraciones en C, agnósticas de motor de base de datos.
 */
#ifndef CLAVEL_SCHEMA_H
#define CLAVEL_SCHEMA_H

#include "arena.h"

typedef struct SchemaColumn SchemaColumn;
struct SchemaColumn {
    const char *name;
    const char *type;
    int is_primary;
    int is_unique;
    int is_nullable;
    const char *default_val;
    SchemaColumn *next;
};

typedef struct {
    const char   *name;
    SchemaColumn *columns;
    SchemaColumn *last_col;
    Arena        *arena;
} SchemaTable;

/* ── Tipos de columnas ─────────────────────────────────────────────── */
SchemaColumn *Table_id(SchemaTable *t);
SchemaColumn *Table_string(SchemaTable *t, const char *name);
SchemaColumn *Table_text(SchemaTable *t, const char *name);
SchemaColumn *Table_integer(SchemaTable *t, const char *name);
SchemaColumn *Table_decimal(SchemaTable *t, const char *name, int m, int d);
void          Table_timestamps(SchemaTable *t);

/* ── Modificadores (encadenables) ──────────────────────────────────── */
SchemaColumn *Col_nullable(SchemaColumn *c);
SchemaColumn *Col_unique(SchemaColumn *c);
SchemaColumn *Col_default(SchemaColumn *c, const char *val);

/* ── Acciones principales ──────────────────────────────────────────── */
void Schema_create(const char *table, void (*blueprint)(SchemaTable *t));
void Schema_dropIfExists(const char *table);

#endif /* CLAVEL_SCHEMA_H */
