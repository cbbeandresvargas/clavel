#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "http.h"
#include "vendor/hmac_sha256.h"

/* ── Compat ─────────────────────────────────────────────────────────── */
#ifdef _WIN32
#  define cv_strcasecmp _stricmp
#else
#  include <strings.h>
#  define cv_strcasecmp strcasecmp
#endif

/* ── URL-decode (in-place) ───────────────────────────────────────────── */
static void _urldecode(char *dst, const char *src, size_t dst_len) {
    size_t di = 0;
    for (size_t si = 0; src[si] && di + 1 < dst_len; si++) {
        if (src[si] == '%' && src[si+1] && src[si+2]) {
            char hex[3] = { src[si+1], src[si+2], 0 };
            dst[di++] = (char)strtol(hex, NULL, 16);
            si += 2;
        } else if (src[si] == '+') {
            dst[di++] = ' ';
        } else {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
}

/* ── Request_parse_query ─────────────────────────────────────────────── */
void Request_parse_query(Request *req, const char *qs) {
    if (!qs || !*qs) return;
    char *copy = Arena_strdup(req->arena, qs);
    char *tok  = strtok(copy, "&");
    while (tok && req->query_count < CLAVEL_MAX_PARAMS) {
        char *eq = strchr(tok, '=');
        if (eq) {
            *eq = '\0';
            char key[256] = {0}, val[1024] = {0};
            _urldecode(key, tok,    sizeof(key) - 1);
            _urldecode(val, eq + 1, sizeof(val) - 1);
            req->query[req->query_count].key   = Arena_strdup(req->arena, key);
            req->query[req->query_count].value = Arena_strdup(req->arena, val);
            req->query_count++;
        }
        tok = strtok(NULL, "&");
    }
}

void Request_parse_post(Request *req) {
    if (!req->body || req->body_len == 0) return;
    char *copy = Arena_strdup(req->arena, req->body);
    char *tok  = strtok(copy, "&");
    while (tok && req->post_count < CLAVEL_MAX_PARAMS) {
        char *eq = strchr(tok, '=');
        if (eq) {
            *eq = '\0';
            char key[256] = {0}, val[1024] = {0};
            _urldecode(key, tok,    sizeof(key) - 1);
            _urldecode(val, eq + 1, sizeof(val) - 1);
            req->post[req->post_count].key   = Arena_strdup(req->arena, key);
            req->post[req->post_count].value = Arena_strdup(req->arena, val);
            req->post_count++;
        }
        tok = strtok(NULL, "&");
    }
}

/* ── Request helpers ─────────────────────────────────────────────────── */
const char *Request_param(Request *req, const char *key) {
    for (int i = 0; i < req->param_count; i++)
        if (strcmp(req->params[i].key, key) == 0) return req->params[i].value;
    return NULL;
}

const char *Request_query(Request *req, const char *key) {
    for (int i = 0; i < req->query_count; i++)
        if (strcmp(req->query[i].key, key) == 0) return req->query[i].value;
    return NULL;
}

const char *Request_post(Request *req, const char *key) {
    for (int i = 0; i < req->post_count; i++)
        if (strcmp(req->post[i].key, key) == 0) return req->post[i].value;
    return NULL;
}

const char *Request_header(Request *req, const char *key) {
    for (int i = 0; i < req->header_count; i++)
        if (cv_strcasecmp(req->headers[i].key, key) == 0) return req->headers[i].value;
    return NULL;
}

/* ── Response helpers ────────────────────────────────────────────────── */
void Response_set_header(Response *res, const char *key, const char *val) {
    if (res->header_count >= CLAVEL_MAX_HEADERS) return;
    res->headers[res->header_count].key   = Arena_strdup(res->arena, key);
    res->headers[res->header_count].value = Arena_strdup(res->arena, val);
    res->header_count++;
}

static void _set_body(Response *res, int status, const char *ct,
                      const char *body) {
    res->status   = status;
    res->body     = (char *)Arena_strdup(res->arena, body ? body : "");
    res->body_len = strlen(res->body);
    Response_set_header(res, "Content-Type", ct);
}

void Response_html(Response *res, int status, const char *html) {
    _set_body(res, status, "text/html; charset=utf-8", html);
}

void Response_json(Response *res, int status, const char *json) {
    _set_body(res, status, "application/json; charset=utf-8", json);
}

void Response_text(Response *res, int status, const char *text) {
    _set_body(res, status, "text/plain; charset=utf-8", text);
}

void Response_abort(Response *res, int status) {
    const char *msg;
    switch (status) {
        case 400: msg = "Bad Request";            break;
        case 401: msg = "Unauthorized";           break;
        case 403: msg = "Forbidden";              break;
        case 404: msg = "Not Found";              break;
        case 405: msg = "Method Not Allowed";     break;
        case 500: msg = "Internal Server Error";  break;
        default:  msg = "Error";                  break;
    }
    char buf[256];
    snprintf(buf, sizeof(buf),
        "<!DOCTYPE html><html><head><title>%d %s</title></head>"
        "<body style='font-family:sans-serif;padding:2rem'>"
        "<h1 style='color:#e74c3c'>%d — %s</h1>"
        "<p>ClaVel Framework</p></body></html>",
        status, msg, status, msg);
    Response_html(res, status, buf);
}

void Response_redirect(Response *res, const char *url) {
    Response_set_header(res, "Location", url);
    Response_html(res, 302, "Redirecting...");
}

/* ── Reactividad / Partials ───────────────────────────────────────────── */
int Request_is_partial(Request *req) {
    const char *h = Request_header(req, "X-ClaVel-Request");
    return h && strcmp(h, "true") == 0;
}

/* ── Cookies & Sessions ───────────────────────────────────────────────── */
static void _parse_cookies_if_needed(Request *req) {
    if (req->cookie_count > 0) return; /* Ya parseado */
    const char *hdr = Request_header(req, "Cookie");
    if (!hdr) return;
    char *copy = Arena_strdup(req->arena, hdr);
    char *tok = strtok(copy, ";");
    while (tok && req->cookie_count < CLAVEL_MAX_COOKIES) {
        while (*tok == ' ') tok++; /* Trim space */
        char *eq = strchr(tok, '=');
        if (eq) {
            *eq = '\0';
            req->cookies[req->cookie_count].key   = Arena_strdup(req->arena, tok);
            req->cookies[req->cookie_count].value = Arena_strdup(req->arena, eq + 1);
            req->cookie_count++;
        }
        tok = strtok(NULL, ";");
    }
}

const char *Request_cookie(Request *req, const char *key) {
    _parse_cookies_if_needed(req);
    for (int i = 0; i < req->cookie_count; i++) {
        if (strcmp(req->cookies[i].key, key) == 0) return req->cookies[i].value;
    }
    return NULL;
}

void Response_cookie(Response *res, const char *key, const char *val, int http_only) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s=%s; Path=/; %s", key, val, http_only ? "HttpOnly;" : "");
    Response_set_header(res, "Set-Cookie", Arena_strdup(res->arena, buf));
}

void Response_cookie_sign(Response *res, const char *key, const char *val) {
    char hash[65];
    hmac_sha256_hex((const uint8_t*)CLAVEL_SECRET_KEY, strlen(CLAVEL_SECRET_KEY),
                    (const uint8_t*)val, strlen(val), hash);
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s.%s", val, hash);
    Response_cookie(res, key, buf, 1);
}

const char *Request_cookie_verify(Request *req, const char *key) {
    const char *val = Request_cookie(req, key);
    if (!val) return NULL;
    char *copy = Arena_strdup(req->arena, val);
    char *dot = strrchr(copy, '.');
    if (!dot) return NULL;
    *dot = '\0';
    const char *data = copy;
    const char *hash = dot + 1;
    
    char expected[65];
    hmac_sha256_hex((const uint8_t*)CLAVEL_SECRET_KEY, strlen(CLAVEL_SECRET_KEY),
                    (const uint8_t*)data, strlen(data), expected);
                    
    if (strcmp(hash, expected) == 0) {
        return data; /* Firma válida */
    }
    return NULL; /* Firma inválida o modificada */
}

/* ── Sessions (Stateless Cookie) ──────────────────────────────────────── */
void Session_set(Request *req, Response *res, const char *key, const char *val) {
    /* Minimalista: para múltiples keys requeriríamos un diccionario JSON en el cookie, 
     * por ahora, para simplicidad de demo, una llave es un cookie. 
     * En un entorno real se serializa toda la sesión a JSON. */
    char cookie_key[256];
    snprintf(cookie_key, sizeof(cookie_key), "session_%s", key);
    Response_cookie_sign(res, cookie_key, val);
}

const char *Session_get(Request *req, const char *key) {
    char cookie_key[256];
    snprintf(cookie_key, sizeof(cookie_key), "session_%s", key);
    return Request_cookie_verify(req, cookie_key);
}

/* ── Flash Sessions (1-request lifespan) ──────────────────────────────── */
void Session_flash(Request *req, Response *res, const char *key, const char *value) {
    char cookie_key[256];
    snprintf(cookie_key, sizeof(cookie_key), "flash_%s", key);
    Response_cookie_sign(res, cookie_key, value);
}

const char *Session_get_flash(Request *req, Response *res, const char *key) {
    char cookie_key[256];
    snprintf(cookie_key, sizeof(cookie_key), "flash_%s", key);
    const char *val = Request_cookie_verify(req, cookie_key);
    if (val) {
        /* Delete the cookie so it doesn't persist */
        char buf[512];
        snprintf(buf, sizeof(buf), "%s=; Path=/; HttpOnly; Max-Age=0", cookie_key);
        Response_set_header(res, "Set-Cookie", buf);
    }
    return val;
}

/* ── Storage / Uploads (Stubs) ────────────────────────────────────────── */
UploadedFile *Request_file(Request *req, const char *form_name) {
    /* TODO: Parse multipart form-data.
       Requires processing req->body boundary, which mongoose can do, 
       but for raw HTTP requires a simple multipart parser. */
    (void)req; (void)form_name;
    return NULL; 
}

int Storage_put(const char *path, UploadedFile *file) {
    if (!file || !file->data) return -1;
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "storage/%s", path);
    FILE *f = fopen(full_path, "wb");
    if (!f) return -1;
    fwrite(file->data, 1, file->size, f);
    fclose(f);
    return 0;
}
