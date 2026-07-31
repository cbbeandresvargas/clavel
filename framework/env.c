#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "env.h"
#include "arena.h"
#include "http.h" /* para KeyValue */

#define MAX_ENV_VARS 256

static Arena *env_arena = NULL;
static KeyValue env_vars[MAX_ENV_VARS];
static int env_count = 0;

static void trim_space(char *str) {
    if (!str) return;
    char *end;
    /* Ltrim */
    char *start = str;
    while(isspace((unsigned char)*start)) start++;
    /* Rtrim */
    end = start + strlen(start) - 1;
    while(end > start && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    /* Move */
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
}

void Env_init(const char *filepath) {
    if (!filepath) filepath = ".env";
    if (!env_arena) env_arena = Arena_new(1024 * 16);
    env_count = 0;

    FILE *f = fopen(filepath, "r");
    if (!f) return; /* Es perfectamente válido no tener .env en producción */

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        trim_space(line);
        if (line[0] == '\0' || line[0] == '#') continue;

        char *eq = strchr(line, '=');
        if (eq) {
            *eq = '\0';
            char *key = line;
            char *val = eq + 1;
            
            trim_space(key);
            trim_space(val);

            /* Remover comillas si existen */
            if ((val[0] == '"' && val[strlen(val)-1] == '"') ||
                (val[0] == '\'' && val[strlen(val)-1] == '\'')) {
                val[strlen(val)-1] = '\0';
                val++;
            }

            if (env_count < MAX_ENV_VARS) {
                env_vars[env_count].key   = Arena_strdup(env_arena, key);
                env_vars[env_count].value = Arena_strdup(env_arena, val);
                env_count++;
            }
        }
    }
    fclose(f);
}

const char *Env_get(const char *key, const char *default_val) {
    /* Primero revisar entorno real del SO (por si se pasó por Docker o CLI) */
    const char *sys_val = getenv(key);
    if (sys_val) return sys_val;

    /* Segundo, revisar .env cacheado */
    for (int i = 0; i < env_count; i++) {
        if (strcmp(env_vars[i].key, key) == 0) {
            return env_vars[i].value;
        }
    }
    return default_val;
}

void Env_free(void) {
    if (env_arena) {
        Arena_destroy(env_arena);
        env_arena = NULL;
    }
    env_count = 0;
}
