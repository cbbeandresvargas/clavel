# 🌸 ClaVel Framework

[![Licencia: MIT](https://img.shields.io/badge/Licencia-MIT-fuchsia.svg)](LICENSE)
[![Versión](https://img.shields.io/badge/versión-0.1.0--alpha-violet)](CHANGELOG.md)
[![Build](https://img.shields.io/badge/build-CMake%20%2B%20Ninja-blue)](CMakeLists.txt)
[![Lenguaje](https://img.shields.io/badge/lenguaje-C11-orange)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Estado](https://img.shields.io/badge/estado-experimental-yellow)]()

> La experiencia de desarrollo de Laravel. El rendimiento de C.  
> Un binario. Sin dependencias. Decenas de miles de req/s.

**[English](README.md)** · [Docs](https://clavel.dev/es/docs) · [Blog](https://clavel.dev/es/blog) · [Changelog](CHANGELOG.md)

---

ClaVel es un framework web MVC batteries-included escrito en C11. Router, ORM, plantillas Blade, CSRF, sesiones en cookies, validación de formularios y UI reactiva — compilado en un único binario de ~3 MB con cero dependencias en runtime.

> **v0.1.0 — Temprano y Experimental.** La API puede cambiar antes de v1.0. Úsalo bajo tu propio riesgo y, por favor, reporta los issues.

## Características

| | |
|---|---|
| ⚡ **Sin dependencias** | Un binario. Mongoose HTTP embebido. Sin npm, pip ni gem. |
| 🧠 **Arena allocator** | Pool de 1 MB por petición. Liberación `O(1)`. Sin pausas de GC. |
| 🗄️ **ORM encadenable** | Prepared statements. SQLite / PostgreSQL / MySQL. Anti-SQL injection. |
| 🌿 **Plantillas Blade** | Compiladas a C al hacer build. `@if`, `@foreach`, `@csrf`, layouts. |
| 🔒 **Auth y CSRF** | Cookies firmadas con HMAC-SHA256. Rotación de tokens. Flash messages. |
| ✨ **UI Reactiva** | `c-get` / `c-post` / `c-target`. View Transitions API. Sin SPA. |
| 🛡️ **Validación** | `required \| min:N \| max:N \| email \| unique:tabla,col` |
| 📦 **Migraciones** | Esquema en C. Patrón `up_` / `down_`. Auto-ejecución al iniciar. |

## Inicio Rápido

**Linux / macOS:**
```bash
curl -sS https://clavel.dev/install | bash
```

**Manual:**
```bash
git clone https://github.com/cbbeandresvargas/clavel mi-app
cd mi-app
cmake -B cmake-build -G Ninja
cmake --build cmake-build
./cmake-build/clavel_app   # → http://localhost:8080
```

**Requisitos:** GCC/Clang ≥ 12, CMake ≥ 3.20, Ninja.

## Uso

### Controlador

```c
// app/controllers/product_controller.c
#include <stdlib.h>
#include "product_controller.h"
#include "../../framework/db.h"
#include "../../framework/view.h"
#include "../../framework/validator.h"

void ProductController_show(Request *req, Response *res) {
    DB_set_arena(req->arena);

    const char *slug = Request_param(req, "slug");
    QueryBuilder *qb = DB_table("products");
    DBRow *product = qb->where(qb, "slug", "=", slug)
                       ->where(qb, "active", "=", "1")
                       ->first(qb);

    if (!product) { Response_abort(res, 404); return; }

    TemplateData *d = product;            // DBRow ES un TemplateData
    TemplateData_set(d, "page_title", DBRow_get(product, "name"));

    View_render(req, res, "products.show", d);
}

void ProductController_store(Request *req, Response *res) {
    ValidationResult val = Validator_check(req, (ValidationRule[]){
        {"name",  "required|min:3|max:100"},
        {"slug",  "required|min:3|unique:products,slug"},
        {"price", "required"},
        {NULL, NULL}
    });
    if (val.fails) return Response_redirect_back_with_errors(req, res, &val);

    DB_set_arena(req->arena);
    DBRow *data = DBRow_new(req->arena);
    DBRow_set(data, "name",  Request_post(req, "name"));
    DBRow_set(data, "slug",  Request_post(req, "slug"));
    DBRow_set(data, "price", Request_post(req, "price"));
    DB_table("products")->create(qb, data);

    Session_flash(req, res, "success", "Producto creado.");
    Response_redirect(res, "/products");
}
```

### Vista (Blade)

```html
<!-- app/views/products/show.blade.html -->
<x-layout>

<h1>{{ name }}</h1>
<p>{{ description }}</p>

@if(in_stock)
  <span>✓ En stock: {{ stock }} unidades</span>
@else
  <span>✗ Sin stock</span>
@endif

<button c-post="/carrito" c-target="#cart-count">
  Agregar al carrito
</button>

</x-layout>
```

### Rutas

```c
// app/routes.c
void register_routes(void) {
    Middleware_use(Middleware_logger);
    Middleware_use(Middleware_csrf);

    Route_get("/products",        ProductController_index);
    Route_get("/products/create", ProductController_create);
    Route_post("/products",       ProductController_store);
    Route_get("/products/{slug}", ProductController_show);
}
```

### Migración

```c
// app/migrations/001_create_products.c
static void blueprint(SchemaTable *t) {
    Table_id(t);
    Col_unique(Table_string(t, "slug"));
    Table_string(t, "name");
    Col_nullable(Table_text(t, "description"));
    Col_default(Table_string(t, "price"), "0.00");
    Col_default(Table_integer(t, "stock"), "0");
    Col_default(Table_integer(t, "active"), "1");
    Table_timestamps(t);
}

void up_001_create_products(void) { Schema_create("products", blueprint); }
void down_001_create_products(void) { Schema_dropIfExists("products"); }
```

## Estructura del Proyecto

```
mi-app/
├── app/
│   ├── routes.c              ← registra rutas y middlewares
│   ├── controllers/
│   ├── views/
│   │   ├── layouts/app.blade.html
│   │   └── <seccion>/<nombre>.blade.html
│   ├── migrations/
│   └── seeders/
├── framework/                ← núcleo de ClaVel (no modificar)
├── public/
│   ├── style.css             ← embebido en el binario al compilar
│   └── app.js                ← sistema reactivo
├── config/database.h         ← elige SQLite / Postgres / MySQL
└── CMakeLists.txt
```

## Modelo de Memoria

Cada petición HTTP obtiene un `Arena` de 1 MB. Todas las asignaciones dentro de la petición usan `Arena_alloc`, `Arena_strdup` o `Arena_sprintf`. Después de enviar la respuesta, el arena completo se libera en `O(1)`. Nunca guardes punteros entre peticiones. Nunca llames a `malloc`/`free` en un controlador.

## Hoja de Ruta

| Fase | Estado | Descripción |
|------|--------|-------------|
| 1 — HTTP Core | ✅ | Arena Allocator, Request/Response, Mongoose |
| 2 — Router | ✅ | `{parámetros}`, stack de middlewares, CORS |
| 3 — CLI + Vistas | ✅ | Transpilador Blade→C, minificación, assets embebidos |
| 4 — ORM | ✅ | Query builder encadenable, SQLite/PostgreSQL/MySQL |
| 5 — Auth/CSRF | ✅ | Sesiones HMAC-SHA256, tokens CSRF, flash messages |
| 6 — Build System | ✅ | CMake + Ninja, binario único, `clavel watch` |
| 7 — Reactividad | ✅ | `c-get`/`c-post`/`c-target`, View Transitions API |
| 8 — Migraciones | ✅ | `Schema_create`, `Table_*`, `Col_*`, seeders |
| 9 — Validación | ✅ | `Validator_check()`, `required\|min\|max\|email\|unique` |
| 10 — Config/Logs | ✅ | Parser `.env`, `Log_info/warning/error` |
| 11 — Minificación | ✅ | HTML/CSS/JS minificados al compilar |
| 12 — Storage | ✅ | Assets embebidos en RAM, streaming de `storage/public/` |

## Contribuir

ClaVel es de licencia MIT y está muy abierto a contribuciones — especialmente en esta etapa temprana.

Consulta **[CONTRIBUTING-es.md](CONTRIBUTING-es.md)** para la guía completa en español.  
See **[CONTRIBUTING.md](CONTRIBUTING.md)** for the guide in English.

## Licencia

[MIT](LICENSE)
