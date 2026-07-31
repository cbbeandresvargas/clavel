#include "../../framework/schema.h"

static void blueprint(SchemaTable *t) {
    Table_id(t);
    Col_unique(Table_string(t, "slug"));
    Table_string(t, "name");
    Col_nullable(Table_text(t, "description"));
    Col_default(Table_string(t, "price"), "0.00"); // SQLite doesn't strictly type decimals nicely, string is fine for now
    Col_default(Table_integer(t, "stock"), "0");
    Col_default(Table_integer(t, "active"), "1");
    Table_timestamps(t);
}

void up_001_create_products() {
    Schema_create("products", blueprint);
}

void down_001_create_products() {
    Schema_dropIfExists("products");
}
