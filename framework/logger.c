#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "logger.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

static FILE *log_file = NULL;

void Logger_init(void) {
#ifdef _WIN32
    CreateDirectoryA("storage", NULL);
    CreateDirectoryA("storage\\logs", NULL);
#else
    mkdir("storage", 0777);
    mkdir("storage/logs", 0777);
#endif

    log_file = fopen("storage/logs/clavel.log", "a");
    if (!log_file) {
        fprintf(stderr, "[clavel] Advertencia: No se pudo abrir storage/logs/clavel.log\n");
    }
}

void Logger_log(LogLevel level, const char *file, int line, const char *fmt, ...) {
    time_t now;
    time(&now);
    struct tm *info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", info);

    const char *level_str = "INFO";
    const char *color = "\033[36m"; /* Cyan */
    
    if (level == LOG_LEVEL_WARNING) {
        level_str = "WARNING";
        color = "\033[33m"; /* Yellow */
    } else if (level == LOG_LEVEL_ERROR) {
        level_str = "ERROR";
        color = "\033[31m"; /* Red */
    }

    char msg_buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg_buf, sizeof(msg_buf), fmt, args);
    va_end(args);

    /* 1. Imprimir a consola con color */
    printf("%s[%s] %s:\033[0m %s\n", color, time_buf, level_str, msg_buf);

    /* 2. Guardar en archivo (sin color y con ruta de archivo) */
    if (log_file) {
        fprintf(log_file, "[%s] %s: %s (en %s:%d)\n", time_buf, level_str, msg_buf, file, line);
        fflush(log_file); /* Escribir a disco inmediatamente para no perder logs si crashea */
    }
}

void Logger_free(void) {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}
