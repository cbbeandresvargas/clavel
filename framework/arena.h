/**
 * ClaVel Framework — Arena Allocator
 * Un bloque de memoria pre-asignado por petición HTTP que se libera en un
 * solo paso. Cero malloc/free manuales en el código del usuario.
 */
#ifndef CLAVEL_ARENA_H
#define CLAVEL_ARENA_H

#include <stddef.h>
#include <stdarg.h>

#define ARENA_DEFAULT_SIZE (1024 * 1024) /* 1 MB por petición */

typedef struct Arena {
    char   *buf;
    size_t  cap;
    size_t  used;
} Arena;

Arena *Arena_new(size_t cap);
void  *Arena_alloc(Arena *a, size_t size);
char  *Arena_strdup(Arena *a, const char *s);
char  *Arena_strndup(Arena *a, const char *s, size_t n);
char  *Arena_sprintf(Arena *a, const char *fmt, ...);
char  *Arena_vsprintf(Arena *a, const char *fmt, va_list args);
void   Arena_reset(Arena *a);
void   Arena_destroy(Arena *a);

#endif /* CLAVEL_ARENA_H */
