#ifndef MINISQL_STORAGE_H
#define MINISQL_STORAGE_H

#include "db.h"
#include "error.h"

#include <stdbool.h>

bool storage_load(const char *path, Database *db, size_t index_order, SqlError *error);
bool storage_save(const char *path, const Database *db, SqlError *error);

#endif
