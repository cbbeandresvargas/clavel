### Proyecto ClaVel: El "Laravel" de C

**ClaVel** es un framework web MVC "batteries-included" escrito en C. Su objetivo es combinar la **velocidad extrema y el despliegue en un único binario** de C, con la **experiencia de desarrollo limpia, expresiva y rápida** de Laravel.

La idea central es desplazar toda la complejidad al **tiempo de compilación**: un CLI propio traduce tus vistas HTML y modelos a código C nativo, mientras un gestor de memoria automático (Arena Allocator) elimina la necesidad de escribir `malloc` o `free`.

### La promesa: Sintaxis súper simple

El código que escribirás en tu día a día ocultará toda la complejidad de C. Así se verá tu desarrollo:

**1. Rutas limpias (`routes.c`):**

```c
Route_get("/productos/{slug}", ProductoController_show);
Route_post("/productos", ProductoController_store);

```

**2. Controladores y ORM expresivo (`producto_controller.c`):**

```c
void ProductoController_show(Request *req, Response *res) {
    // 1. Obtener parámetro de la URL
    const char *slug = Request_param(req, "slug");

    // 2. ORM encadenable (Simple y directo)
    Producto *producto = DB_table("productos")->where("slug", "=", slug)->first();

    if (!producto) {
        return Response_abort(res, 404);
    }

    // 3. Renderizar vista "Blade" compilada
    return View_render(res, "productos.show", producto);
}

```

**3. Vistas estilo Blade (`views/productos/show.blade.html`):**

```html
<x-layout>
    <h1>{{ producto.nombre }}</h1>
    <p>Precio: ${{ producto.precio }}</p>

    @if(producto.stock > 0)
        <button>Comprar</button>
    @endif
</x-layout>

```

**En resumen:** Desarrollas con la agilidad de un lenguaje de alto nivel moderno, ejecutas `make`, y obtienes un binario de 3 MB capaz de procesar decenas de miles de peticiones por segundo.

### Fase 1: Core HTTP y Memoria (Días 1-2)

Los cimientos del framework para garantizar velocidad y evitar fugas de memoria.

* **Arena Allocator:** Crear `arena.c`. Bloque de memoria (ej. 1MB) por petición HTTP que se libera automáticamente al responder. Cero `free()` manuales.
* **Servidor HTTP:** Integrar `mongoose.c` (un solo archivo) para manejar sockets y parseo del protocolo HTTP.
* **Request/Response:** Structs que encapsulen los datos entrantes (headers, body, query) y salientes.

### Fase 2: Enrutador Dinámico y Middlewares (Días 3-4)

El control de flujo de las peticiones.

* **Router:** Árbol o tabla hash que soporte parámetros dinámicos (`/products/{slug}`) y verbos HTTP (`GET`, `POST`).
* **Middlewares:** Arreglo de punteros a función ejecutados antes del controlador (interceptores para Auth o Logs).

### Fase 3: CLI "Clavel" y Vistas Blade (Días 5-7)

La herramienta generadora de código (tu Artisan).

* **Transpilador HTML:** Script CLI que lea `views/*.blade.html`, parsee variables `{{ }}` y bucles `@foreach`, y genere funciones en C en `build/views.h`.
* **Embebido de Assets:** El CLI lee `public/` (CSS, JS) y los convierte en arreglos de bytes (`static_assets.h`) para servirlos desde la RAM.

### Fase 4: Base de Datos y ORM (Días 8-10)

Persistencia de datos segura.

* **Motor:** Integrar `sqlite3.c` directamente en el binario.
* **Query Builder:** Crear funciones encadenables estilo Laravel: `DB_table("users")->where("id", "=", "1")->get();`.
* **Seguridad:** Uso estricto de consultas preparadas (*prepared statements*) internamente para evitar SQL Injection.

### Fase 5: Baterías Incluidas (Días 11-12)

Las funciones diarias de desarrollo web.

* **Sesiones/Auth:** Firmado de cookies con `HMAC-SHA256` y manejo de login.
* **Seguridad:** Auto-escape HTML en las vistas compiladas (Anti-XSS) y tokens CSRF en peticiones POST.
* **Storage:** Procesamiento de subida de archivos (`multipart/form-data`) para guardarlos en disco.

### Fase 6: Build System (Día 13)

La experiencia de usuario del desarrollador.

* **Makefile Principal:** Unifica la ejecución del CLI, la compilación de C (`gcc`) y el *hot-reload*.
* **Binario Único:** Ajustar los flags de compilación para que el resultado sea un solo ejecutable listo para producción.