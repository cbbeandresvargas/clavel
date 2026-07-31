# Contribuir a ClaVel

ClaVel está en `v0.1.0` — es una versión temprana y experimental. Cada contribución importa, ya sea un reporte de bug, una corrección de documentación o una nueva funcionalidad. Gracias por considerar ayudar.

## Formas de contribuir

- **Reportes de bugs** — abre un issue con una reproducción mínima
- **Corrección de bugs** — elige un issue abierto, comenta que estás trabajando en él, abre un PR
- **Documentación** — errores tipográficos, explicaciones confusas, ejemplos faltantes
- **Funcionalidades** — revisa el roadmap en el README primero; abre un issue para discutirlo antes de construirlo

## Configuración del entorno

```bash
# Dependencias (Linux)
sudo apt install gcc cmake ninja-build libsqlite3-dev

# Clonar y compilar
git clone https://github.com/cbbeandresvargas/clavel
cd clavel
cmake -B cmake-build -G Ninja
cmake --build cmake-build

# Ejecutar
./cmake-build/clavel_app

# Modo watch (recompila automáticamente al cambiar archivos)
./cmake-build/clavel-cli watch
```

## Flujo de trabajo

```bash
# 1. Haz fork del repo en GitHub, luego clona tu fork
git clone https://github.com/<tu-usuario>/clavel
cd clavel
git remote add upstream https://github.com/cbbeandresvargas/clavel

# 2. Crea una rama desde main
git checkout -b fix/crash-csrf-null
# o
git checkout -b feat/soporte-websocket

# 3. Haz tus cambios, compila y prueba
cmake --build cmake-build
./cmake-build/clavel_app

# 4. Haz commit con un mensaje claro
git commit -m "fix: resolver desreferenciación NULL en middleware CSRF en sesiones nuevas"

# 5. Sube y abre un PR
git push origin fix/crash-csrf-null
```

## Estilo de código C

| Regla | Ejemplo |
|---|---|
| Funciones: `Modulo_verbo_sustantivo` | `Request_parse_post`, `DB_table` |
| Tipos: `PascalCase` | `QueryBuilder`, `TemplateData` |
| Constantes/macros: `MAYUSCULAS_GUION_BAJO` | `CLAVEL_MAX_ROUTES`, `DB_DRIVER_SQLITE` |
| Indentación: 4 espacios | (sin tabs) |
| Longitud de línea: ≤ 100 caracteres | |
| Sin asignación dinámica | Usa `Arena_alloc`, nunca `malloc`/`free` en handlers de petición |
| Solo prepared statements | Nunca interpoles valores en strings SQL |
| Comentarios: explica el *porqué*, no el *qué* | Solo invariantes no obvios |

## Modelo de memoria

Cada petición corre dentro de un `Arena` de 1 MB. Todas las asignaciones por petición deben usar:

```c
Arena_alloc(arena, size)
Arena_strndup(arena, str, len)
Arena_strdup(arena, str)
Arena_sprintf(arena, fmt, ...)
```

Nunca llames a `malloc`/`calloc`/`strdup` en handlers de petición — esas asignaciones se filtrarían. El `Arena` se destruye después de enviar la respuesta; `O(1)`, sin fragmentación.

## Checklist del PR

- [ ] El código compila sin errores: `cmake --build cmake-build`
- [ ] Sin nuevas advertencias del compilador
- [ ] Sin `malloc`/`free` en handlers de petición (usar arena)
- [ ] Sin interpolación de strings en SQL (usar ORM con prepared statements)
- [ ] La descripción del PR explica **qué** y **por qué**

## Mensajes de commit

Sigue [Conventional Commits](https://www.conventionalcommits.org/es/v1.0.0/):

```
fix: resolver desreferenciación NULL en middleware CSRF
feat: agregar soporte de upgrade WebSocket
docs: aclarar el lifetime del arena allocator en el README
chore: actualizar mongoose a 7.15
```

## ¿Preguntas?

Abre un issue — todavía no hay Discord, pero respondemos a los issues de GitHub.
