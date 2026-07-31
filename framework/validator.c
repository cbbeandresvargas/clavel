#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "validator.h"

static void add_error(ValidationResult *res, const char *field, const char *msg) {
    if (res->error_count >= MAX_VALIDATION_ERRORS) return;
    res->errors[res->error_count].field = field;
    res->errors[res->error_count].message = msg;
    res->error_count++;
    res->fails = 1;
}

ValidationResult Validator_check(Request *req, ValidationRule *rules) {
    ValidationResult res;
    memset(&res, 0, sizeof(res));

    if (!rules) return res;

    for (int i = 0; rules[i].field != NULL; i++) {
        const char *field = rules[i].field;
        const char *val   = Request_post(req, field);
        if (!val) val = "";

        char rules_copy[256];
        strncpy(rules_copy, rules[i].rules, sizeof(rules_copy) - 1);
        rules_copy[255] = '\0';

        char *tok = strtok(rules_copy, "|");
        while (tok) {
            if (strcmp(tok, "required") == 0) {
                if (strlen(val) == 0) {
                    add_error(&res, field, "El campo es obligatorio.");
                    break; /* Si es requerido y está vacío, no evaluar más reglas */
                }
            }
            else if (strncmp(tok, "min:", 4) == 0) {
                int min = atoi(tok + 4);
                if (strlen(val) > 0 && strlen(val) < (size_t)min) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Mínimo %d caracteres.", min);
                    /* Allocated strings for errors are tricky without arena in res,
                       but we can allocate on request arena */
                    add_error(&res, field, Arena_strdup(req->arena, msg));
                }
            }
            else if (strncmp(tok, "max:", 4) == 0) {
                int max = atoi(tok + 4);
                if (strlen(val) > (size_t)max) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "Máximo %d caracteres.", max);
                    add_error(&res, field, Arena_strdup(req->arena, msg));
                }
            }
            else if (strcmp(tok, "email") == 0) {
                if (strlen(val) > 0 && (!strchr(val, '@') || !strchr(val, '.'))) {
                    add_error(&res, field, "Debe ser un correo válido.");
                }
            }
            else if (strncmp(tok, "unique:", 7) == 0) {
                /* unique:table,column */
                char *table = Arena_strdup(req->arena, tok + 7);
                char *col = strchr(table, ',');
                if (col) {
                    *col = '\0';
                    col++;
                } else {
                    col = (char *)field;
                }
                
                if (strlen(val) > 0) {
                    /* Comprobar en la BD si existe */
                    QueryBuilder *qb = DB_table(table);
                    DBRow *m = qb->where(qb, col, "=", val)->first(qb);
                    if (m) {
                        add_error(&res, field, "El valor ya está en uso.");
                    }
                }
            }

            tok = strtok(NULL, "|");
        }
    }

    return res;
}

void Response_redirect_back_with_errors(Request *req, Response *res, ValidationResult *val) {
    const char *referer = Request_header(req, "Referer");
    if (!referer) referer = "/"; /* Default fallback */

    /* Guardar los errores en la sesión Flash (JSON simplificado o uno por uno) */
    /* Para simplicidad, guardamos los errores uno por uno como error_CAMPO */
    for (int i = 0; i < val->error_count; i++) {
        char key[128];
        snprintf(key, sizeof(key), "error_%s", val->errors[i].field);
        Session_flash(req, res, key, val->errors[i].message);
    }
    
    /* También flashear un flag de que hubo error genérico */
    Session_flash(req, res, "has_errors", "1");

    Response_redirect(res, referer);
}
