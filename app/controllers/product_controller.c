/**
 * ProductController — demuestra el ORM de ClaVel en acción.
 * Aquí se puede apreciar la API encadenable estilo Laravel.
 */
#include <stdio.h>
#include "product_controller.h"
#include "../../framework/db.h"
#include "../../framework/view.h"
#include "../../framework/validator.h"

/* ── index: GET /products ──────────────────────────────────────────── */
void ProductController_index(Request *req, Response *res) {
    DB_set_arena(req->arena);

    /* ORM encadenable — traer todos los productos activos */
    QueryBuilder *qb = DB_table("products");
    qb->where(qb, "active", "=", "1");
    qb->orderBy(qb, "name", "ASC");
    DBResult *products = qb->get(qb);

    TemplateData *d = TemplateData_new(req->arena);
    TemplateData_set(d, "page_title",       "Productos");
    TemplateData_set(d, "meta_description", "Catálogo de productos — ClaVel Demo");

    /* Pasar el conteo como dato de template */
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%d",
             products ? products->count : 0);
    TemplateData_set(d, "product_count", count_str);

    /* Para la demo generamos el HTML de la lista directamente.
     * En una app real, usarías @foreach en la vista cuando se implemente. */
    if (products && products->count > 0) {
        char *list_html = Arena_alloc(req->arena, 8192);
        list_html[0] = '\0';

        for (int i = 0; i < products->count; i++) {
            DBRow *p = products->rows[i];
            const char *slug  = DBRow_get(p, "slug");
            const char *name  = DBRow_get(p, "name");
            const char *price = DBRow_get(p, "price");
            const char *stock = DBRow_get(p, "stock");

            char card[1024];
            snprintf(card, sizeof(card),
                "<a href='/products/%s' class='feature-card' style='text-decoration:none'>"
                "  <div class='feature-icon'>📦</div>"
                "  <h3 class='feature-title'>%s</h3>"
                "  <p class='feature-desc' style='margin-bottom:.75rem'>Stock: %s unidades</p>"
                "  <strong style='color:var(--pink);font-size:1.1rem'>$%s</strong>"
                "</a>",
                slug, name, stock, price);
            strncat(list_html, card,
                    8192 - strlen(list_html) - 1);
        }
        TemplateData_set(d, "products_grid", list_html);
        TemplateData_set(d, "has_products", "1");
    } else {
        TemplateData_set(d, "products_grid", "");
        TemplateData_set(d, "has_products",  "");
    }

    View_render(req, res, "products.index", d);
}

/* ── create: GET /products/create ──────────────────────────────────── */
void ProductController_create(Request *req, Response *res) {
    TemplateData *d = TemplateData_new(req->arena);
    TemplateData_set(d, "page_title", "Crear Producto");
    View_render(req, res, "products.create", d);
}

/* ── store: POST /products ─────────────────────────────────────────── */
void ProductController_store(Request *req, Response *res) {
    /* 1. Validación de los datos recibidos (Fase 9) */
    ValidationResult val = Validator_check(req, (ValidationRule[]){
        {"name", "required|min:3|max:100"},
        {"slug", "required|min:3|unique:products,slug"},
        {"price", "required"},
        {NULL, NULL}
    });

    if (val.fails) {
        /* Falla: Regresar a /products/create con errores inyectados */
        return Response_redirect_back_with_errors(req, res, &val);
    }

    /* 2. Éxito: Procedemos a guardar en BD */
    DB_set_arena(req->arena);
    
    DBRow *data = DBRow_new(req->arena);
    DBRow_set(data, "name", Request_post(req, "name"));
    DBRow_set(data, "slug", Request_post(req, "slug"));
    DBRow_set(data, "price", Request_post(req, "price"));
    DBRow_set(data, "stock", "0");
    DBRow_set(data, "description", "Creado manualmente.");
    
    QueryBuilder *qb = DB_table("products");
    qb->create(qb, data);

    /* Mensaje de éxito en sesión flash (Opcional, pero buena práctica) */
    Session_flash(req, res, "success", "Producto creado exitosamente.");
    
    Response_redirect(res, "/products");
}

/* ── show: GET /products/{slug} ────────────────────────────────────── */
void ProductController_show(Request *req, Response *res) {
    DB_set_arena(req->arena);

    const char *slug = Request_param(req, "slug");
    if (!slug || !*slug) { Response_abort(res, 400); return; }

    /* ORM: buscar por slug con prepared statement (anti SQLi) */
    QueryBuilder *qb = DB_table("products");
    qb->where(qb, "slug", "=", slug);
    qb->where(qb, "active", "=", "1");
    DBRow *product = qb->first(qb);

    if (!product) { Response_abort(res, 404); return; }

    /* El DBRow ES un TemplateData — se pasa directo a View_render */
    TemplateData *d = product;  /* mismo struct, sin conversión */

    /* Agregar metadatos adicionales */
    char meta[256];
    snprintf(meta, sizeof(meta), "Compra %s por $%s — ClaVel Demo",
             DBRow_get(product, "name"), DBRow_get(product, "price"));
    TemplateData_set(d, "page_title",        DBRow_get(product, "name"));
    TemplateData_set(d, "meta_description",  meta);
    TemplateData_set(d, "in_stock",
        atoi(DBRow_get(product, "stock")) > 0 ? "1" : "");

    View_render(req, res, "products.show", d);
}

