# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Configure (first time or after CMakeLists changes)
cmake -B cmake-build -G Ninja

# Build everything (transpiles Blade views, embeds assets, compiles app)
cmake --build cmake-build

# Run the server (default port 8080, reads .env)
./cmake-build/clavel_app

# Run on a specific port
./cmake-build/clavel_app --port 3000

# Run seeders then exit
./cmake-build/clavel_app --seed
```

The `DB_PATH` and `APP_PORT` environment variables are read from `.env` at startup. SQLite DB is created at `storage/clavel.db` by default.

## Architecture

ClaVel is a two-phase build: `clavel-cli` runs first to transpile Blade templates into C functions (`build/views.h`) and embed CSS/JS as C strings (`build/static_assets.h`). The main app then compiles against those generated headers. CMake handles this automatically when you run `cmake --build`.

**Request lifecycle**: Mongoose receives HTTP → `main.c` builds `Request`/`Response` from arena memory → middleware stack runs (logger, CSRF) → `Router_dispatch` finds the route → controller runs → `mg_http_reply` sends the response → arena is freed in O(1).

**Memory model**: Each request gets a 1 MB `Arena`. All allocations within a request use `Arena_alloc` or `Arena_strndup`. The arena is destroyed after the response is sent — never hold pointers across requests.

## Adding a Page

1. Create `app/views/<section>/<name>.blade.html`
2. Create `app/controllers/<name>_controller.h` and `.c`
3. Register the route in `app/routes.c` with `Route_get`, `Route_post`, etc.
4. Add the `.c` file to `clavel_app` sources in `CMakeLists.txt`
5. `cmake --build cmake-build`

## ORM

The query builder uses prepared statements — values are never interpolated into SQL:

```c
DB_set_arena(req->arena);  // always call first in a controller

// Read
DBResult *rows = DB_table("products")->where(qb, "active", "=", "1")->get(qb);
DBRow    *one  = DB_table("products")->where(qb, "slug",   "=", slug)->first(qb);

// Write
DBRow *data = DBRow_new(req->arena);
DBRow_set(data, "name", "value");
DB_table("products")->create(qb, data);
DB_table("products")->where(qb, "id", "=", "1")->update(qb, data);
DB_table("products")->where(qb, "id", "=", "1")->delete_rows(qb);
```

`DBRow` is the same struct as `TemplateData`, so a row returned from the ORM can be passed directly to `View_render`.

## Migrations & Seeders

Create files in `app/migrations/` and `app/seeders/`. CMake auto-detects them via `file(GLOB ...)`. Each migration implements `up_<name>()` and `down_<name>()` using `Schema_create`, `Table_*` column helpers, and `Col_*` modifiers. Migrations run automatically on server startup via `DB_migrate`.

## Blade Templates

| Syntax | Behavior |
|--------|----------|
| `{{ var }}` | HTML-escaped output |
| `{!! var !!}` | Raw HTML output |
| `@if(var)` / `@else` / `@endif` | Conditional (truthy: non-empty, not `"0"` or `"false"`) |
| `<x-layout>` / `</x-layout>` | Wraps in `layouts/app.blade.html` |

## Database Configuration

Edit `config/database.h` to switch the DB driver (`DB_DRIVER_SQLITE`, `DB_DRIVER_POSTGRES`, `DB_DRIVER_MYSQL`). The DSN is read at runtime from `.env` — `DB_PATH` for SQLite, or the appropriate `DATABASE_URL` for the others.

## Validation

```c
ValidationResult val = Validator_check(req, (ValidationRule[]){
    {"field", "required|min:3|max:100|unique:table,column"},
    {NULL, NULL}
});
if (val.fails) return Response_redirect_back_with_errors(req, res, &val);
```
