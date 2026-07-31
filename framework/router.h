/**
 * ClaVel Framework — Router Dinámico
 * Soporta parámetros de ruta {slug}, verbos HTTP y tabla de rutas global.
 *
 * Uso:
 *   Route_get("/productos/{slug}", ProductoController_show);
 *   Route_post("/productos",       ProductoController_store);
 */
#ifndef CLAVEL_ROUTER_H
#define CLAVEL_ROUTER_H

#include "http.h"

#define CLAVEL_MAX_ROUTES 256

typedef void (*HandlerFn)(Request *req, Response *res);

typedef struct Route {
    const char *method;
    const char *pattern;
    HandlerFn   handler;
} Route;

typedef struct Router {
    Route routes[CLAVEL_MAX_ROUTES];
    int   count;
} Router;

/* Router global — poblado en app/routes.c */
extern Router g_router;

/* Registro de rutas */
void Route_get(const char *pattern, HandlerFn handler);
void Route_post(const char *pattern, HandlerFn handler);
void Route_put(const char *pattern, HandlerFn handler);
void Route_delete(const char *pattern, HandlerFn handler);
void Route_any(const char *method, const char *pattern, HandlerFn handler);

/* Despacho: 1 si se encontró ruta, 0 si no */
int Router_dispatch(Router *router, Request *req, Response *res);

#endif /* CLAVEL_ROUTER_H */
