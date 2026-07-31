# Changelog

All notable changes to ClaVel will be documented in this file.

Format: [Keep a Changelog](https://keepachangelog.com/en/1.0.0/)  
Versioning: [Semantic Versioning](https://semver.org/spec/v2.0.0.html)

---

## [0.1.0] — 2025-07-31

### Added

**Core**
- Arena allocator: per-request 1 MB memory pool, `O(1)` free after response
- Mongoose-based HTTP server (embedded, single-file)
- `Request` / `Response` structs with arena-allocated fields
- `.env` parser for `APP_PORT`, `DB_PATH`, `CLAVEL_SECRET_KEY`
- Structured logging to `storage/logs/clavel.log` (`Log_info`, `Log_warning`, `Log_error`)

**Router**
- `Route_get`, `Route_post`, `Route_put`, `Route_delete`
- URL parameter capture: `/products/{slug}` → `Request_param(req, "slug")`
- Query string parsing: `Request_query(req, "key")`
- Middleware stack with short-circuit on `res->body != NULL`

**Blade Template Engine**
- `clavel-cli` transpiler: `.blade.html` → C functions at build time
- `{{ variable }}` with HTML auto-escape (anti-XSS)
- `{!! variable !!}` raw HTML output
- `@if(var)` / `@else` / `@endif` conditionals
- `@foreach(col, item)` / `@endforeach` loops
- `@error('field')` / `@enderror` validation error display
- `@csrf` token injection
- `<x-layout>` / `</x-layout>` with `{{ __slot }}`
- `clavel watch` command with auto-rebuild (Linux/macOS via `fork`/`execl`, Windows via `CreateProcess`)

**ORM**
- Chainable query builder: `DB_table("t")->where()->orderBy()->get()`
- `first()`, `get()`, `create()`, `update()`, `delete_rows()`
- All values via prepared statements (SQL injection not possible)
- SQLite driver (default), PostgreSQL driver, MySQL driver
- `DBRow` == `TemplateData` — ORM results pass directly to views

**Migrations & Seeders**
- C files with `up_<name>()` / `down_<name>()` pattern
- Schema builder: `Table_id`, `Table_string`, `Table_integer`, `Table_text`, `Table_timestamps`
- Column modifiers: `Col_nullable`, `Col_unique`, `Col_default`
- Auto-detect and auto-run at startup via `DB_migrate()`
- `--seed` CLI flag to run seeders and exit

**Auth & Security**
- Cookie sessions signed with HMAC-SHA256
- CSRF token generation and validation (POST/PUT/DELETE/PATCH)
- Flash messages: `Session_flash()` / `Session_get_flash()` — consumed on read

**Validation**
- `Validator_check()` with rules: `required`, `min:N`, `max:N`, `email`, `unique:table,column`
- `Response_redirect_back_with_errors()` for form re-display

**Reactive System**
- `c-get`, `c-post`, `c-target` HTML attributes
- `form[c-submit]` async form submission
- `Request_is_partial()` / `View_render_partial()` for fragment responses
- View Transitions API integration in `public/app.js`

**Build**
- CMake + Ninja: single-command cross-platform build
- CSS/JS embedded as C strings at build time
- CORS middleware (`Middleware_cors`)

### Known Limitations

- No WebSocket support
- Blade compiler does not support nested `@foreach`
- PostgreSQL `last_insert_id` may return row count in some configurations
- No Windows installer script yet
- API stability not guaranteed before `v1.0`

[0.1.0]: https://github.com/cbbeandresvargas/clavel/releases/tag/v0.1.0
