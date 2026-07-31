/**
 * ClaVel — Rutas de la aplicación
 *
 * Define aquí todas las rutas de tu app. Esta función es llamada
 * automáticamente por main.c al arrancar el servidor.
 */
#include "../framework/router.h"
#include "../framework/middleware.h"
#include "controllers/home_controller.h"
#include "controllers/product_controller.h"

void register_routes(void) {
    /* ── Middlewares globales ──────────────────────────────────────── */
    Middleware_use(Middleware_logger);
    Middleware_use(Middleware_csrf);

    /* ── Rutas home ───────────────────────────────────────────────── */
    Route_get("/",      HomeController_index);
    Route_get("/about", HomeController_about);
    Route_get("/docs",  HomeController_docs);
    Route_get("/reactive", HomeController_reactive);

    /* ── Rutas de productos (demo ORM Fase 4 y Validación Fase 9) ─────────────────────── */
    Route_get("/products",            ProductController_index);
    Route_get("/products/create",     ProductController_create);
    Route_post("/products",           ProductController_store);
    Route_get("/products/{slug}",     ProductController_show);
}
