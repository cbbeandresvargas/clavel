/**
 * ClaVel — Driver SQLite
 * Requiere: libsqlite3 del sistema
 *   Windows (MSYS2): pacman -S mingw-w64-ucrt-x86_64-sqlite3
 *   Ubuntu/Debian:   sudo apt install libsqlite3-dev
 *   macOS:           ya incluida en el SDK del sistema
 */
#ifndef CLAVEL_SQLITE_DRIVER_H
#define CLAVEL_SQLITE_DRIVER_H

#include "../db_driver.h"

/**
 * Crea un nuevo driver SQLite.
 * Uso:
 *   DB_set_driver(SQLiteDriver_new());
 *   DB_connect("sqlite:./data/app.db");
 */
DBDriver *SQLiteDriver_new(void);

#endif /* CLAVEL_SQLITE_DRIVER_H */
