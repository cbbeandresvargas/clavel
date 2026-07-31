#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "middleware.h"

MiddlewareStack g_middleware = {{0}, 0};

void Middleware_use(MiddlewareFn fn) {
    if (g_middleware.count >= CLAVEL_MAX_MIDDLEWARES) {
        fprintf(stderr, "[clavel] Middleware: límite de %d alcanzado.\n",
                CLAVEL_MAX_MIDDLEWARES);
        return;
    }
    g_middleware.fns[g_middleware.count++] = fn;
}

void Middleware_run(Request *req, Response *res) {
    for (int i = 0; i < g_middleware.count; i++) {
        g_middleware.fns[i](req, res);
    }
}

/* ── Logger ────────────────────────────────────────────────────────── */
void Middleware_logger(Request *req, Response *res) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[20];
    strftime(ts, sizeof(ts), "%H:%M:%S", t);
    printf("[%s] \033[36m%-6s\033[0m %s\n", ts, req->method, req->path);
    (void)res;
}

/* ── CORS básico ───────────────────────────────────────────────────── */
void Middleware_cors(Request *req, Response *res) {
    (void)req;
    Response_set_header(res, "Access-Control-Allow-Origin", "*");
    Response_set_header(res, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    Response_set_header(res, "Access-Control-Allow-Headers", "Content-Type,Authorization,X-CSRF-TOKEN,X-ClaVel-Request");
}

/* ── CSRF ──────────────────────────────────────────────────────────── */
static void _generate_random_string(char *s, int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    for (int i = 0; i < len; ++i) {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    s[len] = 0;
}

void Middleware_csrf(Request *req, Response *res) {
    /* 1. Si no hay token en sesión, generamos uno nuevo */
    const char *session_token = Session_get(req, "csrf_token");
    if (!session_token) {
        char new_token[33];
        _generate_random_string(new_token, 32);
        Session_set(req, res, "csrf_token", new_token);
        /* Session_set escribe en la RESPUESTA, no en la petición.
         * Usamos directamente el valor generado como token de referencia. */
        session_token = Arena_strdup(req->arena, new_token);
    }

    /* 2. Para peticiones de escritura, validamos el token */
    if (strcmp(req->method, "POST") == 0 || strcmp(req->method, "PUT") == 0 ||
        strcmp(req->method, "DELETE") == 0 || strcmp(req->method, "PATCH") == 0) {

        const char *token_in_req = NULL;

        /* Prioridad 1: header X-CSRF-TOKEN (peticiones JS fetch) */
        token_in_req = Request_header(req, "X-CSRF-TOKEN");

        /* Prioridad 2: campo csrf_token del form (ya parseado) */
        if (!token_in_req)
            token_in_req = Request_post(req, "csrf_token");

        if (!token_in_req || strcmp(session_token, token_in_req) != 0) {
            Response_html(res, 419, "419 Page Expired (CSRF Token Mismatch)");
            return;
        }
    }
}
