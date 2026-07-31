/**
 * ClaVel — Driver PostgreSQL
 * Requiere: libpq (cliente de PostgreSQL)
 *   Windows (MSYS2): pacman -S mingw-w64-ucrt-x86_64-postgresql
 *   Ubuntu/Debian:   sudo apt install libpq-dev
 *   macOS:           brew install postgresql
 */
#ifndef CLAVEL_POSTGRES_DRIVER_H
#define CLAVEL_POSTGRES_DRIVER_H

#include "../db_driver.h"

/**
 * Crea un nuevo driver PostgreSQL.
 * Uso:
 *   DB_set_driver(PostgresDriver_new());
 *   DB_connect("postgresql://user:pass@localhost:5432/mydb");
 */
DBDriver *PostgresDriver_new(void);

#endif /* CLAVEL_POSTGRES_DRIVER_H */
