/* This file generates a large binary data file for local perf runs. */
#include "store.h"

#include <stdlib.h>

/* 요약: 성능 비교용 대량 책 데이터를 만들어 저장한다. */
Err gen_perf(const char *path, int cnt, char *err, size_t cap) {
    Db db;
    int i;
    char title[TITLE_LEN];
    char author[AUTH_LEN];
    char genre[GENRE_LEN];
    Err res;
    int id;

    db_init(&db);
    res = db_set_path(&db, path, err, cap);
    if (res != ERR_OK) {
        db_free(&db);
        return res;
    }
    for (i = 0; i < cnt; ++i) {
        snprintf(title, sizeof(title), "Book %d", i + 1);
        snprintf(author, sizeof(author), "Author %d", i % 1000);
        snprintf(genre, sizeof(genre), "Genre %d", i % 20);
        res = db_add(&db, title, author, genre, &id, err, cap);
        if (res != ERR_OK) {
            db_free(&db);
            return res;
        }
    }
    res = db_save(&db, err, cap);
    db_free(&db);
    return res;
}

#ifdef PERF_MAIN
/* 요약: 대량 데이터 생성기를 독립 실행형으로 돌린다. */
int main(int argc, char **argv) {
    char err[256];
    int cnt;

    if (argc != 3) {
        fprintf(stderr, "usage: gen_perf <path> <count>\n");
        return 1;
    }
    cnt = atoi(argv[2]);
    if (cnt <= 0) {
        fprintf(stderr, "count must be positive\n");
        return 1;
    }
    if (gen_perf(argv[1], cnt, err, sizeof(err)) != ERR_OK) {
        fprintf(stderr, "[ERR] %s\n", err);
        return 1;
    }
    printf("generated %d rows in %s\n", cnt, argv[1]);
    return 0;
}
#endif
