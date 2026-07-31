#ifndef CLAVEL_ENV_H
#define CLAVEL_ENV_H

/* Inicializa el parser de entorno leyendo el archivo especificado (por defecto ".env") */
void Env_init(const char *filepath);

/* Obtiene el valor de una variable de entorno. 
   Si no existe, devuelve default_val. */
const char *Env_get(const char *key, const char *default_val);

/* Libera la memoria ocupada por las variables de entorno */
void Env_free(void);

#endif /* CLAVEL_ENV_H */
