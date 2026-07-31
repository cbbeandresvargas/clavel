/**
 * ClaVel — main.c
 * Punto de entrada del servidor. Inicializa mongoose, registra rutas y
 * middlewares, y despacha cada petición HTTP a través del framework.
 */

/* ── Mongoose: deshabilitar TLS para simplificar el build ──────────── */
#define MG_ENABLE_OPENSSL 0
#define MG_ENABLE_MBEDTLS 0

#include "framework/vendor/mongoose.h"
#include "framework/arena.h"
#include "framework/http.h"
#include "framework/router.h"
#include "framework/middleware.h"
#include "framework/view.h"
#include "framework/db.h"
#include "framework/env.h"
#include "framework/logger.h"

/* Configuración de base de datos */
#include "config/database.h"

/* Drivers de base de datos (condicionales según CMake) */
#if CLAVEL_DB_DRIVER == DB_DRIVER_SQLITE
#  include "framework/drivers/sqlite_driver.h"
#elif CLAVEL_DB_DRIVER == DB_DRIVER_POSTGRES
#  include "framework/drivers/postgres_driver.h"
#elif CLAVEL_DB_DRIVER == DB_DRIVER_MYSQL
#  include "framework/drivers/mysql_driver.h"
#endif

/* Archivos autogenerados por clavel-cli */
#include "build/views.h"
#include "build/static_assets.h"
#include "build/migrations.h"
#include "build/seeders.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>

/* ── Forward declarations ──────────────────────────────────────────── */
void register_routes(void);

/* ── Estado global del servidor ────────────────────────────────────── */
static int g_running = 1;

static void _sig_handler(int sig) {
    (void)sig;
    printf("\n\033[33m[clavel] Apagando servidor...\033[0m\n");
    g_running = 0;
}

/* ── Servir assets estáticos embebidos ─────────────────────────────── */
static int _serve_static(struct mg_connection *c,
                          struct mg_http_message *hm) {
    struct mg_str uri = hm->uri;

    /* /style.css */
    if (mg_match(uri, mg_str("/style.css"), NULL)) {
        mg_http_reply(c, 200,
            "Content-Type: text/css\r\n"
            "Cache-Control: public, max-age=3600\r\n",
            "%s", _clavel_css);
        return 1;
    }
    /* /app.js */
    if (mg_match(uri, mg_str("/app.js"), NULL)) {
        mg_http_reply(c, 200,
            "Content-Type: application/javascript\r\n"
            "Cache-Control: public, max-age=3600\r\n",
            "%s", _clavel_js);
        return 1;
    }
    
    /* Archivos dinámicos: /storage/* -> se leen del disco (storage/public) */
    if (mg_match(uri, mg_str("/storage/#"), NULL)) {
        struct mg_http_serve_opts opts = { .root_dir = "storage/public" };
        
        /* Hack: reescribir URI temporalmente para que Mongoose mapee
         * /storage/file.png a storage/public/file.png en lugar de
         * storage/public/storage/file.png */
        struct mg_str old_uri = hm->uri;
        hm->uri.buf += 8; /* saltar "/storage" */
        hm->uri.len -= 8;
        
        mg_http_serve_dir(c, hm, &opts);
        
        hm->uri = old_uri; /* restaurar */
        return 1;
    }

    return 0;
}

/* ── Handler principal de mongoose ─────────────────────────────────── */
static void _mg_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev != MG_EV_HTTP_MSG) return;

    struct mg_http_message *hm = (struct mg_http_message *)ev_data;

    /* Assets estáticos embebidos */
    if (_serve_static(c, hm)) return;

    /* ── Crear arena por petición (1 MB) ──────────────────────────── */
    Arena *arena = Arena_new(ARENA_DEFAULT_SIZE);
    if (!arena) {
        mg_http_reply(c, 500, "", "Internal Server Error");
        return;
    }

    /* Hacer el arena disponible para el ORM en esta petición */
    DB_set_arena(arena);

    /* ── Construir Request ────────────────────────────────────────── */
    Request req;
    memset(&req, 0, sizeof(req));
    req.arena = arena;

    req.method = Arena_strndup(arena, hm->method.buf, hm->method.len);

    /* Path sin query string */
    struct mg_str uri = hm->uri;
    req.path = Arena_strndup(arena, uri.buf, uri.len);

    /* Query string */
    if (hm->query.len > 0) {
        char *qs = Arena_strndup(arena, hm->query.buf, hm->query.len);
        Request_parse_query(&req, qs);
    }

    /* Body */
    if (hm->body.len > 0) {
        req.body     = Arena_strndup(arena, hm->body.buf, hm->body.len);
        req.body_len = hm->body.len;
        if (strcmp(req.method, "POST") == 0 || strcmp(req.method, "PUT") == 0) {
            Request_parse_post(&req);
        }
    }

    /* Headers */
    for (int i = 0; i < MG_MAX_HTTP_HEADERS; i++) {
        if (hm->headers[i].name.len == 0) break;
        if (req.header_count >= CLAVEL_MAX_HEADERS) break;
        req.headers[req.header_count].key =
            Arena_strndup(arena, hm->headers[i].name.buf,
                                 hm->headers[i].name.len);
        req.headers[req.header_count].value =
            Arena_strndup(arena, hm->headers[i].value.buf,
                                 hm->headers[i].value.len);
        req.header_count++;
    }

    /* ── Construir Response ───────────────────────────────────────── */
    Response res;
    memset(&res, 0, sizeof(res));
    res.arena  = arena;
    res.status = 200;

    /* ── Middlewares → Router ─────────────────────────────────────── */
    Middleware_run(&req, &res);

    if (res.status == 0 || res.body == NULL) {
        /* El middleware no cortocircuitó — despachar al router */
        if (!Router_dispatch(&g_router, &req, &res)) {
            Response_abort(&res, 404);
        }
    }

    /* ── Construir cabeceras HTTP de respuesta ─────────────────────── */
    char extra_headers[1024] = "";
    for (int i = 0; i < res.header_count; i++) {
        char line[256];
        snprintf(line, sizeof(line), "%s: %s\r\n",
                 res.headers[i].key, res.headers[i].value);
        size_t remaining = sizeof(extra_headers) - strlen(extra_headers) - 1;
        strncat(extra_headers, line, remaining);
    }

    /* ── Enviar respuesta ─────────────────────────────────────────── */
    mg_http_reply(c, res.status, extra_headers,
                  "%.*s", (int)res.body_len, res.body);

    /* Access Logging */
    if (res.status >= 500) {
        Log_error("%s %s - %d", req.method, req.path, res.status);
    } else if (res.status >= 400) {
        Log_warning("%s %s - %d", req.method, req.path, res.status);
    } else {
        Log_info("%s %s - %d", req.method, req.path, res.status);
    }

    /* ── Liberar arena — O(1) ─────────────────────────────────────── */
    Arena_destroy(arena);
}

/* ── main ─────────────────────────────────────────────────────────── */
/* ── Inicializar base de datos ──────────────────────────────────────── */
static void _init_db(void) {
    /* Crear directorio de datos si no existe */
#ifdef _WIN32
    _mkdir("data");
#else
    mkdir("data", 0755);
#endif

    /* Seleccionar y registrar driver según config/database.h */
#if CLAVEL_DB_DRIVER == DB_DRIVER_SQLITE
    DB_set_driver(SQLiteDriver_new());
    
    char dsn[256];
    snprintf(dsn, sizeof(dsn), "sqlite:%s", Env_get("DB_PATH", "storage/clavel.db"));
    if (DB_connect(dsn) != 0) {
        Log_error("No se pudo conectar a SQLite: %s", dsn);
        return;
    }
#elif CLAVEL_DB_DRIVER == DB_DRIVER_POSTGRES
    DB_set_driver(PostgresDriver_new());
    /* En Postgres, componer URL: postgresql://user:pass@host/db */
#elif CLAVEL_DB_DRIVER == DB_DRIVER_MYSQL
    DB_set_driver(MySQLDriver_new());
#else
#  error "CLAVEL_DB_DRIVER no válido en config/database.h"
#endif

    Log_info("Base de datos conectada correctamente.");

    /* Aplicar migraciones pendientes (definidas en build/migrations.h) */
    DB_migrate(g_clavel_migrations, g_clavel_migrations_count, NULL);
}

int main(int argc, char *argv[]) {
    /* Inicializar Entorno y Logger */
    Env_init(".env");
    Logger_init();

    const char *port = Env_get("APP_PORT", "8080");
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = argv[++i];
        }
    }

    signal(SIGINT,  _sig_handler);
    signal(SIGTERM, _sig_handler);

    printf("\n");
    printf("  \033[35m🌸 ClaVel Framework v0.1\033[0m\n");
    printf("  \033[90m─────────────────────────────────\033[0m\n");
    printf("  \033[36mServidor:\033[0m  http://localhost:%s\n", port);
    printf("  \033[36mProceso:\033[0m   PID %d\n", (int)
#ifdef _WIN32
           GetCurrentProcessId()
#else
           getpid()
#endif
    );
    printf("  \033[36mParar:\033[0m     Ctrl+C\n");
    printf("  \033[90m─────────────────────────────────\033[0m\n\n");

    /* Inicializar base de datos y migraciones */
    _init_db();

    /* ── Comandos CLI del servidor (ej. --seed) ────────────────────────── */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--seed") == 0) {
            printf("\n\033[36m[clavel] Ejecutando Seeders...\033[0m\n");
            for (int s = 0; s < g_clavel_seeders_count; s++) {
                printf("  -> Ejecutando seeder: %s\n", g_clavel_seeders[s].name);
                g_clavel_seeders[s].run();
            }
            printf("\033[32m[clavel] Seeders ejecutados exitosamente. (%d totales)\033[0m\n", g_clavel_seeders_count);
            return 0;
        }
    }

    /* Registrar vistas y rutas */
    Views_init();
    register_routes();

    /* Inicializar mongoose */
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    char addr[64];
    snprintf(addr, sizeof(addr), "http://0.0.0.0:%s", port);

    if (!mg_http_listen(&mgr, addr, _mg_handler, NULL)) {
        fprintf(stderr, "\033[31m[clavel] Error: no se puede escuchar en %s\033[0m\n",
                addr);
        mg_mgr_free(&mgr);
        return 1;
    }

    while (g_running) {
        mg_mgr_poll(&mgr, 100);
    }

    mg_mgr_free(&mgr);
    printf("\n\033[36m[clavel] Servidor detenido.\033[0m\n");
    
    Env_free();
    Logger_free();
    return 0;
}
