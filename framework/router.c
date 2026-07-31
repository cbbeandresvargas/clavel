#include <string.h>
#include <stdio.h>
#include "router.h"

#ifdef _WIN32
#define strtok_r strtok_s
#endif

Router g_router = {{0}, 0};

/* ── Registro ────────────────────────────────────────────────────────── */
void Route_any(const char *method, const char *pattern, HandlerFn handler) {
    if (g_router.count >= CLAVEL_MAX_ROUTES) {
        fprintf(stderr, "[clavel] Router: límite de %d rutas alcanzado.\n",
                CLAVEL_MAX_ROUTES);
        return;
    }
    g_router.routes[g_router.count].method  = method;
    g_router.routes[g_router.count].pattern = pattern;
    g_router.routes[g_router.count].handler = handler;
    g_router.count++;
}

void Route_get(const char *p, HandlerFn h)    { Route_any("GET",    p, h); }
void Route_post(const char *p, HandlerFn h)   { Route_any("POST",   p, h); }
void Route_put(const char *p, HandlerFn h)    { Route_any("PUT",    p, h); }
void Route_delete(const char *p, HandlerFn h) { Route_any("DELETE", p, h); }

/* ── Pattern matching ────────────────────────────────────────────────── */
/*
 * Compara patrón con path real. Captura segmentos {param}.
 * Ejemplo: "/products/{id}/reviews/{rid}" vs "/products/42/reviews/7"
 *   → req->params["id"]="42", req->params["rid"]="7"
 */
static int _match(const char *pattern, const char *path, Request *req,
                  int *param_count_out) {
    char pat_buf[512], path_buf[512];
    strncpy(pat_buf,  pattern, sizeof(pat_buf)  - 1);
    strncpy(path_buf, path,    sizeof(path_buf) - 1);
    pat_buf[sizeof(pat_buf)   - 1] = '\0';
    path_buf[sizeof(path_buf) - 1] = '\0';

    /* Eliminar query string del path si la hubiera */
    char *q = strchr(path_buf, '?');
    if (q) *q = '\0';

    char *pat_save  = NULL;
    char *path_save = NULL;
    char *pat_seg   = strtok_r(pat_buf,  "/", &pat_save);
    char *path_seg  = strtok_r(path_buf, "/", &path_save);

    int captured = 0;

    while (pat_seg || path_seg) {
        if (!pat_seg || !path_seg) return 0; /* longitudes distintas */

        if (pat_seg[0] == '{') {
            /* Segmento dinámico */
            size_t klen = strlen(pat_seg);
            if (pat_seg[klen - 1] != '}') return 0;
            char key[64] = {0};
            strncpy(key, pat_seg + 1, klen - 2 < 63 ? klen - 2 : 63);

            if (captured < CLAVEL_MAX_PARAMS) {
                req->params[captured].key   = Arena_strdup(req->arena, key);
                req->params[captured].value = Arena_strdup(req->arena, path_seg);
                captured++;
            }
        } else if (strcmp(pat_seg, path_seg) != 0) {
            return 0;
        }

        pat_seg  = strtok_r(NULL, "/", &pat_save);
        path_seg = strtok_r(NULL, "/", &path_save);
    }

    *param_count_out = captured;
    return 1;
}

/* ── Dispatch ────────────────────────────────────────────────────────── */
int Router_dispatch(Router *router, Request *req, Response *res) {
    for (int i = 0; i < router->count; i++) {
        Route *r = &router->routes[i];
        if (strcmp(r->method, req->method) != 0) continue;

        int captured = 0;
        req->param_count = 0; /* reset antes de cada intento */

        if (_match(r->pattern, req->path, req, &captured)) {
            req->param_count = captured;
            r->handler(req, res);
            return 1;
        }
    }
    return 0;
}
