#ifndef CLAVEL_LOGGER_H
#define CLAVEL_LOGGER_H

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR
} LogLevel;

/* Inicializa el logger. Crea el directorio de logs si no existe. */
void Logger_init(void);

/* Escribe un mensaje en el log. Usar a través de las macros. */
void Logger_log(LogLevel level, const char *file, int line, const char *fmt, ...);

/* Cierra el logger (opcional, para limpieza final) */
void Logger_free(void);

#define Log_info(...)    Logger_log(LOG_LEVEL_INFO,    __FILE__, __LINE__, __VA_ARGS__)
#define Log_warning(...) Logger_log(LOG_LEVEL_WARNING, __FILE__, __LINE__, __VA_ARGS__)
#define Log_error(...)   Logger_log(LOG_LEVEL_ERROR,   __FILE__, __LINE__, __VA_ARGS__)

#endif /* CLAVEL_LOGGER_H */
