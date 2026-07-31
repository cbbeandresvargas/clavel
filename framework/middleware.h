/**
 * ClaVel Framework — Middlewares
 * Interceptores que se ejecutan ANTES del controlador.
 * Cada middleware puede modificar req/res o cortocircuitar la cadena.
 *
 * Uso:
 *   Middleware_use(Middleware_logger);
 *   Middleware_use(MiAuth_middleware);
 */
#ifndef CLAVEL_MIDDLEWARE_H
#define CLAVEL_MIDDLEWARE_H

#include "http.h"

#define CLAVEL_MAX_MIDDLEWARES 16

typedef void (*MiddlewareFn)(Request *req, Response *res);

typedef struct MiddlewareStack {
    MiddlewareFn fns[CLAVEL_MAX_MIDDLEWARES];
    int          count;
} MiddlewareStack;

extern MiddlewareStack g_middleware;

void Middleware_use(MiddlewareFn fn);

/* Ejecuta todos los middlewares registrados */
void Middleware_run(Request *req, Response *res);

/* ── Middlewares incluidos ─────────────────────────────────────────── */
/* Logger: imprime método + path + status en la consola */
void Middleware_logger(Request *req, Response *res);

/* CORS básico: añade headers para desarrollo */
void Middleware_cors(Request *req, Response *res);

/* CSRF: Protege contra peticiones POST/PUT/DELETE forjadas */
void Middleware_csrf(Request *req, Response *res);

#endif /* CLAVEL_MIDDLEWARE_H */
