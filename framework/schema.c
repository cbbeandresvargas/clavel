#include "schema.h"
#include "db_driver.h"
#include <stdio.h>
#include <stdlib.h>

extern DBDriver *g_driver; // Definido en db.c
extern Arena    *g_arena;  // Definido en db.c

/* ── Utilidades internas ───────────────────────────────────────────── */
static SchemaColumn *_add_col(SchemaTable *t, const char *name, const char *type) {
    SchemaColumn *c = Arena_alloc(t->arena, sizeof(SchemaColumn));
    c->name        = name;
    c->type        = type;
    c->is_primary  = 0;
    c->is_unique   = 0;
    c->is_nullable = 0;
    c->default_val = NULL;
    c->next        = NULL;

    if (!t->columns) {
        t->columns = c;
        t->last_col = c;
    } else {
        t->last_col->next = c;
        t->last_col = c;
    }
    return c;
}

/* ── Tipos de columnas ─────────────────────────────────────────────── */
SchemaColumn *Table_id(SchemaTable *t) {
    SchemaColumn *c = _add_col(t, "id", "id");
    c->is_primary = 1;
    return c;
}

SchemaColumn *Table_string(SchemaTable *t, const char *name) {
    return _add_col(t, name, "string");
}

SchemaColumn *Table_text(SchemaTable *t, const char *name) {
    return _add_col(t, name, "text");
}

SchemaColumn *Table_integer(SchemaTable *t, const char *name) {
    return _add_col(t, name, "integer");
}

SchemaColumn *Table_decimal(SchemaTable *t, const char *name, int m, int d) {
    (void)m; (void)d; // En un framework real se guardarían m y d
    return _add_col(t, name, "decimal");
}

void Table_timestamps(SchemaTable *t) {
    SchemaColumn *created = _add_col(t, "created_at", "timestamp");
    Col_default(created, "CURRENT_TIMESTAMP");
    SchemaColumn *updated = _add_col(t, "updated_at", "timestamp");
    Col_default(updated, "CURRENT_TIMESTAMP");
}

/* ── Modificadores ─────────────────────────────────────────────────── */
SchemaColumn *Col_nullable(SchemaColumn *c) {
    c->is_nullable = 1;
    return c;
}

SchemaColumn *Col_unique(SchemaColumn *c) {
    c->is_unique = 1;
    return c;
}

SchemaColumn *Col_default(SchemaColumn *c, const char *val) {
    c->default_val = val;
    return c;
}

/* ── Acciones principales ──────────────────────────────────────────── */
void Schema_create(const char *table, void (*blueprint)(SchemaTable *t)) {
    if (!g_driver || !g_driver->schema_create) {
        fprintf(stderr, "[schema] Error: driver no soporta schema_create\n");
        return;
    }

    Arena *arena = Arena_new(8192);
    SchemaTable t = {
        .name = table,
        .columns = NULL,
        .last_col = NULL,
        .arena = arena
    };

    blueprint(&t);

    if (g_driver->schema_create(g_driver, table, t.columns) == 0) {
        printf("[schema] Tabla %s creada (o verificada).\n", table);
    } else {
        fprintf(stderr, "[schema] Error al crear %s: %s\n", table, g_driver->error(g_driver));
    }

    Arena_destroy(arena);
}

void Schema_dropIfExists(const char *table) {
    if (!g_driver || !g_driver->schema_drop) return;
    if (g_driver->schema_drop(g_driver, table) == 0) {
        printf("[schema] Tabla %s eliminada.\n", table);
    }
}
