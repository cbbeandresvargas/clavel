#include <stdio.h>
#include <stdlib.h>
#include "../../framework/db.h"
#include "../../framework/factory.h"

static const char *adjectives[] = {
    "Increíble", "Fantástico", "Elegante", "Moderno", "Clásico", 
    "Profesional", "Portátil", "Potente", "Minimalista", "Avanzado", NULL
};

static const char *nouns[] = {
    "Teléfono", "Laptop", "Teclado", "Monitor", "Ratón", 
    "Auriculares", "Cámara", "Micrófono", "Reloj", "Tableta", NULL
};

void seeder_001_product_seeder(void) {
    /* 1. Limpiar tabla (opcional) */
    /* DB_table("products")->delete_all(); // Dependería de una API extendida del ORM */
    
    printf("    -> Sembrando 50 productos de prueba...\n");
    
    /* Crear Arena temporal para los strings generados (evita memory leaks) */
    Arena *arena = Arena_new(1024 * 64);
    
    /* Configurar el ORM para que use este Arena (el Framework asume que DB tiene uno) */
    DB_set_arena(arena);
    
    for (int i = 0; i < 50; i++) {
        char name[128];
        snprintf(name, sizeof(name), "%s %s", 
            Factory_random_element(adjectives), 
            Factory_random_element(nouns)
        );
        
        char slug[128];
        snprintf(slug, sizeof(slug), "producto-%s-%d", Factory_random_string(arena, 6), i);
        
        char price[32];
        snprintf(price, sizeof(price), "%.2f", Factory_random_double(10.0, 999.99));
        
        char stock[16];
        snprintf(stock, sizeof(stock), "%d", Factory_random_int(0, 100));
        
        DBRow *data = DBRow_new(arena);
        DBRow_set(data, "name", name);
        DBRow_set(data, "slug", slug);
        DBRow_set(data, "description", "Generado por ClaVel Factory.");
        DBRow_set(data, "price", price);
        DBRow_set(data, "stock", stock);
        
        QueryBuilder *qb = DB_table("products");
        qb->create(qb, data);
        
        /* Reset the arena memory partially or just let it grow. For 50 it's fine. */
    }
    
    Arena_destroy(arena);
}
