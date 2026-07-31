#include <stdlib.h>
#include "home_controller.h"
#include "../../framework/view.h"

void HomeController_index(Request *req, Response *res) {
    TemplateData *d = TemplateData_new(req->arena);
    TemplateData_set(d, "page_title",        "Inicio");
    TemplateData_set(d, "meta_description",
        "ClaVel es un framework MVC batteries-included escrito en C. "
        "Velocidad extrema con una DX similar a Laravel.");
    TemplateData_set(d, "subtitle",
        "Desarrolla con la agilidad de un lenguaje moderno. "
        "Despliega un binario de 3\xc2\xa0MB capaz de procesar "
        "decenas de miles de peticiones por segundo.");

    View_render(req, res, "home.index", d);
}

void HomeController_about(Request *req, Response *res) {
    TemplateData *d = TemplateData_new(req->arena);
    TemplateData_set(d, "page_title",       "Sobre ClaVel");
    TemplateData_set(d, "meta_description",
        "Descubre la historia y el stack técnico de ClaVel, "
        "el framework MVC escrito en C.");

    View_render(req, res, "home.about", d);
}

void HomeController_reactive(Request *req, Response *res) {
    TemplateData *d = TemplateData_new(req->arena);
    /* Increment a counter randomly for demo */
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%d", rand() % 1000);
    TemplateData_set(d, "random_count", count_str);
    
    if (Request_is_partial(req)) {
        View_render_partial(req, res, "home.reactive", d);
    } else {
        View_render(req, res, "home.reactive", d);
    }
}

void HomeController_docs(Request *req, Response *res) {
    TemplateData *d = TemplateData_new(req->arena);
    TemplateData_set(d, "page_title",       "Documentación");
    TemplateData_set(d, "meta_description", "Documentación de ClaVel Framework.");

    /* Vista aún no creada — respuesta temporal */
    (void)d;
    Response_html(res, 200,
        "<!DOCTYPE html><html><head><title>Docs — ClaVel</title>"
        "<link rel='stylesheet' href='/style.css'></head>"
        "<body><div class='page-header'>"
        "<h1>Documentación <span class='gradient-text'>ClaVel</span></h1>"
        "<p>Próximamente... estamos construyendo esto contigo.</p>"
        "</div></body></html>");
}
