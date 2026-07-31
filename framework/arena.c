#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "arena.h"

Arena *Arena_new(size_t cap) {
    Arena *a = malloc(sizeof(Arena));
    if (!a) return NULL;
    a->buf  = malloc(cap);
    if (!a->buf) { free(a); return NULL; }
    a->cap  = cap;
    a->used = 0;
    return a;
}

void *Arena_alloc(Arena *a, size_t size) {
    /* Alinear a 8 bytes para evitar undefined behaviour en accesos */
    size = (size + 7u) & ~7u;
    if (a->used + size > a->cap) {
        /* Crecer automáticamente */
        size_t new_cap = a->cap * 2;
        while (new_cap < a->used + size) new_cap *= 2;
        char *new_buf = realloc(a->buf, new_cap);
        if (!new_buf) return NULL;
        a->buf = new_buf;
        a->cap = new_cap;
    }
    void *ptr = a->buf + a->used;
    a->used += size;
    memset(ptr, 0, size);
    return ptr;
}

char *Arena_strdup(Arena *a, const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    return Arena_strndup(a, s, len);
}

char *Arena_strndup(Arena *a, const char *s, size_t n) {
    if (!s) return NULL;
    char *copy = Arena_alloc(a, n + 1);
    if (!copy) return NULL;
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

char *Arena_vsprintf(Arena *a, const char *fmt, va_list args) {
    va_list args2;
    va_copy(args2, args);
    int len = vsnprintf(NULL, 0, fmt, args2);
    va_end(args2);
    if (len < 0) return NULL;

    char *buf = Arena_alloc(a, (size_t)len + 1);
    if (!buf) return NULL;
    vsnprintf(buf, (size_t)len + 1, fmt, args);
    return buf;
}

char *Arena_sprintf(Arena *a, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *result = Arena_vsprintf(a, fmt, args);
    va_end(args);
    return result;
}

void Arena_reset(Arena *a) {
    a->used = 0;
}

void Arena_destroy(Arena *a) {
    if (!a) return;
    free(a->buf);
    free(a);
}
