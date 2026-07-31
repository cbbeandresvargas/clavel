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

* **Motor:** Integrar `sqlite3.c` fuera del binario y que sea agnostico con dbs, que permita sqlite, postgresql, mysql y de ser posible mongodb tambien.
* **Query Builder:** Crear funciones encadenables estilo Laravel: `DB_table("users")->where("id", "=", "1")->get();`.
* **Seguridad:** Uso estricto de consultas preparadas (*prepared statements*) internamente para evitar SQL Injection.
* **funciones minimas que deberian existir en mi framework:**
    * **Filtros (El "Query Builder")**
        * `where(columna, operador, valor)`: El filtro principal (ej. `where("stock", ">", "0")`).
        * `order_by(columna, direccion)`: Para ordenar (`order_by("created_at", "DESC")`).
        * `limit(cantidad)`: Paginación o top resultados (`limit(10)`).
    * **Ejecución y Lectura (Read)**
        * `get()`: Ejecuta los filtros previos y devuelve una lista de registros.
        * `first()`: Ejecuta los filtros pero devuelve solo el primer registro encontrado (o nulo).
        * `find(id)`: Atajo ultra rápido para buscar un solo registro por su ID principal (`DB_table("users")->find(1)`).
        * `all()`: Atajo rápido para traer toda la tabla sin filtros.
    * **Mutación (Escritura)**
        * `create(datos)`: Inserta una fila nueva.
        * `update(id, datos)`: Actualiza campos de un registro existente.
        * `delete(id)`: Elimina un registro directamente.
        * `save(registro)`: Detecta automáticamente si el registro es nuevo (hace `INSERT`) o si ya existía (hace `UPDATE`).
    * **Migraciones**

#### 1. Filtros (El "Query Builder")

Son funciones encadenables que ayudan a construir la consulta SQL paso a paso.

* **`where(columna, operador, valor)`:** El filtro principal (ej. `where("stock", ">", "0")`).
* **`order_by(columna, direccion)`:** Para ordenar (`order_by("created_at", "DESC")`).
* **`limit(cantidad)`:** Paginación o top resultados (`limit(10)`).

#### 2. Ejecución y Lectura (Read)

Son las funciones terminales que ejecutan la consulta y devuelven los datos a la Arena de memoria.

* **`get()`:** Ejecuta los filtros previos y devuelve una lista de registros.
* **`first()`:** Ejecuta los filtros pero devuelve solo el primer registro encontrado (o nulo).
* **`find(id)`:** Atajo ultra rápido para buscar un solo registro por su ID principal (`DB_table("users")->find(1)`).
* **`all()`:** Atajo rápido para traer toda la tabla sin filtros.

#### 3. Mutación (Escritura)

Funciones para insertar, modificar y destruir datos en la base de datos.

* **`create(datos)`:** Inserta una fila nueva.
* **`update(id, datos)`:** Actualiza campos de un registro existente.
* **`delete(id)`:** Elimina un registro directamente.
* **`save(registro)`:** Detecta automáticamente si el registro es nuevo (hace `INSERT`) o si ya existía (hace `UPDATE`).

---

> **Regla de oro:** Para que tu framework sea seguro, TODAS estas funciones deben convertir los valores en **Prepared Statements** bajo el capó. El usuario jamás debe concatenar cadenas SQL directas.

### Fase 5: Baterías Incluidas (Días 11-12)

Las funciones diarias de desarrollo web.

* **Sesiones/Auth:** Firmado de cookies con `HMAC-SHA256` y manejo de login.
* **Seguridad:** Auto-escape HTML en las vistas compiladas (Anti-XSS) y tokens CSRF en peticiones POST.
* **Storage:** Procesamiento de subida de archivos (`multipart/form-data`) para guardarlos en disco.

### Fase 6: Build System (Día 13)

La experiencia de usuario del desarrollador.

* **Makefile Principal:** Unifica la ejecución del CLI, la compilación de C (`gcc`) y el *hot-reload*.
* **Binario Único:** Ajustar los flags de compilación para que el resultado sea un solo ejecutable listo para producción.

### Fase 7: Reactividad Nativa (View Transitions) (Días 14-15)

El objetivo es lograr una experiencia de Single Page Application (SPA) con HTML puro y cero dependencias externas pero con renderiado del lado del servidor, para una optimizacion seo, y mejorar la experiencia del usuario. 

* **Micro-script JS (< 2KB):** Un archivo inyectado automáticamente que busca atributos como `c-get`, `c-post` y `c-target`. Intercepta eventos, usa `fetch()` y actualiza el DOM con `document.startViewTransition()`.
* **Detección en Backend (C):** Se añade una función `Request_is_partial(req)` que lee una cabecera personalizada (ej. `X-ClaVel-Request: true`).
* **Renderizado Condicional:** Si la petición es normal, el controlador renderiza toda la página con el layout. Si es "reactiva", solo devuelve el componente o fragmento HTML solicitado.

**Ejemplo de uso:**

```html
<!-- La vista en Blade -->
<button c-get="/productos/1/stock" c-target="#stock-badge">
    Revisar Stock
</button>

```

```c
// El controlador en C
void ProductoController_stock(Request *req, Response *res) {
    int stock = DB_table("productos")->find(1)->stock;
    
    if (Request_is_partial(req)) {
        return View_render_partial(res, "components.stock", stock);
    }
    return View_render(res, "productos.stock_page", stock);
}

```

---

### Fase 8: Migraciones y Seeders (Corregida para C Nativo)

Estructura y población de datos automatizada directamente desde el código C, manteniendo todo tipo-seguro y evitando inyecciones SQL.

* **Migraciones (Estructura):** Se encargarán de crear las tablas si no existen.
```c
void migration_create_products_table() {
    // En C usamos varargs (argumentos variables) terminados en NULL
    Schema_create_table("products", 
        "id", "INTEGER PRIMARY KEY AUTOINCREMENT",
        "name", "TEXT NOT NULL",
        "description", "TEXT",
        "price", "REAL DEFAULT 0.0",
        "stock", "INTEGER DEFAULT 0",
        NULL // C necesita saber dónde termina la lista
    );
}

```


* **Seeders (Fábricas de Datos):**
Para insertar datos de distintos tipos (texto, números) de forma limpia en C, definiremos un tipo `DBParam` (Clave-Valor como texto) manejado internamente por el ORM con consultas preparadas.
```c
void seeder_create_products(int count) {
    for (int i = 0; i < count; i++) {
        // Sintaxis C moderna usando inicializadores designados
        DB_insert("products", (DBParam[]){
            {"name", "Producto de prueba"},
            {"description", "Generado automáticamente"},
            {"price", "19.99"},
            {"stock", "10"},
            {NULL, NULL} // Terminador
        });
    }
}

```



---

### Fase 9: Validación de Datos (Request Validation)

El escudo principal del framework. Nunca debes confiar en lo que el usuario envía en un formulario (peticiones POST).

* **El Validador:** Una función que recibe el `Request` y un conjunto de reglas. Si falla, redirige automáticamente de vuelta con los mensajes de error.
* **Ejemplo de uso en Controlador:**
```c
void UserController_store(Request *req, Response *res) {
    // Reglas de validación al estilo Laravel
    ValidationResult val = Validator_check(req, (ValidationRule[]){
        {"name", "required|min:3|max:50"},
        {"email", "required|email|unique:users"},
        {"password", "required|min:8"},
        {NULL, NULL}
    });

    if (val.fails) {
        // Redirige atrás y guarda los errores en la sesión temporal (Flash)
        return Response_redirect_back_with_errors(res, val.errors);
    }

    // Si pasa, procedemos a guardar...
    DB_insert("users", ...);
    Response_redirect(res, "/dashboard");
}

```



---

### Fase 10: Configuración Global y Logs (Core Utilities)

Las herramientas invisibles que hacen que tu aplicación sea mantenible y segura en producción.

* **Variables de Entorno (`.env`):**
Un parser ultra ligero que se ejecuta al arrancar el servidor. Lee un archivo `.env` en la raíz del proyecto.
```env
# Archivo .env
APP_NAME=ClaVel
APP_ENV=production
APP_PORT=8080
DB_PATH=storage/database.sqlite
# o las variabels necesarias para la db correcpondiente (postgresql, mysql, etc)
# Configuración agnóstica de DB
DB_CONNECTION=postgres
DB_HOST=127.0.0.1
DB_DATABASE=mi_tienda
DB_USER=root
DB_PASS=secreta

```


**Uso en C:**
```c
// Si no encuentra APP_PORT, usa "3000" por defecto
const char *port = Env_get("APP_PORT", "3000"); 

```


* **Sistema de Logging (`Logger`):**
Si tu programa en C falla en producción, necesitas saber por qué. Escribirá de forma asíncrona (o síncrona optimizada) en `storage/logs/clavel.log`.
```c
Log_info("Servidor iniciado en el puerto %s", port);
Log_warning("Intento de login fallido para el IP: %s", req->ip);
Log_error("Error de base de datos: %s", db_get_last_error());

```



---

### Fase 11: Minificación Extrema en Tiempo de Compilación (Zero-Cost Minification)

El CLI se encargará de comprimir todos los recursos antes de generar el código C, reduciendo el peso de la transferencia de red y el tamaño del propio binario, sin afectar el contenido dinámico.

* **HTML (Vistas Blade):** Al momento de que el CLI lea el `.blade.html` para pasarlo a C, ejecutará un algoritmo que elimine saltos de línea `\n`, tabulaciones `\t` y comentarios HTML `<!-- -->`. Las variables `{{ producto.nombre }}` se mantendrán intactas porque el CLI las convierte en marcadores de formato (como `%s`) para inyectar la data de la DB después.
* **CSS y JS (Assets Estáticos):** Antes de que el CLI convierta los archivos de la carpeta `public/` en arreglos de bytes (`static_assets.h`), limpiará los espacios en blanco, saltos de línea y comentarios (`/* */` o `//`).
* **Cabeceras GZIP/Brotli (Opcional en el servidor):** Al enviar la respuesta al navegador, el servidor HTTP de ClaVel puede añadir soporte para comprimir este payload ya minificado usando GZIP, haciendo que el peso baje a unos pocos kilobytes.

**El resultado:** Tu código fuente de desarrollo se mantiene legible y ordenado, pero tu servidor responde con un HTML brutalmente comprimido, en una sola línea, a la velocidad de la luz.

### Fase 12: Gestión Inteligente de Archivos y Despliegue (Memoria vs. Disco)

El framework separará estrictamente lo que se compila dentro del binario y lo que se lee dinámicamente, optimizando tanto la RAM como el almacenamiento.

* **Regla de Separación:**
* `public/`: (Tus CSS, JS, fuentes y logo base). El CLI los minifica y **los embebe dentro del binario**. Pesan poco y se sirven desde la RAM a máxima velocidad.
* `storage/public/`: (Fotos de usuarios, PDFs, videos, audios). **NO se embeben**. Se quedan en el disco duro del servidor.


* **Enrutador Híbrido:**
El motor HTTP de ClaVel detectará la ruta. Si pides `/storage/video.mp4`, el servidor no lo carga a la RAM de golpe; usa *streaming* (lectura por fragmentos pequeños) para enviarlo al cliente, manteniendo el consumo de memoria del servidor cercano a 0.
* **Resultado de Compilación y Despliegue:**
Tras ejecutar `make` en tu computadora, la estructura final que **subirás a tu servidor VPS en producción** será únicamente esto:
```text
/produccion
├── server           # El ejecutable binario (~3MB). Contiene C, HTML, CSS y JS.
├── .env             # Tu archivo con contraseñas de producción.
└── storage/         # Carpeta de lectura/escritura dinámica.
    ├── database.sqlite
    ├── logs/
    └── public/      # Archivos multimedia subidos por los usuarios.

```


**Despliegue:** Copias esos 3 elementos a tu VPS Linux, ejecutas `./server` y tu app estará en línea. Sin instalar PHP, sin Node_modules y sin configurar Nginx (si no quieres).