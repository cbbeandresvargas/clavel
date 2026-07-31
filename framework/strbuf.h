/**
 * ClaVel Framework — String Builder (header-only)
 * Construye strings grandes de forma incremental usando el arena allocator.
 * Usado internamente por las vistas compiladas.
 */
#ifndef CLAVEL_STRBUF_H
#define CLAVEL_STRBUF_H

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "arena.h"

typedef struct StrBuf {
    Arena  *arena;
    char   *data;
    size_t  len;
    size_t  cap;
} StrBuf;

static inline StrBuf *StrBuf_new(Arena *a) {
    StrBuf *sb  = (StrBuf *)Arena_alloc(a, sizeof(StrBuf));
    sb->arena   = a;
    sb->cap     = 8192;
    sb->data    = (char *)Arena_alloc(a, sb->cap);
    sb->len     = 0;
    sb->data[0] = '\0';
    return sb;
}

static inline void StrBuf_add(StrBuf *sb, const char *s) {
    if (!s) return;
    size_t slen = strlen(s);
    if (slen == 0) return;
    /* Crecer si es necesario — el bloque anterior queda "desperdiciado"
     * en el arena pero se libera junto con él al reset(). */
    if (sb->len + slen + 1 > sb->cap) {
        size_t new_cap = sb->cap * 2;
        while (new_cap < sb->len + slen + 1) new_cap *= 2;
        char *new_data = (char *)Arena_alloc(sb->arena, new_cap);
        memcpy(new_data, sb->data, sb->len);
        sb->data = new_data;
        sb->cap  = new_cap;
    }
    memcpy(sb->data + sb->len, s, slen);
    sb->len            += slen;
    sb->data[sb->len]   = '\0';
}

static inline void StrBuf_addf(StrBuf *sb, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char *tmp = Arena_vsprintf(sb->arena, fmt, args);
    va_end(args);
    StrBuf_add(sb, tmp);
}

static inline char *StrBuf_str(StrBuf *sb) {
    return sb->data;
}

#endif /* CLAVEL_STRBUF_H */
