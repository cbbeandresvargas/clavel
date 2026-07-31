#ifndef CLAVEL_VALIDATOR_H
#define CLAVEL_VALIDATOR_H

#include "http.h"
#include "db.h"

#define MAX_VALIDATION_ERRORS 16

typedef struct {
    const char *field;
    const char *rules; /* e.g. "required|min:3|unique:users,email" */
} ValidationRule;

typedef struct {
    const char *field;
    const char *message;
} ValidationError;

typedef struct {
    int fails;
    ValidationError errors[MAX_VALIDATION_ERRORS];
    int error_count;
} ValidationResult;

/* Ejecuta la validación. Si devuelve fails=1, la petición no pasó. */
ValidationResult Validator_check(Request *req, ValidationRule *rules);

/* Redirige a la página anterior (Referer) flasheando los errores a la sesión. */
void Response_redirect_back_with_errors(Request *req, Response *res, ValidationResult *val);

#endif /* CLAVEL_VALIDATOR_H */
