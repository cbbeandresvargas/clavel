# 🌸 ClaVel Framework

[![License: MIT](https://img.shields.io/badge/License-MIT-fuchsia.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-0.1.0--alpha-violet)](CHANGELOG.md)
[![Build](https://img.shields.io/badge/build-CMake%20%2B%20Ninja-blue)](CMakeLists.txt)
[![Language](https://img.shields.io/badge/language-C11-orange)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Status](https://img.shields.io/badge/status-experimental-yellow)]()

> Laravel's developer experience. C's performance.
> One binary. No dependencies. Tens of thousands of req/s.

**[Español](README-es.md)** · [Docs](https://clavel.dev/docs) · [Blog](https://clavel.dev/blog) · [Changelog](CHANGELOG.md)

---

ClaVel is a batteries-included MVC web framework written in C11. Router, ORM, Blade templates, CSRF, cookie sessions, form validation, and reactive UI — compiled into a single ~3 MB binary with zero runtime dependencies.

> **v0.1.0 — Early & Experimental.** The API may change before v1.0. Use at your own risk, and please report issues.

## Features

| | |
|---|---|
| ⚡ **Zero dependencies** | One binary. Mongoose HTTP server is embedded. No npm, pip, or gem. |
| 🧠 **Arena allocator** | Per-request 1 MB memory pool. `O(1)` free. No GC pauses. |
| 🗄️ **Chainable ORM** | Prepared statements. SQLite / PostgreSQL / MySQL. Anti-SQL injection. |
| 🌿 **Blade templates** | Compiled to C at build time. `@if`, `@foreach`, `@csrf`, layouts. |
| 🔒 **Auth & CSRF** | HMAC-SHA256 signed cookies. Token rotation. Flash messages. |
| ✨ **Reactive UI** | `c-get` / `c-post` / `c-target`. View Transitions API. No SPA needed. |
| 🛡️ **Validation** | `required \| min:N \| max:N \| email \| unique:table,col` |
| 📦 **Migrations** | Schema in C. `up_` / `down_` pattern. Auto-run at startup. |

## Quick Start

**Linux / macOS:**
```bash
curl -sS https://clavel.dev/install | bash
```

**Manual:**
```bash
git clone https://github.com/cbbeandresvargas/clavel my-app
cd my-app
cmake -B cmake-build -G Ninja
cmake --build cmake-build
./cmake-build/clavel_app   # → http://localhost:8080
```

**Requirements:** GCC/Clang ≥ 12, CMake ≥ 3.20, Ninja.

## Usage

### Controller

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

    TemplateData *d = product;            // DBRow IS a TemplateData
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

    Session_flash(req, res, "success", "Product created.");
    Response_redirect(res, "/products");
}
```

### View (Blade)

```html
<!-- app/views/products/show.blade.html -->
<x-layout>

<h1>{{ name }}</h1>
<p>{{ description }}</p>

@if(in_stock)
  <span>✓ In stock: {{ stock }} units</span>
@else
  <span>✗ Out of stock</span>
@endif

<button c-post="/cart" c-target="#cart-count">
  Add to cart
</button>

</x-layout>
```

### Routes

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

### Migration

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

## Project Structure

```
my-app/
├── app/
│   ├── routes.c              ← register routes and middleware
│   ├── controllers/
│   ├── views/
│   │   ├── layouts/app.blade.html
│   │   └── <section>/<name>.blade.html
│   ├── migrations/
│   └── seeders/
├── framework/                ← ClaVel core (don't modify)
├── public/
│   ├── style.css             ← embedded in binary at build time
│   └── app.js                ← reactive system
├── config/database.h         ← choose SQLite / Postgres / MySQL
└── CMakeLists.txt
```

## Memory Model

Every HTTP request gets a 1 MB `Arena`. All allocations within the request use `Arena_alloc`, `Arena_strdup`, or `Arena_sprintf`. After the response is sent, the entire arena is freed in `O(1)`. Never hold pointers across requests. Never call `malloc`/`free` in a controller.

## Roadmap

| Phase | Status | Description |
|-------|--------|-------------|
| 1 — HTTP Core | ✅ | Arena Allocator, Request/Response, Mongoose |
| 2 — Router | ✅ | `{params}`, middleware stack, CORS |
| 3 — CLI + Views | ✅ | Blade→C transpiler, minification, embedded assets |
| 4 — ORM | ✅ | Chainable query builder, SQLite/PostgreSQL/MySQL |
| 5 — Auth/CSRF | ✅ | HMAC-SHA256 sessions, CSRF tokens, flash messages |
| 6 — Build System | ✅ | CMake + Ninja, single binary, `clavel watch` |
| 7 — Reactivity | ✅ | `c-get`/`c-post`/`c-target`, View Transitions API |
| 8 — Migrations | ✅ | `Schema_create`, `Table_*`, `Col_*`, seeders |
| 9 — Validation | ✅ | `Validator_check()`, `required\|min\|max\|email\|unique` |
| 10 — Config/Logs | ✅ | `.env` parser, `Log_info/warning/error` |
| 11 — Minification | ✅ | HTML/CSS/JS minified at compile time |
| 12 — Storage | ✅ | Embedded assets in RAM, `storage/public/` disk streaming |

## Contributing

ClaVel is MIT-licensed and very open to contributions — especially at this early stage.

See **[CONTRIBUTING.md](CONTRIBUTING.md)** for the full guide.  
See **[CONTRIBUTING-es.md](CONTRIBUTING-es.md)** for the guide in Spanish.

## License

[MIT](LICENSE)
