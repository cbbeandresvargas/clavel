/**
 * ClaVel — Configuración de base de datos
 *
 * Edita este archivo para cambiar el motor de base de datos.
 * No es necesario modificar ningún otro archivo del framework.
 */
#ifndef CLAVEL_DATABASE_CONFIG_H
#define CLAVEL_DATABASE_CONFIG_H

/* ── Selección de driver ─────────────────────────────────────────────
 * Descomenta el driver que quieres usar:
 */
#define DB_DRIVER_SQLITE   1
#define DB_DRIVER_POSTGRES 2
#define DB_DRIVER_MYSQL    3

#define CLAVEL_DB_DRIVER   DB_DRIVER_SQLITE

/* ── DSN (Data Source Name) ──────────────────────────────────────────
 * Formato según el driver:
 *
 *   SQLite:
 *     "sqlite:./storage/clavel.db"    → archivo en disco
 *     "sqlite::memory:"            → en RAM (se pierde al reiniciar)
 *
 *   PostgreSQL:
 *     "postgresql://user:pass@localhost:5432/clavel_dev"
 *
 *   MySQL / MariaDB:
 *     "mysql://user:pass@localhost:3306/clavel_dev"
 *
 * (Se configura dinámicamente usando el .env en tiempo de ejecución)
 */
#endif /* CLAVEL_DATABASE_CONFIG_H */
