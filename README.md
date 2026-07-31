# 🌸 ClaVel Framework

> El "Laravel" de C. MVC batteries-included. Un binario. Decenas de miles de req/s.

```c
Route_get("/productos/{slug}", ProductoController_show);

void ProductoController_show(Request *req, Response *res) {
    const char *slug = Request_param(req, "slug");
    TemplateData *d  = TemplateData_new(req->arena);
    TemplateData_set(d, "nombre", slug);
    View_render(res, "productos.show", d);
}
```

---

## ⚡ Instalación del entorno de desarrollo

### 🪟 Windows (recomendado: MSYS2)

MSYS2 instala GCC, CMake y todas las herramientas POSIX en Windows.

**Paso 1 — Instalar MSYS2**

1. Descarga el instalador desde **[msys2.org](https://www.msys2.org/)**
2. Ejecuta el instalador y acepta los valores por defecto
3. Al terminar, abre la terminal **MSYS2 UCRT64** (búscala en el menú Inicio)

**Paso 2 — Instalar GCC + CMake + Ninja**

```bash
# En la terminal MSYS2 UCRT64:
pacman -Syu
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-gdb
```

**Paso 3 — Verificar la instalación**

```bash
gcc --version    # → gcc (Rev...) 13.x.x
cmake --version  # → cmake version 3.x.x
```

**Paso 4 — Agregar al PATH de Windows** *(opcional, para usar desde PowerShell)*

Agrega `C:\msys64\ucrt64\bin` a las variables de entorno de Windows.

---

### 🍎 macOS

```bash
# Instalar Xcode Command Line Tools (incluye clang y make)
xcode-select --install

# Instalar CMake y Ninja con Homebrew
brew install cmake ninja
```

---

### 🐧 Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install gcc cmake ninja-build
```

### 🐧 Linux (Fedora/RHEL)

```bash
sudo dnf install gcc cmake ninja-build
```

---

## 🚀 Primer build

```bash
# 1. Clonar / navegar al proyecto
cd /ruta/al/proyecto/clavel   # o en MSYS2: cd /c/GH/clavel

# 2. Configurar CMake (genera el sistema de build)
cmake -B cmake-build -G Ninja

# 3. Compilar todo (CLI → vistas → servidor)
cmake --build cmake-build

# 4. Ejecutar el servidor
./cmake-build/clavel_app          # Linux/macOS
./cmake-build/clavel_app.exe      # Windows (MSYS2)
```

El servidor arranca en **http://localhost:8080** 🎉

---

## 🛠️ Flujo de desarrollo

```
app/views/*.blade.html  ──┐
public/style.css          ├──► clavel-cli build ──► build/views.h
public/app.js           ──┘                         build/static_assets.h
                                                          │
app/routes.c ────────────────────────────────────────┐    │
framework/*.c ───────────────────────────────────────┤    │
main.c ──────────────────────────────────────────────┴────▼
                                                     gcc / clang
                                                          │
                                                          ▼
                                                    clavel_app  (~3 MB)
```

### Reconstruir tras cambiar vistas

CMake detecta automáticamente los cambios en `.blade.html` y recompila:

```bash
cmake --build cmake-build
```

### Agregar una nueva página

**1. Crea la vista:**
```bash
# app/views/blog/index.blade.html
```

**2. Crea el controlador:**
```c
// app/controllers/blog_controller.h + .c
void BlogController_index(Request *req, Response *res);
```

**3. Registra la ruta en `app/routes.c`:**
```c
Route_get("/blog", BlogController_index);
```

**4. Recompila:**
```bash
cmake --build cmake-build
```

---

## 📁 Estructura del proyecto

```
clavel/
├── CMakeLists.txt              # Build system (cross-platform)
├── main.c                      # Entrada del servidor
│
├── framework/                  # Core del framework
│   ├── arena.h / arena.c       # Arena Allocator (O(1) alloc/free)
│   ├── http.h / http.c         # Request / Response
│   ├── router.h / router.c     # Router con {parámetros}
│   ├── middleware.h / .c       # Stack de middlewares
│   ├── view.h / view.c         # Sistema de vistas + TemplateData
│   ├── strbuf.h                # String builder (header-only)
│   └── vendor/
│       ├── mongoose.h          # HTTP server (single-file)
│       └── mongoose.c
│
├── app/                        # Tu código
│   ├── routes.c                # 👈 Define tus rutas aquí
│   ├── controllers/
│   │   └── home_controller.c   # Controladores
│   └── views/
│       ├── layouts/
│       │   └── app.blade.html  # Layout principal
│       └── home/
│           ├── index.blade.html
│           └── about.blade.html
│
├── clavel-cli/                 # CLI transpilador (escrito en C)
│   ├── clavel.c
│   └── CMakeLists.txt
│
├── public/                     # Assets estáticos
│   ├── style.css               # CSS (embebido en el binario)
│   └── app.js                  # JS  (embebido en el binario)
│
└── build/                      # Generado automáticamente
    ├── views.h                 # Vistas compiladas → funciones C
    └── static_assets.h        # CSS/JS como strings C
```

---

## 🧩 Sintaxis Blade soportada

| Sintaxis | Descripción |
|----------|-------------|
| `{{ variable }}` | Inserta con **auto-escape HTML** (anti-XSS) |
| `{!! variable !!}` | Inserta **sin escape** (HTML crudo) |
| `@if(var)` / `@else` / `@endif` | Bloque condicional (truthy si no es vacío/"0"/"false") |
| `@foreach(col, item)` / `@endforeach` | Iteración — requiere `col_count` + `col_N_campo` en TemplateData |
| `@error('campo')` / `@enderror` | Muestra mensajes de validación del campo |
| `@csrf` | Inyecta `<input type="hidden" name="csrf_token">` |
| `<x-layout>` / `</x-layout>` | Envuelve con el layout `layouts/app.blade.html` |

### Ejemplo: `@foreach` en el controlador

```c
// Controlador — pasar lista de items
TemplateData_set(d, "products_count", "3");
TemplateData_set(d, "products_0_name", "Laptop Pro");
TemplateData_set(d, "products_1_name", "Teclado MX");
TemplateData_set(d, "products_2_name", "Monitor 4K");
```

```html
<!-- Vista Blade -->
@foreach(products, product)
  <li>{{ product.name }}</li>
@endforeach
```

### Ejemplo: reactividad nativa

```html
<!-- El servidor devuelve solo el fragmento si X-ClaVel-Request: true -->
<div id="stock-badge">{{ stock }}</div>
<button c-get="/products/1/stock" c-target="#stock-badge">
    Revisar stock
</button>
```

```c
// Controlador — renderizado condicional
if (Request_is_partial(req)) {
    View_render_partial(req, res, "components.stock", d);
} else {
    View_render(req, res, "products.show", d);
}
```

---

## 🗺️ Hoja de ruta

| Fase | Estado | Descripción |
|------|--------|-------------|
| 1 — HTTP Core | ✅ | Arena Allocator, Request/Response, Mongoose |
| 2 — Router | ✅ | Parámetros `{slug}`, middlewares, CORS |
| 3 — CLI + Vistas | ✅ | Transpilador Blade→C, minificación Zero-Cost, assets embebidos |
| 4 — ORM | ✅ | Query Builder encadenable, SQLite/PostgreSQL/MySQL, prepared statements |
| 5 — Auth/CSRF | ✅ | Sesiones firmadas HMAC-SHA256, tokens CSRF, flash sessions |
| 6 — Build System | ✅ | CMake + Ninja, binario único, `clavel watch` con hot-reload |
| 7 — Reactividad | ✅ | `c-get`/`c-post`/`c-target`, `Request_is_partial()`, View Transitions |
| 8 — Migraciones | ✅ | `Schema_create`, `Table_*`, `Col_*`, seeders, `Factory` |
| 9 — Validación | ✅ | `Validator_check()`, reglas `required|min|max|email|unique` |
| 10 — Config/Logs | ✅ | Parser `.env`, `Log_info/warning/error` → `storage/logs/clavel.log` |
| 11 — Minificación | ✅ | HTML/CSS/JS minificados en tiempo de compilación por el CLI |
| 12 — Storage | ✅ | Assets embebidos en RAM, `storage/public/` en disco con streaming |

---

## 📄 Licencia

MIT — úsalo como quieras.
