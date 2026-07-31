/**
 * ClaVel Framework — Sistema de Vistas (Blade-style)
 *
 * Las vistas se compilan en tiempo de build por `clavel-cli` y se
 * registran en Views_init() (definida en build/views.h).
 *
 * Uso en controladores:
 *   TemplateData *d = TemplateData_new(req->arena);
 *   TemplateData_set(d, "titulo", "Hola Mundo");
 *   View_render(req, res, "home.index", d);
 */
#ifndef CLAVEL_VIEW_H
#define CLAVEL_VIEW_H

#include "arena.h"
#include "strbuf.h"
#include "http.h"

#define CLAVEL_MAX_VIEWS    64
#define CLAVEL_MAX_TD_PAIRS 64

/* ── TemplateData — diccionario string→string para las vistas ──────── */
typedef struct TemplateData {
    KeyValue pairs[CLAVEL_MAX_TD_PAIRS];
    int      count;
    Arena   *arena;
} TemplateData;

TemplateData *TemplateData_new(Arena *arena);
void          TemplateData_set(TemplateData *d, const char *key,
                                const char *value);
const char   *TemplateData_get(TemplateData *d, const char *key);
int           TemplateData_truthy(TemplateData *d, const char *key);

/* ── Escapado HTML (anti-XSS) ─────────────────────────────────────── */
static inline const char *_html_safe(Arena *a, const char *s) {
    if (!s || !*s) return "";
    StrBuf *sb = StrBuf_new(a);
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '&':  StrBuf_add(sb, "&amp;");  break;
            case '<':  StrBuf_add(sb, "&lt;");   break;
            case '>':  StrBuf_add(sb, "&gt;");   break;
            case '"':  StrBuf_add(sb, "&quot;"); break;
            case '\'': StrBuf_add(sb, "&#39;");  break;
            default: {
                char c[2] = { *p, '\0' };
                StrBuf_add(sb, c);
            }
        }
    }
    return StrBuf_str(sb);
}

/* ── ViewFn — firma de función de vista compilada ─────────────────── */
typedef char *(*ViewFn)(Arena *arena, TemplateData *data);

/* ── Registro global ──────────────────────────────────────────────── */
void View_register(const char *name, ViewFn fn);

/* Definida en build/views.h — registra todas las vistas compiladas.
 * No declarar aquí para evitar conflicto con la definición static inline. */

/* ── Render ──────────────────────────────────────────────────────── */
void View_render(Request *req, Response *res, const char *view_name, TemplateData *data);
void View_render_partial(Request *req, Response *res, const char *view_name, TemplateData *data);

#endif /* CLAVEL_VIEW_H */
