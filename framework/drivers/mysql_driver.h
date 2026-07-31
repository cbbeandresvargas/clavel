/**
 * ClaVel — Driver MySQL / MariaDB
 * Requiere: libmysqlclient o libmariadb
 *   Windows (MSYS2): pacman -S mingw-w64-ucrt-x86_64-libmariadbclient
 *   Ubuntu/Debian:   sudo apt install libmysqlclient-dev
 *   macOS:           brew install mysql-client
 */
#ifndef CLAVEL_MYSQL_DRIVER_H
#define CLAVEL_MYSQL_DRIVER_H

#include "../db_driver.h"

/**
 * Crea un nuevo driver MySQL/MariaDB.
 * Uso:
 *   DB_set_driver(MySQLDriver_new());
 *   DB_connect("mysql://user:pass@localhost:3306/mydb");
 */
DBDriver *MySQLDriver_new(void);

#endif /* CLAVEL_MYSQL_DRIVER_H */
