#include <stdio.h>
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
    /* El status se loguea DESPUÉS de que el controlador lo establezca.
     * Para eso usamos un "post-hook" simple: almacenamos el tiempo de
     * inicio en el arena y lo imprimimos aquí (pre-controller).
     * Por ahora imprimimos la línea de request. */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[20];
    strftime(ts, sizeof(ts), "%H:%M:%S", t);

    /* El status aún es 0 aquí; se imprimirá 200 como default.
     * Un logger completo requiere un hook post-controlador. */
    printf("[%s] \033[36m%-6s\033[0m %s\n", ts, req->method, req->path);
    (void)res; /* se usa en post-hook futuro */
}

/* ── CORS básico ───────────────────────────────────────────────────── */
void Middleware_cors(Request *req, Response *res) {
    (void)req;
    Response_set_header(res, "Access-Control-Allow-Origin", "*");
    Response_set_header(res, "Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
    Response_set_header(res, "Access-Control-Allow-Headers", "Content-Type,Authorization");
    (void)req;
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
    /* 1. Si no hay token en sesión, lo generamos para futuras vistas (ej. peticiones GET) */
    const char *session_token = Session_get(req, "csrf_token");
    if (!session_token) {
        char new_token[33];
        _generate_random_string(new_token, 32);
        Session_set(req, res, "csrf_token", new_token);
        session_token = Session_get(req, "csrf_token"); /* reload */
    }

    /* 2. Si es peticion de escritura (POST, PUT, DELETE, PATCH), validamos */
    if (strcmp(req->method, "POST") == 0 || strcmp(req->method, "PUT") == 0 ||
        strcmp(req->method, "DELETE") == 0 || strcmp(req->method, "PATCH") == 0) {
        
        const char *token_in_req = NULL;
        
        /* Prioridad 1: header X-CSRF-TOKEN (para APIs JS fetch) */
        token_in_req = Request_header(req, "X-CSRF-TOKEN");
        
        /* Prioridad 2: body form data (para formularios web estándar) */
        if (!token_in_req && req->body) {
            /* Un parser de body super rudimentario para URL encoded */
            char *body_copy = Arena_strdup(req->arena, req->body);
            char *tok = strtok(body_copy, "&");
            while (tok) {
                if (strncmp(tok, "csrf_token=", 11) == 0) {
                    token_in_req = tok + 11;
                    break;
                }
                tok = strtok(NULL, "&");
            }
        }
        
        if (!token_in_req || strcmp(session_token, token_in_req) != 0) {
            /* Token inválido o ausente */
            Response_html(res, 419, "419 Page Expired (CSRF Token Mismatch)");
            return;
        }
    }
}
