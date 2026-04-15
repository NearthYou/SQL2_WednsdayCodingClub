/* In-memory books table and binary storage API. */
#ifndef STORE_H
#define STORE_H

#include "base.h"
#include "bpt.h"

typedef struct {
    int id;
    char title[TITLE_LEN];
    char author[AUTH_LEN];
    char genre[GENRE_LEN];
} Book;

typedef struct {
    Book *rows;
    size_t len;
    size_t cap;
    int next_id;
    char path[PATH_LEN];
    BpTree idx;
} Db;

void db_init(Db *db);
void db_free(Db *db);
Err db_set_path(Db *db, const char *path, char *err, size_t cap);
Err db_load(Db *db, char *err, size_t cap);
Err db_save(Db *db, char *err, size_t cap);
Err db_seed(Db *db, char *err, size_t cap);
Err db_add(Db *db, const char *title, const char *author, const char *genre,
           int *new_id, char *err, size_t cap);
Err db_reidx(Db *db, char *err, size_t cap);

#endif
