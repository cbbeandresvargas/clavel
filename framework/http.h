/**
 * ClaVel Framework — HTTP Request / Response
 * Abstracciones sobre el protocolo HTTP que el código del usuario manipula.
 * Toda la memoria vive en el Arena de la petición.
 */
#ifndef CLAVEL_HTTP_H
#define CLAVEL_HTTP_H

#include <stddef.h>
#include "arena.h"

#define CLAVEL_MAX_HEADERS 32
#define CLAVEL_MAX_PARAMS  16
#define CLAVEL_MAX_COOKIES 16

/* Clave secreta (debería venir de un .env o config, harcodeada para la demo) */
#define CLAVEL_SECRET_KEY "clavel-super-secret-key-change-me"

/* ── Par clave-valor ──────────────────────────────────────────────────── */
typedef struct {
    const char *key;
    const char *value;
} KeyValue;

/* ── UploadedFile ─────────────────────────────────────────────────────── */
typedef struct {
    const char *filename;
    const char *data;
    size_t      size;
} UploadedFile;

/* ── Request ──────────────────────────────────────────────────────────── */
typedef struct Request {
    Arena    *arena;

    const char *method;          /* "GET", "POST", …                    */
    const char *path;            /* "/productos/mi-slug"                 */
    const char *body;            /* Cuerpo crudo de la petición          */
    size_t      body_len;

    KeyValue headers[CLAVEL_MAX_HEADERS];
    int      header_count;

    KeyValue query[CLAVEL_MAX_PARAMS];  /* ?key=val                      */
    int      query_count;

    KeyValue params[CLAVEL_MAX_PARAMS]; /* Parámetros de ruta {slug}     */
    int      param_count;

    KeyValue post[CLAVEL_MAX_PARAMS];   /* Parámetros de POST            */
    int      post_count;

    KeyValue cookies[CLAVEL_MAX_COOKIES];
    int      cookie_count;

    /* Para que funciones de framework (como cookies) puedan acceder a utilidades de mongoose si lo necesitan */
    void     *mg_req;
} Request;

/* ── Response ─────────────────────────────────────────────────────────── */
typedef struct Response {
    Arena    *arena;

    int    status;
    char  *body;
    size_t body_len;

    KeyValue headers[CLAVEL_MAX_HEADERS];
    int      header_count;
} Response;

/* ── Request helpers ──────────────────────────────────────────────────── */
const char *Request_param(Request *req, const char *key);
const char *Request_query(Request *req, const char *key);
const char *Request_post(Request *req, const char *key);
const char *Request_header(Request *req, const char *key);
void        Request_parse_query(Request *req, const char *qs);
void        Request_parse_post(Request *req);

/* ── Response helpers ─────────────────────────────────────────────────── */
void Response_set_header(Response *res, const char *key, const char *val);
void Response_html(Response *res, int status, const char *html);
void Response_json(Response *res, int status, const char *json);
void Response_text(Response *res, int status, const char *text);
void Response_redirect(Response *res, const char *url);
void Response_abort(Response *res, int status);

/* ── Reactividad / Partials ───────────────────────────────────────────── */
int Request_is_partial(Request *req);

/* ── Cookies & Sessions ───────────────────────────────────────────────── */
const char *Request_cookie(Request *req, const char *key);
const char *Request_cookie_verify(Request *req, const char *key);

void Response_cookie(Response *res, const char *key, const char *val, int http_only);
void Response_cookie_sign(Response *res, const char *key, const char *val);

void Session_set(Request *req, Response *res, const char *key, const char *val);
const char *Session_get(Request *req, const char *key);

/* ── Flash Sessions (1-request lifespan) ──────────────────────────────── */
void Session_flash(Request *req, Response *res, const char *key, const char *value);
const char *Session_get_flash(Request *req, Response *res, const char *key);

/* ── Storage / Uploads ────────────────────────────────────────────────── */
UploadedFile *Request_file(Request *req, const char *form_name);
int Storage_put(const char *path, UploadedFile *file);

#endif /* CLAVEL_HTTP_H */
