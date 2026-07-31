#include <string.h>
#include <stdio.h>
#include "view.h"

/* ── Registro de vistas ──────────────────────────────────────────────── */
typedef struct ViewEntry {
    const char *name;
    ViewFn      fn;
} ViewEntry;

static ViewEntry _registry[CLAVEL_MAX_VIEWS];
static int       _registry_count = 0;

void View_register(const char *name, ViewFn fn) {
    if (_registry_count >= CLAVEL_MAX_VIEWS) {
        fprintf(stderr, "[clavel] Views: límite de %d vistas alcanzado.\n",
                CLAVEL_MAX_VIEWS);
        return;
    }
    _registry[_registry_count].name = name;
    _registry[_registry_count].fn   = fn;
    _registry_count++;
}

/* ── Render ─────────────────────────────────────────────────────────── */
void View_render(Request *req, Response *res, const char *view_name, TemplateData *data) {
    for (int i = 0; i < _registry_count; i++) {
        if (strcmp(_registry[i].name, view_name) == 0) {
            /* Inject CSRF token automatically */
            const char *csrf = Session_get(req, "csrf_token");
            if (csrf) {
                TemplateData_set(data, "csrf_token", csrf);
            }

            /* Inject all flash sessions */
            Request_cookie(req, "__dummy__"); /* force parse cookies */
            for (int k = 0; k < req->cookie_count; k++) {
                if (strncmp(req->cookies[k].key, "flash_", 6) == 0) {
                    const char *flash_key = req->cookies[k].key + 6;
                    const char *flash_val = Session_get_flash(req, res, flash_key);
                    if (flash_val) {
                        TemplateData_set(data, req->cookies[k].key, flash_val); /* Set as flash_error_name */
                        TemplateData_set(data, flash_key, flash_val);           /* Set as error_name */
                    }
                }
            }
            
            char *html = _registry[i].fn(res->arena, data);
            Response_html(res, 200, html);
            return;
        }
    }
    /* Vista no encontrada */
    char buf[256];
    snprintf(buf, sizeof(buf),
        "<h1>Vista no encontrada: <code>%s</code></h1>", view_name);
    Response_html(res, 500, buf);
}

void View_render_partial(Request *req, Response *res, const char *view_name, TemplateData *data) {
    if (!data) data = TemplateData_new(req->arena);
    TemplateData_set(data, "__is_partial", "1");
    View_render(req, res, view_name, data);
}

/* ── TemplateData ────────────────────────────────────────────────────── */
TemplateData *TemplateData_new(Arena *arena) {
    TemplateData *d = (TemplateData *)Arena_alloc(arena, sizeof(TemplateData));
    d->count = 0;
    d->arena = arena;
    return d;
}

void TemplateData_set(TemplateData *d, const char *key, const char *value) {
    /* Actualizar si ya existe */
    for (int i = 0; i < d->count; i++) {
        if (strcmp(d->pairs[i].key, key) == 0) {
            d->pairs[i].value = value;
            return;
        }
    }
    if (d->count < CLAVEL_MAX_TD_PAIRS) {
        d->pairs[d->count].key   = key;
        d->pairs[d->count].value = value;
        d->count++;
    }
}

const char *TemplateData_get(TemplateData *d, const char *key) {
    if (!d) return "";
    for (int i = 0; i < d->count; i++)
        if (strcmp(d->pairs[i].key, key) == 0)
            return d->pairs[i].value ? d->pairs[i].value : "";
    return "";
}

int TemplateData_truthy(TemplateData *d, const char *key) {
    const char *v = TemplateData_get(d, key);
    return v && *v && strcmp(v, "0") != 0 && strcmp(v, "false") != 0;
}
