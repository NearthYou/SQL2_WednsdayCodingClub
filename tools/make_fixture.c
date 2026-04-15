#include "db.h"
#include "storage.h"

#include <stdio.h>

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "data/default.msqldb";
    SqlError error;
    error_clear(&error);

    Database db;
    if (!database_init(&db, 8, &error)) {
        fprintf(stderr, "fixture error: %s\n", error.message);
        return 1;
    }

    if (!table_insert(&db.records, "alice", 10, &error) ||
        !table_insert(&db.records, "bob", 20, &error) ||
        !table_insert(&db.records, "carol", 10, &error)) {
        fprintf(stderr, "fixture error: %s\n", error.message);
        database_free(&db);
        return 1;
    }

    if (!storage_save(path, &db, &error)) {
        fprintf(stderr, "fixture error: %s\n", error.message);
        database_free(&db);
        return 1;
    }

    database_free(&db);
    printf("wrote %s\n", path);
    return 0;
}
