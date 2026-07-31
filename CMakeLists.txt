cmake_minimum_required(VERSION 3.15)
project(clavel C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Opciones de compilación ──────────────────────────────────────────
if(MSVC)
    add_compile_options(/W3 /wd4996)  # wd4996: deshabilitar warnings de funciones "unsafe"
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -O2)
endif()

# ── Paso 1: Compilar el CLI de clavel ────────────────────────────────
add_subdirectory(clavel-cli)

# ── Paso 2: Generar views.h y static_assets.h ────────────────────────
# Listar archivos de vistas para que CMake detecte cambios
file(GLOB_RECURSE BLADE_FILES
    "${CMAKE_SOURCE_DIR}/app/views/*.blade.html"
)
file(GLOB ASSET_FILES
    "${CMAKE_SOURCE_DIR}/public/*.css"
    "${CMAKE_SOURCE_DIR}/public/*.js"
)
file(GLOB MIGRATION_FILES
    "${CMAKE_SOURCE_DIR}/app/migrations/*.c"
)
file(GLOB SEEDER_FILES
    "${CMAKE_SOURCE_DIR}/app/seeders/*.c"
)

add_custom_command(
    OUTPUT
        "${CMAKE_SOURCE_DIR}/build/views.h"
        "${CMAKE_SOURCE_DIR}/build/static_assets.h"
        "${CMAKE_SOURCE_DIR}/build/migrations.h"
        "${CMAKE_SOURCE_DIR}/build/migrations.c"
        "${CMAKE_SOURCE_DIR}/build/seeders.h"
        "${CMAKE_SOURCE_DIR}/build/seeders.c"
    COMMAND
        clavel-cli build --root "${CMAKE_SOURCE_DIR}"
    DEPENDS
        clavel-cli
        ${BLADE_FILES}
        ${ASSET_FILES}
        ${MIGRATION_FILES}
        ${SEEDER_FILES}
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "🌸 Transpilando vistas Blade, embebiendo assets, migraciones y seeders..."
)

add_custom_target(clavel_build_views
    DEPENDS
        "${CMAKE_SOURCE_DIR}/build/views.h"
        "${CMAKE_SOURCE_DIR}/build/static_assets.h"
        "${CMAKE_SOURCE_DIR}/build/migrations.h"
        "${CMAKE_SOURCE_DIR}/build/migrations.c"
        "${CMAKE_SOURCE_DIR}/build/seeders.h"
        "${CMAKE_SOURCE_DIR}/build/seeders.c"
)

# ── Paso 3: Compilar la aplicación principal ─────────────────────────
add_executable(clavel_app
    main.c
    framework/arena.c
    framework/http.c
    framework/router.c
    framework/middleware.c
    framework/view.c
    framework/db.c
    framework/schema.c
    framework/validator.c
    framework/env.c
    framework/logger.c
    framework/drivers/sqlite_driver.c
    framework/drivers/postgres_driver.c
    framework/drivers/mysql_driver.c
    framework/vendor/mongoose.c
    framework/vendor/hmac_sha256.c
    framework/factory.c
    app/routes.c
    app/controllers/home_controller.c
    app/controllers/product_controller.c
    build/migrations.c
    build/seeders.c
    ${MIGRATION_FILES}
    ${SEEDER_FILES}
)

# Esperar a que los archivos generados estén listos
add_dependencies(clavel_app clavel_build_views)

# Directorios de include
target_include_directories(clavel_app PRIVATE "${CMAKE_SOURCE_DIR}")

# ── Configuración por plataforma ─────────────────────────────────────
target_compile_definitions(clavel_app PRIVATE
    MG_ENABLE_OPENSSL=0
    MG_ENABLE_MBEDTLS=0
)

# Buscar y enlazar SQLite3
find_package(SQLite3)
if(SQLite3_FOUND)
    target_link_libraries(clavel_app PRIVATE SQLite::SQLite3)
    target_compile_definitions(clavel_app PRIVATE CLAVEL_DRIVER_SQLITE=1)
endif()

# Buscar PostgreSQL (libpq)
find_package(PostgreSQL)
if(PostgreSQL_FOUND)
    target_link_libraries(clavel_app PRIVATE PostgreSQL::PostgreSQL)
    target_compile_definitions(clavel_app PRIVATE CLAVEL_DRIVER_POSTGRES=1)
endif()

# Buscar MySQL/MariaDB
find_package(PkgConfig)
if(PKG_CONFIG_FOUND)
    pkg_search_module(MARIADB libmariadb mariadb mysqlclient)
    if(MARIADB_FOUND)
        target_include_directories(clavel_app PRIVATE ${MARIADB_INCLUDE_DIRS})
        target_link_libraries(clavel_app PRIVATE ${MARIADB_LIBRARIES})
        target_compile_definitions(clavel_app PRIVATE CLAVEL_DRIVER_MYSQL=1)
    endif()
endif()

if(WIN32)
    target_link_libraries(clavel_app PRIVATE ws2_32)
    # En Windows, _stricmp está en string.h (sin lib extra)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(clavel_app PRIVATE m)
endif()

# ── Mensaje de ayuda post-build ──────────────────────────────────────
add_custom_command(TARGET clavel_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "  ✅ Build completado"
    COMMAND ${CMAKE_COMMAND} -E echo "  🚀 Ejecutar: $<TARGET_FILE:clavel_app>"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)
