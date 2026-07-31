#ifndef CLAVEL_FACTORY_H
#define CLAVEL_FACTORY_H

#include "arena.h"

/* ── Generación de datos falsos (Seeders) ────────────────────────────── */

/* Genera un número aleatorio entre min y max (inclusive) */
int Factory_random_int(int min, int max);

/* Genera un flotante aleatorio entre min y max */
double Factory_random_double(double min, double max);

/* Devuelve un elemento aleatorio de un arreglo de strings terminado en NULL */
const char *Factory_random_element(const char **elements);

/* Genera un string aleatorio alfanumérico en el Arena proporcionado */
const char *Factory_random_string(Arena *arena, int length);

#endif /* CLAVEL_FACTORY_H */
