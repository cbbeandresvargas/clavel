#ifndef PRODUCT_CONTROLLER_H
#define PRODUCT_CONTROLLER_H

#include "../../framework/http.h"

/* GET /products           — listado de todos los productos */
void ProductController_index(Request *req, Response *res);

/* GET /products/{slug}    — detalle de un producto */
void ProductController_show(Request *req, Response *res);

/* GET  /products/create   — formulario de creación */
void ProductController_create(Request *req, Response *res);

/* POST /products          — guardar nuevo producto */
void ProductController_store(Request *req, Response *res);

#endif /* PRODUCT_CONTROLLER_H */
