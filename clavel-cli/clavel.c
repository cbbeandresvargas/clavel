/**
 * clavel-cli — Transpilador Blade → C y embebedor de assets
 *
 * Uso:
 *   clavel build [--root <directorio>]
 *   clavel build                   # usa el directorio actual como raíz
 *
 * Genera:
 *   build/views.h         — Vistas Blade compiladas a funciones C
 *   build/static_assets.h — CSS/JS embebidos como strings C
 *
 * Sintaxis Blade soportada:
 *   {{ variable }}         — Inserción con auto-escape HTML
 *   {!! variable !!}       — Inserción SIN escape (raw)
 *   @if(var)  ... @endif   — Condicional (truthy si el valor es no-vacío)
 *   @foreach(items,item)   — Iteración (Fase 4, cuando haya ORM)
 *   <x-layout>             — Envuelve el contenido en el layout principal
 *   <!-- ... -->           — Los comentarios HTML se preservan
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

#ifdef _WIN32
#  include <windows.h>
#  include <direct.h>
#  define PATH_SEP '\\'
#  define PATH_SEP_STR "\\"
#else
#  include <dirent.h>
#  include <sys/stat.h>
#  define PATH_SEP '/'
#  define PATH_SEP_STR "/"
#endif

/* ── Constantes ──────────────────────────────────────────────────────── */
#define MAX_FILES       128
#define MAX_PATH        512
#define MAX_FILE_SIZE   (512 * 1024) /* 512 KB por vista */
#define MAX_CSS_SIZE    (256 * 1024) /* 256 KB de CSS */
#define MAX_MIGRATIONS  128

/* ── Utilidades ──────────────────────────────────────────────────────── */
static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "\033[31m[clavel] Error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\033[0m\n");
    va_end(ap);
    exit(1);
}

static void info(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "\033[35m[clavel]\033[0m ");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);
}

/* ── Leer archivo completo ───────────────────────────────────────────── */
static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > MAX_FILE_SIZE) { fclose(f); return NULL; }
    char *buf = malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t read = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[read] = '\0';
    if (out_len) *out_len = read;
    return buf;
}

/* ── Trim whitespace ─────────────────────────────────────────────────── */
static void trim(char *s) {
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len-1])) s[--len] = '\0';
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
}

/* ── Nombre de función a partir del path ─────────────────────────────── */
/*
 * "app/views/home/index.blade.html"  → "_view_home_index"
 * "app/views/layouts/app.blade.html" → "_view_layouts_app"
 * "app/views/productos/show.blade.html" → "_view_productos_show"
 */
static void path_to_fn(const char *rel_path, char *fn_out, char *name_out) {
    /* Encontrar "views/" en el path */
    const char *views_pos = strstr(rel_path, "views");
    if (!views_pos) { strcpy(fn_out, "_view_unknown"); strcpy(name_out, "unknown"); return; }

    const char *p = views_pos + strlen("views");
    if (*p == '/' || *p == '\\') p++;

    char seg[256];
    strncpy(seg, p, sizeof(seg) - 1);
    seg[sizeof(seg)-1] = '\0';

    /* Quitar extensión .blade.html */
    char *blade = strstr(seg, ".blade.html");
    if (blade) *blade = '\0';

    /* Construir nombre "home.index" */
    char dot_name[256];
    strncpy(dot_name, seg, sizeof(dot_name) - 1);
    dot_name[sizeof(dot_name)-1] = '\0';
    for (char *c = dot_name; *c; c++) {
        if (*c == '/' || *c == '\\') *c = '.';
    }
    strcpy(name_out, dot_name);

    /* Construir nombre de función "_view_home_index" */
    char under_name[256];
    strncpy(under_name, seg, sizeof(under_name) - 1);
    under_name[sizeof(under_name)-1] = '\0';
    for (char *c = under_name; *c; c++) {
        if (*c == '/' || *c == '\\') *c = '_';
    }
    snprintf(fn_out, 256, "_view_%s", under_name);
}

/* ── Escapar string para literal C ──────────────────────────────────── */
static void emit_c_string_char(FILE *out, char c) {
    if (c == '"')       fputs("\\\"", out);
    else if (c == '\\') fputs("\\\\", out);
    else if (c == '\n') fputs("\\n", out);
    else if (c == '\r') { /* skip CR */ }
    else if (c == '\t') fputs("\\t", out);
    else                fputc(c, out);
}

/* ── Emitir StrBuf_add con literal de texto (con Minificación Zero-Cost) ── */
static void emit_text(FILE *out, const char *start, size_t len) {
    if (len == 0) return;
    fprintf(out, "    StrBuf_add(_sb, \"");
    
    int in_comment = 0;
    int last_was_space = 0;
    
    for (size_t i = 0; i < len; i++) {
        /* Detect HTML comment */
        if (!in_comment && i + 3 < len && start[i] == '<' && start[i+1] == '!' && start[i+2] == '-' && start[i+3] == '-') {
            in_comment = 1;
            i += 3;
            continue;
        }
        if (in_comment && i + 2 < len && start[i] == '-' && start[i+1] == '-' && start[i+2] == '>') {
            in_comment = 0;
            i += 2;
            continue;
        }
        if (in_comment) continue;
        
        char c = start[i];
        int is_sp = isspace((unsigned char)c);
        
        if (is_sp) {
            if (!last_was_space) {
                emit_c_string_char(out, ' ');
                last_was_space = 1;
            }
        } else {
            emit_c_string_char(out, c);
            last_was_space = 0;
        }
    }
    fprintf(out, "\");\n");
}

/* ── Parsear y generar el body de una vista ──────────────────────────── */
static void generate_body(FILE *out, const char *content, size_t len,
                          int indent) {
    const char *p = content;
    const char *end = content + len;
    const char *text_start = p;

    (void)indent; /* reservado para anidamiento futuro */

    while (p < end) {

        /* ── {{ variable }} ─────────────────────────────────────── */
        if (p + 1 < end && p[0] == '{' && p[1] == '{') {
            emit_text(out, text_start, (size_t)(p - text_start));
            const char *var_start = p + 2;
            const char *var_end   = strstr(var_start, "}}");
            if (!var_end) { p++; continue; }
            char var[128] = {0};
            size_t vlen = (size_t)(var_end - var_start);
            if (vlen >= sizeof(var)) vlen = sizeof(var) - 1;
            strncpy(var, var_start, vlen);
            trim(var);
            if (strcmp(var, "__slot") == 0) {
                fprintf(out, "    StrBuf_add(_sb, _slot);\n");
            } else {
                fprintf(out,
                    "    StrBuf_add(_sb, _html_safe(a, TemplateData_get(d, \"%s\")));\n",
                    var);
            }
            p = var_end + 2;
            text_start = p;
            continue;
        }

        /* ── {!! raw !!} ────────────────────────────────────────── */
        if (p + 1 < end && p[0] == '{' && p[1] == '!') {
            emit_text(out, text_start, (size_t)(p - text_start));
            const char *raw_start = p + 2;
            if (*raw_start == '!') raw_start++;
            const char *raw_end = strstr(raw_start, "!!}");
            if (!raw_end) { p++; continue; }
            char var[128] = {0};
            size_t vlen = (size_t)(raw_end - raw_start);
            if (vlen >= sizeof(var)) vlen = sizeof(var) - 1;
            strncpy(var, raw_start, vlen);
            trim(var);
            fprintf(out,
                "    StrBuf_add(_sb, TemplateData_get(d, \"%s\"));\n", var);
            p = raw_end + 3;
            text_start = p;
            continue;
        }

        /* ── @csrf ──────────────────────────────────────────────────────── */
        if (*p == '@' && (size_t)(end - p) >= 5 && strncmp(p, "@csrf", 5) == 0) {
            emit_text(out, text_start, (size_t)(p - text_start));
            fprintf(out,
                "    StrBuf_add(_sb, \"<input type=\\\"hidden\\\" name=\\\"csrf_token\\\" value=\\\"\");\n"
                "    StrBuf_add(_sb, _html_safe(a, TemplateData_get(d, \"csrf_token\")));\n"
                "    StrBuf_add(_sb, \"\\\">\");\n"
            );
            p += 5;
            text_start = p;
            continue;
        }

        /* ── @if(cond) ──────────────────────────────────────────── */
        if (*p == '@' && (size_t)(end - p) >= 4 &&
            strncmp(p, "@if(", 4) == 0) {
            emit_text(out, text_start, (size_t)(p - text_start));
            const char *cond_start = p + 4;
            const char *cond_end   = strchr(cond_start, ')');
            if (!cond_end) { p++; continue; }
            char cond[128] = {0};
            size_t clen = (size_t)(cond_end - cond_start);
            if (clen >= sizeof(cond)) clen = sizeof(cond) - 1;
            strncpy(cond, cond_start, clen);
            trim(cond);
            fprintf(out,
                "    if (TemplateData_truthy(d, \"%s\")) {\n", cond);
            p = cond_end + 1;
            if (*p == '\n') p++;
            text_start = p;
            continue;
        }

        /* ── @endif ─────────────────────────────────────────────── */
        if (*p == '@' && (size_t)(end - p) >= 6 &&
            strncmp(p, "@endif", 6) == 0) {
            emit_text(out, text_start, (size_t)(p - text_start));
            fprintf(out, "    }\n");
            p += 6;
            if (*p == '\n') p++;
            text_start = p;
            continue;
        }

        /* ── @else ──────────────────────────────────────────────── */
        if (*p == '@' && (size_t)(end - p) >= 5 &&
            strncmp(p, "@else", 5) == 0 && !isalnum((unsigned char)p[5])) {
            emit_text(out, text_start, (size_t)(p - text_start));
            fprintf(out, "    } else {\n");
            p += 5;
            if (*p == '\n') p++;
            text_start = p;
            continue;
        }

        /* ── @error('field') ────────────────────────────────────── */
        if (*p == '@' && (size_t)(end - p) >= 8 && strncmp(p, "@error(", 7) == 0) {
            emit_text(out, text_start, (size_t)(p - text_start));
            const char *field_start = p + 7;
            if (*field_start == '\'' || *field_start == '"') field_start++;
            const char *field_end = strchr(field_start, ')');
            if (field_end) {
                const char *quote = field_end - 1;
                if (*quote == '\'' || *quote == '"') quote--; else quote = field_end - 1;
                char field[128] = {0};
                size_t flen = (size_t)(quote - field_start + 1);
                if (flen >= sizeof(field)) flen = sizeof(field) - 1;
                strncpy(field, field_start, flen);
                fprintf(out,
                    "    const char *err_%s = TemplateData_get(d, \"error_%s\");\n"
                    "    if (err_%s && err_%s[0] != '\\0') {\n"
                    "        TemplateData_set(d, \"message\", err_%s);\n",
                    field, field, field, field, field);
                p = field_end + 1;
                if (*p == '\n') p++;
                text_start = p;
                continue;
            }
        }

        /* ── @enderror ────────────────────────────────────────── */
        if (*p == '@' && (size_t)(end - p) >= 9 && strncmp(p, "@enderror", 9) == 0) {
            emit_text(out, text_start, (size_t)(p - text_start));
            fprintf(out, "        TemplateData_set(d, \"message\", \"\");\n    }\n");
            p += 9;
            if (*p == '\n') p++;
            text_start = p;
            continue;
        }

        p++;
    }

    /* Texto restante */
    emit_text(out, text_start, (size_t)(p - text_start));
}

/* ── Determinar si una vista es layout ──────────────────────────────── */
/*
 * Los layouts viven en views/layouts/ y reciben un parámetro `_slot`.
 * Su firma es: static char *_view_layouts_X(Arena*,TemplateData*,const char*)
 */
static int is_layout(const char *fn_name) {
    return strncmp(fn_name, "_view_layouts_", strlen("_view_layouts_")) == 0;
}

/* ── Compilar una vista ──────────────────────────────────────────────── */
static void compile_view(FILE *out, const char *path, const char *fn_name,
                         const char *view_name) {
    size_t len;
    char *content = read_file(path, &len);
    if (!content) {
        fprintf(stderr, "[clavel] Advertencia: no se puede leer %s\n", path);
        return;
    }

    info("Compilando: %s → %s()", view_name, fn_name);

    /* Detectar uso de <x-layout> */
    const char *layout_tag  = strstr(content, "<x-layout>");
    const char *layout_end  = strstr(content, "</x-layout>");
    int uses_layout = (layout_tag != NULL && layout_end != NULL);

    if (is_layout(fn_name)) {
        /* Layout: firma de 3 parámetros */
        fprintf(out,
            "static char *%s(Arena *a, TemplateData *d, const char *_slot) {\n"
            "    StrBuf *_sb = StrBuf_new(a);\n",
            fn_name);
        generate_body(out, content, len, 0);
        fprintf(out,
            "    return StrBuf_str(_sb);\n"
            "}\n\n");
    } else if (uses_layout) {
        /* Vista con layout — genera el contenido del slot y lo pasa */
        const char *inner_start = layout_tag + strlen("<x-layout>");
        const char *inner_end   = layout_end;
        /* Saltamos el salto de línea inicial del slot si existe */
        if (*inner_start == '\n') inner_start++;

        fprintf(out,
            "static char *%s(Arena *a, TemplateData *d) {\n"
            "    StrBuf *_sb = StrBuf_new(a);\n",
            fn_name);
        generate_body(out, inner_start, (size_t)(inner_end - inner_start), 0);
        fprintf(out,
            "    const char *_inner = StrBuf_str(_sb);\n"
            "    if (TemplateData_truthy(d, \"__is_partial\")) return (char*)_inner;\n"
            "    return _view_layouts_app(a, d, _inner);\n"
            "}\n\n");
    } else {
        /* Vista simple sin layout */
        fprintf(out,
            "static char *%s(Arena *a, TemplateData *d) {\n"
            "    StrBuf *_sb = StrBuf_new(a);\n",
            fn_name);
        generate_body(out, content, len, 0);
        fprintf(out,
            "    return StrBuf_str(_sb);\n"
            "}\n\n");
    }

    free(content);
}

/* ── Colección de vistas encontradas ─────────────────────────────────── */
typedef struct {
    char path[MAX_PATH];
    char fn_name[256];
    char view_name[256];
    int  is_layout;
} ViewFile;

static ViewFile g_views[MAX_FILES];
static int      g_view_count = 0;

/* ── Colección de migraciones encontradas ────────────────────────────── */
typedef struct {
    char name[256];
    char path[MAX_PATH];
} MigrationFile;

static MigrationFile g_migrations[MAX_MIGRATIONS];
static int           g_migration_count = 0;

static MigrationFile g_seeders[MAX_MIGRATIONS];
static int           g_seeder_count = 0;

/* ── Caminar directorios ─────────────────────────────────────────────── */
#ifdef _WIN32
static void walk_dir(const char *dir_path) {
    WIN32_FIND_DATAA fd;
    char search[MAX_PATH];
    snprintf(search, sizeof(search), "%s\\*", dir_path);
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s\\%s", dir_path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            walk_dir(full);
        } else {
            size_t fn_len = strlen(fd.cFileName);
            if (fn_len > 11 && strcmp(fd.cFileName + fn_len - 11, ".blade.html") == 0) {
                if (g_view_count >= MAX_FILES) continue;
                ViewFile *vf = &g_views[g_view_count++];
                strncpy(vf->path, full, sizeof(vf->path) - 1);
                /* Normalizar separadores */
                for (char *c = vf->path; *c; c++) if (*c == '\\') *c = '/';
                path_to_fn(vf->path, vf->fn_name, vf->view_name);
                vf->is_layout = is_layout(vf->fn_name);
            }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static void walk_migrations(const char *dir_path) {
    WIN32_FIND_DATAA fd;
    char search[MAX_PATH];
    snprintf(search, sizeof(search), "%s\\*.c", dir_path);
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            if (g_migration_count >= MAX_MIGRATIONS) continue;
            MigrationFile *mf = &g_migrations[g_migration_count++];
            snprintf(mf->path, sizeof(mf->path), "%s\\%s", dir_path, fd.cFileName);
            strncpy(mf->name, fd.cFileName, sizeof(mf->name) - 1);
            // Remover la extension .c del nombre
            size_t len = strlen(mf->name);
            if (len > 2 && strcmp(mf->name + len - 2, ".c") == 0) {
                mf->name[len - 2] = '\0';
            }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

static void walk_seeders(const char *dir_path) {
    WIN32_FIND_DATAA fd;
    char search[MAX_PATH];
    snprintf(search, sizeof(search), "%s\\*", dir_path);
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s\\%s", dir_path, fd.cFileName);
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            size_t len = strlen(fd.cFileName);
            if (len > 2 && strcmp(fd.cFileName + len - 2, ".c") == 0) {
                if (g_seeder_count >= MAX_MIGRATIONS) continue;
                MigrationFile *sf = &g_seeders[g_seeder_count++];
                strncpy(sf->path, full, sizeof(sf->path) - 1);
                strncpy(sf->name, fd.cFileName, sizeof(sf->name) - 1);
                sf->name[len - 2] = '\0';
            }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}
#else
static void walk_dir(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", dir_path, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            walk_dir(full);
        } else {
            size_t fn_len = strlen(ent->d_name);
            if (fn_len > 11 && strcmp(ent->d_name + fn_len - 11, ".blade.html") == 0) {
                if (g_view_count >= MAX_FILES) continue;
                ViewFile *vf = &g_views[g_view_count++];
                strncpy(vf->path, full, sizeof(vf->path) - 1);
                path_to_fn(vf->path, vf->fn_name, vf->view_name);
                vf->is_layout = is_layout(vf->fn_name);
            }
        }
    }
    closedir(dir);
}

static void walk_migrations(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", dir_path, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (!S_ISDIR(st.st_mode)) {
            size_t len = strlen(ent->d_name);
            if (len > 2 && strcmp(ent->d_name + len - 2, ".c") == 0) {
                if (g_migration_count >= MAX_MIGRATIONS) continue;
                MigrationFile *mf = &g_migrations[g_migration_count++];
                strncpy(mf->path, full, sizeof(mf->path) - 1);
                strncpy(mf->name, ent->d_name, sizeof(mf->name) - 1);
                mf->name[len - 2] = '\0';
            }
        }
    }
    closedir(dir);
}

static void walk_seeders(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir) return;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", dir_path, ent->d_name);
        struct stat st;
        if (stat(full, &st) != 0) continue;
        if (!S_ISDIR(st.st_mode)) {
            size_t len = strlen(ent->d_name);
            if (len > 2 && strcmp(ent->d_name + len - 2, ".c") == 0) {
                if (g_seeder_count >= MAX_MIGRATIONS) continue;
                MigrationFile *sf = &g_seeders[g_seeder_count++];
                strncpy(sf->path, full, sizeof(sf->path) - 1);
                strncpy(sf->name, ent->d_name, sizeof(sf->name) - 1);
                sf->name[len - 2] = '\0';
            }
        }
    }
    closedir(dir);
}
#endif

/* ── Minificador de Assets (CSS/JS) ──────────────────────────────────── */
static void minify_asset(char *content, size_t *len_inout, int is_js) {
    char *src = content;
    char *dst = content;
    int in_block_comment = 0;
    int in_line_comment = 0;
    char in_string = 0;
    int last_was_space = 0;
    
    while (*src) {
        if (in_block_comment) {
            if (*src == '*' && *(src + 1) == '/') {
                in_block_comment = 0;
                src += 2;
            } else {
                src++;
            }
            continue;
        }
        if (in_line_comment) {
            if (*src == '\n') {
                in_line_comment = 0;
                *dst++ = '\n';
                last_was_space = 1;
            }
            src++;
            continue;
        }
        
        if (!in_string) {
            if (*src == '/' && *(src + 1) == '*') {
                in_block_comment = 1;
                src += 2;
                continue;
            }
            if (is_js && *src == '/' && *(src + 1) == '/') {
                in_line_comment = 1;
                src += 2;
                continue;
            }
            if (*src == '"' || *src == '\'') {
                in_string = *src;
                *dst++ = *src++;
                last_was_space = 0;
                continue;
            }
            
            int is_sp = isspace((unsigned char)*src);
            if (is_sp) {
                if (!last_was_space) {
                    if (is_js && *src == '\n') {
                        *dst++ = '\n';
                    } else {
                        *dst++ = ' ';
                    }
                    last_was_space = 1;
                } else if (is_js && *src == '\n' && *(dst - 1) == ' ') {
                    *(dst - 1) = '\n';
                }
            } else {
                *dst++ = *src;
                last_was_space = 0;
            }
        } else {
            /* Dentro de un string, preservar todo */
            if (*src == '\\' && *(src + 1) != '\0') {
                *dst++ = *src++;
                *dst++ = *src++;
                continue;
            }
            if (*src == in_string) {
                in_string = 0;
            }
            *dst++ = *src;
            last_was_space = 0;
        }
        src++;
    }
    *dst = '\0';
    *len_inout = (size_t)(dst - content);
}

/* ── Embeber archivo como string C ───────────────────────────────────── */
static void embed_file_as_string(FILE *out, const char *path,
                                  const char *var_name) {
    size_t len;
    char *content = read_file(path, &len);
    if (!content) {
        fprintf(out, "static const char *%s = \"\";\n", var_name);
        return;
    }
    
    int is_js = (strstr(path, ".js") != NULL);
    minify_asset(content, &len, is_js);
    fprintf(out, "static const char *%s = \n\"", var_name);
    for (size_t i = 0; i < len; i++) {
        emit_c_string_char(out, content[i]);
        /* Romper líneas largas para evitar warnings del compilador */
        if (content[i] == '\n') fprintf(out, "\"\n\"");
    }
    fprintf(out, "\";\n\n");
    info("Embebido: %s (%zu bytes)", path, len);
    free(content);
}

/* ── Comando: build ──────────────────────────────────────────────────── */
static void cmd_build(const char *root) {
    char views_dir[MAX_PATH];
    char build_dir[MAX_PATH];
    char migrations_dir[MAX_PATH];
    char views_h[MAX_PATH];
    char assets_h[MAX_PATH];
    char migrations_h[MAX_PATH];
    char migrations_c[MAX_PATH];

    snprintf(views_dir, sizeof(views_dir), "%s/app/views",  root);
    snprintf(build_dir, sizeof(build_dir), "%s/build",      root);
    snprintf(migrations_dir, sizeof(migrations_dir), "%s/app/migrations", root);
    snprintf(views_h,   sizeof(views_h),   "%s/views.h",    build_dir);
    snprintf(assets_h,  sizeof(assets_h),  "%s/static_assets.h", build_dir);
    snprintf(migrations_h, sizeof(migrations_h), "%s/migrations.h", build_dir);
    snprintf(migrations_c, sizeof(migrations_c), "%s/migrations.c", build_dir);

    /* Crear directorio build si no existe */
#ifdef _WIN32
    _mkdir(build_dir);
#else
    mkdir(build_dir, 0755);
#endif

    /* ── Recopilar vistas ────────────────────────────────────────── */
    g_view_count = 0;
    walk_dir(views_dir);

    if (g_view_count == 0) {
        fprintf(stderr, "[clavel] Advertencia: no se encontraron vistas en %s\n",
                views_dir);
    }

    /* Ordenar: layouts primero (para que las funciones estén declaradas antes) */
    for (int i = 0; i < g_view_count; i++) {
        for (int j = i + 1; j < g_view_count; j++) {
            if (!g_views[i].is_layout && g_views[j].is_layout) {
                ViewFile tmp = g_views[i];
                g_views[i]   = g_views[j];
                g_views[j]   = tmp;
            }
        }
    }

    /* ── Generar build/views.h ───────────────────────────────────── */
    FILE *vh = fopen(views_h, "w");
    if (!vh) die("No se puede escribir %s: %s", views_h, strerror(errno));

    fprintf(vh,
        "/**\n"
        " * build/views.h — GENERADO AUTOMÁTICAMENTE por clavel-cli\n"
        " * NO EDITES ESTE ARCHIVO. Edita las vistas en app/views/ y\n"
        " * vuelve a ejecutar: clavel build\n"
        " */\n"
        "#ifndef CLAVEL_BUILD_VIEWS_H\n"
        "#define CLAVEL_BUILD_VIEWS_H\n\n"
        "#include \"../framework/view.h\"\n"
        "#include \"../framework/strbuf.h\"\n\n"
    );

    /* Declaraciones forward de todas las funciones */
    for (int i = 0; i < g_view_count; i++) {
        if (g_views[i].is_layout) {
            fprintf(vh,
                "static char *%s(Arena *a, TemplateData *d, const char *_slot);\n",
                g_views[i].fn_name);
        } else {
            fprintf(vh,
                "static char *%s(Arena *a, TemplateData *d);\n",
                g_views[i].fn_name);
        }
    }
    fprintf(vh, "\n");

    /* Definiciones de funciones */
    for (int i = 0; i < g_view_count; i++) {
        compile_view(vh, g_views[i].path, g_views[i].fn_name,
                     g_views[i].view_name);
    }

    /* Views_init — registra todas las vistas no-layout */
    fprintf(vh,
        "/* Llamado por main() para registrar todas las vistas */\n"
        "static inline void Views_init(void) {\n");
    for (int i = 0; i < g_view_count; i++) {
        if (!g_views[i].is_layout) {
            fprintf(vh,
                "    View_register(\"%s\", %s);\n",
                g_views[i].view_name, g_views[i].fn_name);
        }
    }
    fprintf(vh,
        "}\n\n"
        "#endif /* CLAVEL_BUILD_VIEWS_H */\n");
    fclose(vh);
    info("Generado: %s (%d vistas)", views_h, g_view_count);

    /* ── Generar build/static_assets.h ──────────────────────────── */
    FILE *ah = fopen(assets_h, "w");
    if (!ah) die("No se puede escribir %s: %s", assets_h, strerror(errno));

    fprintf(ah,
        "/**\n"
        " * build/static_assets.h — GENERADO AUTOMÁTICAMENTE por clavel-cli\n"
        " * Assets estáticos embebidos en el binario.\n"
        " */\n"
        "#ifndef CLAVEL_STATIC_ASSETS_H\n"
        "#define CLAVEL_STATIC_ASSETS_H\n\n");

    /* Embeber CSS */
    char css_path[MAX_PATH];
    snprintf(css_path, sizeof(css_path), "%s/public/style.css", root);
    embed_file_as_string(ah, css_path, "_clavel_css");

    /* Embeber JS */
    char js_path[MAX_PATH];
    snprintf(js_path, sizeof(js_path), "%s/public/app.js", root);
    embed_file_as_string(ah, js_path, "_clavel_js");

    fprintf(ah, "#endif /* CLAVEL_STATIC_ASSETS_H */\n");
    fclose(ah);
    info("Generado: %s", assets_h);

    /* ── Generar build/migrations.h ──────────────────────────────── */
    g_migration_count = 0;
    walk_migrations(migrations_dir);

    /* Ordenar migraciones alfabéticamente */
    for (int i = 0; i < g_migration_count; i++) {
        for (int j = i + 1; j < g_migration_count; j++) {
            if (strcmp(g_migrations[i].name, g_migrations[j].name) > 0) {
                MigrationFile tmp = g_migrations[i];
                g_migrations[i]   = g_migrations[j];
                g_migrations[j]   = tmp;
            }
        }
    }

    FILE *mh = fopen(migrations_h, "w");
    if (!mh) die("No se puede escribir %s: %s", migrations_h, strerror(errno));

    fprintf(mh,
        "/**\n"
        " * build/migrations.h — GENERADO AUTOMÁTICAMENTE por clavel-cli\n"
        " */\n"
        "#ifndef CLAVEL_BUILD_MIGRATIONS_H\n"
        "#define CLAVEL_BUILD_MIGRATIONS_H\n\n"
        "#include \"../framework/db.h\"\n\n"
        "extern Migration g_clavel_migrations[];\n"
        "extern const int g_clavel_migrations_count;\n\n"
        "#endif /* CLAVEL_BUILD_MIGRATIONS_H */\n");
    fclose(mh);

    FILE *mc = fopen(migrations_c, "w");
    if (!mc) die("No se puede escribir %s: %s", migrations_c, strerror(errno));

    fprintf(mc,
        "/**\n"
        " * build/migrations.c — GENERADO AUTOMÁTICAMENTE por clavel-cli\n"
        " */\n"
        "#include \"migrations.h\"\n\n");

    for (int i = 0; i < g_migration_count; i++) {
        fprintf(mc, "extern void up_%s(void);\n", g_migrations[i].name);
        fprintf(mc, "extern void down_%s(void);\n", g_migrations[i].name);
    }
    fprintf(mc, "\nMigration g_clavel_migrations[] = {\n");
    for (int i = 0; i < g_migration_count; i++) {
        fprintf(mc, "    {\"%s\", up_%s, down_%s},\n", g_migrations[i].name, g_migrations[i].name, g_migrations[i].name);
    }
    fprintf(mc, "};\n");
    fprintf(mc, "const int g_clavel_migrations_count = %d;\n", g_migration_count);
    fclose(mc);

    info("Generado: %s y %s (%d migraciones)", migrations_h, migrations_c, g_migration_count);

    /* ── Generar build/seeders.h y .c ──────────────────────────────── */
    char seeders_dir[MAX_PATH], seeders_h[MAX_PATH], seeders_c[MAX_PATH];
    snprintf(seeders_dir, sizeof(seeders_dir), "%s/app/seeders", root);
    snprintf(seeders_h, sizeof(seeders_h), "%s/build/seeders.h", root);
    snprintf(seeders_c, sizeof(seeders_c), "%s/build/seeders.c", root);
    
    g_seeder_count = 0;
    walk_seeders(seeders_dir);
    
    for (int i = 0; i < g_seeder_count; i++) {
        for (int j = i + 1; j < g_seeder_count; j++) {
            if (strcmp(g_seeders[i].name, g_seeders[j].name) > 0) {
                MigrationFile tmp = g_seeders[i];
                g_seeders[i]   = g_seeders[j];
                g_seeders[j]   = tmp;
            }
        }
    }

    FILE *sh = fopen(seeders_h, "w");
    if (sh) {
        fprintf(sh,
            "/**\n"
            " * build/seeders.h — GENERADO AUTOMÁTICAMENTE por clavel-cli\n"
            " */\n"
            "#ifndef CLAVEL_BUILD_SEEDERS_H\n"
            "#define CLAVEL_BUILD_SEEDERS_H\n\n"
            "typedef void (*SeederFn)(void);\n"
            "typedef struct { const char *name; SeederFn run; } Seeder;\n\n"
            "extern Seeder g_clavel_seeders[];\n"
            "extern const int g_clavel_seeders_count;\n\n"
            "#endif /* CLAVEL_BUILD_SEEDERS_H */\n");
        fclose(sh);
    }
    
    FILE *sc = fopen(seeders_c, "w");
    if (sc) {
        fprintf(sc,
            "/**\n"
            " * build/seeders.c — GENERADO AUTOMÁTICAMENTE por clavel-cli\n"
            " */\n"
            "#include \"seeders.h\"\n\n");
        for (int i = 0; i < g_seeder_count; i++) {
            fprintf(sc, "extern void seeder_%s(void);\n", g_seeders[i].name);
        }
        fprintf(sc, "\nSeeder g_clavel_seeders[] = {\n");
        for (int i = 0; i < g_seeder_count; i++) {
            fprintf(sc, "    {\"%s\", seeder_%s},\n", g_seeders[i].name, g_seeders[i].name);
        }
        fprintf(sc, "};\n");
        fprintf(sc, "const int g_clavel_seeders_count = %d;\n", g_seeder_count);
        fclose(sc);
    }
    info("Generado: %s y %s (%d seeders)", seeders_h, seeders_c, g_seeder_count);
}

#ifdef _WIN32
static unsigned long long watch_scan_dir(const char *dir_path) {
    unsigned long long max_time = 0;
    WIN32_FIND_DATAA fd;
    char search[MAX_PATH];
    snprintf(search, sizeof(search), "%s\\*", dir_path);
    HANDLE h = FindFirstFileA(search, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s\\%s", dir_path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            unsigned long long t = watch_scan_dir(full);
            if (t > max_time) max_time = t;
        } else {
            size_t len = strlen(fd.cFileName);
            if ((len > 2 && strcmp(fd.cFileName + len - 2, ".c") == 0) ||
                (len > 2 && strcmp(fd.cFileName + len - 2, ".h") == 0) ||
                (len > 5 && strcmp(fd.cFileName + len - 5, ".html") == 0)) {
                ULARGE_INTEGER time;
                time.LowPart = fd.ftLastWriteTime.dwLowDateTime;
                time.HighPart = fd.ftLastWriteTime.dwHighDateTime;
                if (time.QuadPart > max_time) max_time = time.QuadPart;
            }
        }
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return max_time;
}
#endif

/* ── main ────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    printf("\n\033[35m🌸 ClaVel CLI v0.1\033[0m\n\n");

    if (argc < 2) {
        fprintf(stderr,
            "Uso: clavel <comando> [opciones]\n"
            "Comandos:\n"
            "  build [--root <dir>]  Transpila vistas y embebe assets\n"
            "  build:prod            Configura CMake y compila en modo Release (optimizado)\n"
            "  watch [--root <dir>]  Hot-Reload nativo (compila y reinicia al guardar)\n");
        return 1;
    }

    if (strcmp(argv[1], "build") == 0) {
        const char *root = ".";
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
                root = argv[++i];
            }
        }
        cmd_build(root);
        return 0;
    }

    if (strcmp(argv[1], "build:prod") == 0) {
        printf("\n\033[36m[clavel] Compilando para PRODUCCIÓN...\033[0m\n");
        system("clavel build");
        system("cmake -B build -DCMAKE_BUILD_TYPE=Release");
        system("cmake --build build --config Release");
        printf("\033[32m[clavel] ¡Binario optimizado listo en build/clavel_app.exe!\033[0m\n");
        return 0;
    }

    if (strcmp(argv[1], "watch") == 0) {
        const char *root = ".";
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--root") == 0 && i + 1 < argc) {
                root = argv[++i];
            }
        }
        
        printf("\033[36m[clavel] Iniciando modo watch (Hot-Reload nativo)...\033[0m\n");
        char app_dir[MAX_PATH], frm_dir[MAX_PATH];
        snprintf(app_dir, sizeof(app_dir), "%s\\app", root);
        snprintf(frm_dir, sizeof(frm_dir), "%s\\framework", root);
        
        unsigned long long last_time = 0;
        
#ifdef _WIN32
        PROCESS_INFORMATION pi = {0};
#else
        pid_t child_pid = 0;
#endif

        while (1) {
            unsigned long long t1 = 0, t2 = 0;
            
#ifdef _WIN32
            t1 = watch_scan_dir(app_dir);
            t2 = watch_scan_dir(frm_dir);
#endif
            unsigned long long current_time = t1 > t2 ? t1 : t2;
            
            if (current_time > last_time) {
                if (last_time > 0) {
                    printf("\n\033[33m[clavel] Archivos modificados. Recompilando...\033[0m\n");
                }
                last_time = current_time;
                
#ifdef _WIN32
                if (pi.hProcess) {
                    TerminateProcess(pi.hProcess, 0);
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    pi.hProcess = NULL;
                }
#endif
                
                system("cmake --build build");
                
                printf("\033[32m[clavel] Lanzando servidor...\033[0m\n");
#ifdef _WIN32
                STARTUPINFOA si = { sizeof(si) };
                char cmd[] = "build\\clavel_app.exe";
                CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
#endif
            }
            
#ifdef _WIN32
            Sleep(500);
#endif
        }
        return 0;
    }

    fprintf(stderr, "Comando desconocido: %s\n", argv[1]);
    return 1;
}
