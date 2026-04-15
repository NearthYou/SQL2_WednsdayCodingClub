/* This file loads, saves, seeds, and grows the in-memory book cache. */
#include "store.h"

#include "util.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char data[4];
    uint32_t ver;
    uint16_t rec_sz;
    uint16_t keep;
    uint32_t cnt;
    uint32_t next_id;
} DHdr;

/* 요약: 데이터 파일 한 레코드의 고정 크기를 돌려준다. */
static size_t rec_sz(void) {
    return sizeof(uint32_t) + TITLE_LEN + AUTH_LEN + GENRE_LEN;
}

/* 요약: 현재 열린 데이터 파일의 총 크기를 구한다. */
static Err file_size(FILE *fp, long *size, char *err, size_t cap) {
    if (fseek(fp, 0, SEEK_END) != 0) {
        err_set(err, cap, "failed to seek data file");
        return ERR_IO;
    }
    *size = ftell(fp);
    if (*size < 0) {
        err_set(err, cap, "failed to read data file size");
        return ERR_IO;
    }
    return ERR_OK;
}

/* 요약: 헤더의 행 수와 레코드 크기가 실제 파일 크기와 맞는지 본다. */
static Err check_data_size(FILE *fp, const DHdr *hdr, char *err, size_t cap) {
    long got;
    uint64_t need;
    Err res;

    res = file_size(fp, &got, err, cap);
    if (res != ERR_OK) {
        return res;
    }
    need = (uint64_t)sizeof(*hdr) + (uint64_t)hdr->cnt * (uint64_t)hdr->rec_sz;
    if ((uint64_t)got != need) {
        err_set(err, cap, "data file size does not match header");
        return ERR_FMT;
    }
    if (fseek(fp, (long)sizeof(*hdr), SEEK_SET) != 0) {
        err_set(err, cap, "failed to seek data file");
        return ERR_IO;
    }
    return ERR_OK;
}

/* 요약: 책 배열이 더 담을 수 있게 공간을 늘린다. */
static Err grow_rows(Db *db) {
    Book *next;

    if (db->len < db->cap) {
        return ERR_OK;
    }
    db->cap = db->cap == 0 ? 16 : db->cap * 2;
    next = (Book *)realloc(db->rows, db->cap * sizeof(*next));
    if (next == NULL) {
        return ERR_MEM;
    }
    db->rows = next;
    return ERR_OK;
}

/* 요약: 고정 길이 문자열 필드에 안전하게 값을 복사한다. */
static void copy_fix(char *dst, size_t cap, const char *src) {
    memset(dst, 0, cap);
    snprintf(dst, cap, "%s", src);
}

/* 요약: 책 한 행을 바이너리 파일에 쓴다. */
static Err write_row(FILE *fp, const Book *row) {
    uint32_t id;

    id = (uint32_t)row->id;
    if (fwrite(&id, sizeof(id), 1, fp) != 1 ||
        fwrite(row->title, 1, TITLE_LEN, fp) != TITLE_LEN ||
        fwrite(row->author, 1, AUTH_LEN, fp) != AUTH_LEN ||
        fwrite(row->genre, 1, GENRE_LEN, fp) != GENRE_LEN) {
        return ERR_IO;
    }
    return ERR_OK;
}

/* 요약: 책 한 행을 바이너리 파일에서 읽는다. */
static Err read_row(FILE *fp, Book *row) {
    uint32_t id;

    if (fread(&id, sizeof(id), 1, fp) != 1 ||
        fread(row->title, 1, TITLE_LEN, fp) != TITLE_LEN ||
        fread(row->author, 1, AUTH_LEN, fp) != AUTH_LEN ||
        fread(row->genre, 1, GENRE_LEN, fp) != GENRE_LEN) {
        return ERR_IO;
    }
    row->id = (int)id;
    row->title[TITLE_LEN - 1] = '\0';
    row->author[AUTH_LEN - 1] = '\0';
    row->genre[GENRE_LEN - 1] = '\0';
    return ERR_OK;
}

/* 요약: 임시 저장 파일을 실제 데이터 파일로 교체한다. */
static Err swap_file(const char *path, const char *tmp, char *err, size_t cap) {
    char bak[PATH_LEN];

    snprintf(bak, sizeof(bak), "%s.bak", path);
    remove(bak);
    if (rename(path, bak) != 0 && errno != ENOENT) {
        err_set(err, cap, "failed to move old data file");
        return ERR_IO;
    }
    if (rename(tmp, path) != 0) {
        rename(bak, path);
        err_set(err, cap, "failed to replace data file");
        return ERR_IO;
    }
    remove(bak);
    return ERR_OK;
}

/* 요약: 빈 데이터베이스 구조체를 초기 상태로 만든다. */
void db_init(Db *db) {
    memset(db, 0, sizeof(*db));
    db->next_id = 1;
    bp_init(&db->idx);
}

/* 요약: 데이터베이스가 쓰는 메모리와 인덱스를 정리한다. */
void db_free(Db *db) {
    free(db->rows);
    db->rows = NULL;
    db->len = 0;
    db->cap = 0;
    bp_free(&db->idx);
}

/* 요약: 데이터 파일 경로를 구조체에 저장한다. */
Err db_set_path(Db *db, const char *path, char *err, size_t cap) {
    if (strlen(path) >= sizeof(db->path)) {
        err_set(err, cap, "data path is too long");
        return ERR_ARG;
    }
    snprintf(db->path, sizeof(db->path), "%s", path);
    return ERR_OK;
}

/* 요약: 현재 행 배열 기준으로 id 인덱스를 다시 만든다. */
Err db_reidx(Db *db, char *err, size_t cap) {
    size_t i;
    Err res;

    bp_free(&db->idx);
    bp_init(&db->idx);
    for (i = 0; i < db->len; ++i) {
        res = bp_put(&db->idx, db->rows[i].id, (int)i);
        if (res != ERR_OK) {
            err_set(err, cap, "failed to rebuild B+ tree");
            return res;
        }
    }
    return ERR_OK;
}

/* 요약: 책 한 권을 캐시와 B+ 트리에 함께 추가한다. */
Err db_add(Db *db, const char *title, const char *author, const char *genre,
           int *new_id, char *err, size_t cap) {
    Err res;
    Book *row;

    if (strlen(title) >= TITLE_LEN || strlen(author) >= AUTH_LEN ||
        strlen(genre) >= GENRE_LEN) {
        err_set(err, cap, "string value is too long for schema");
        return ERR_EXEC;
    }
    res = grow_rows(db);
    if (res != ERR_OK) {
        err_set(err, cap, "out of memory while growing rows");
        return res;
    }

    row = &db->rows[db->len];
    memset(row, 0, sizeof(*row));
    row->id = db->next_id;
    copy_fix(row->title, sizeof(row->title), title);
    copy_fix(row->author, sizeof(row->author), author);
    copy_fix(row->genre, sizeof(row->genre), genre);
    db->len += 1;
    db->next_id += 1;

    res = bp_put(&db->idx, row->id, (int)(db->len - 1));
    if (res != ERR_OK) {
        db->len -= 1;
        db->next_id -= 1;
        err_set(err, cap, "failed to update B+ tree");
        return res;
    }
    if (new_id != NULL) {
        *new_id = row->id;
    }
    return ERR_OK;
}

/* 요약: 발표용 기본 책 데이터를 메모리에 채운다. */
Err db_seed(Db *db, char *err, size_t cap) {
    static const struct {
        const char *title;
        const char *author;
        const char *genre;
    } seed[] = {
        {"Clean Code", "Robert Martin", "SE"},
        {"Clean Architecture", "Robert Martin", "SE"},
        {"The Hobbit", "J.R.R. Tolkien", "Fantasy"},
        {"The Lord of the Rings", "J.R.R. Tolkien", "Fantasy"},
        {"1984", "George Orwell", "Dystopia"},
        {"Animal Farm", "George Orwell", "Dystopia"},
        {"Dune", "Frank Herbert", "SciFi"},
        {"Dune Messiah", "Frank Herbert", "SciFi"},
        {"Refactoring", "Martin Fowler", "SE"},
        {"Patterns of Enterprise", "Martin Fowler", "SE"},
        {"Pride and Prejudice", "Jane Austen", "Classic"},
        {"Emma", "Jane Austen", "Classic"}
    };
    size_t i;
    int id;
    Err res;

    db->len = 0;
    db->next_id = 1;
    bp_free(&db->idx);
    bp_init(&db->idx);
    for (i = 0; i < sizeof(seed) / sizeof(seed[0]); ++i) {
        res = db_add(db, seed[i].title, seed[i].author, seed[i].genre, &id, err,
                     cap);
        if (res != ERR_OK) {
            return res;
        }
    }
    return ERR_OK;
}

/* 요약: 현재 캐시 전체를 BKDB 파일로 안전하게 저장한다. */
Err db_save(Db *db, char *err, size_t cap) {
    FILE *fp;
    DHdr hdr;
    char tmp[PATH_LEN];
    size_t i;
    Err res;

    if (strlen(db->path) + 4 >= sizeof(tmp)) {
        err_set(err, cap, "data path is too long for temp file");
        return ERR_ARG;
    }
    snprintf(tmp, sizeof(tmp), "%s", db->path);
    strcat(tmp, ".tmp");
    fp = fopen(tmp, "wb");
    if (fp == NULL) {
        err_set(err, cap, "failed to create temp data file");
        return ERR_FILE;
    }
    memcpy(hdr.data, "BKDB", 4);
    hdr.ver = 1;
    hdr.rec_sz = (uint16_t)rec_sz();
    hdr.keep = 0;
    hdr.cnt = (uint32_t)db->len;
    hdr.next_id = (uint32_t)db->next_id;
    if (fwrite(&hdr, sizeof(hdr), 1, fp) != 1) {
        fclose(fp);
        err_set(err, cap, "failed to write data header");
        return ERR_IO;
    }
    for (i = 0; i < db->len; ++i) {
        res = write_row(fp, &db->rows[i]);
        if (res != ERR_OK) {
            fclose(fp);
            err_set(err, cap, "failed to write data rows");
            return res;
        }
    }
    if (fclose(fp) != 0) {
        err_set(err, cap, "failed to close temp data file");
        return ERR_IO;
    }
    return swap_file(db->path, tmp, err, cap);
}

/* 요약: BKDB 파일을 읽어 캐시와 인덱스를 준비한다. */
Err db_load(Db *db, char *err, size_t cap) {
    FILE *fp;
    DHdr hdr;
    size_t i;
    Err res;

    fp = fopen(db->path, "rb");
    if (fp == NULL) {
        if (errno == ENOENT) {
            res = db_seed(db, err, cap);
            if (res != ERR_OK) {
                return res;
            }
            return db_save(db, err, cap);
        }
        if (errno == EACCES) {
            err_set(err, cap, "permission denied: %s", db->path);
            return ERR_FILE;
        }
        err_set(err, cap, "failed to open data file");
        return ERR_FILE;
    }
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) {
        fclose(fp);
        err_set(err, cap, "failed to read data header");
        return ERR_FMT;
    }
    if (memcmp(hdr.data, "BKDB", 4) != 0) {
        fclose(fp);
        err_set(err, cap, "bad data magic");
        return ERR_FMT;
    }
    if (hdr.ver != 1) {
        fclose(fp);
        err_set(err, cap, "bad data version: %u", hdr.ver);
        return ERR_FMT;
    }
    if (hdr.rec_sz != (uint16_t)rec_sz()) {
        fclose(fp);
        err_set(err, cap, "bad data record size");
        return ERR_FMT;
    }
    res = check_data_size(fp, &hdr, err, cap);
    if (res != ERR_OK) {
        fclose(fp);
        return res;
    }

    db->len = 0;
    db->next_id = (int)hdr.next_id;
    bp_free(&db->idx);
    bp_init(&db->idx);
    for (i = 0; i < hdr.cnt; ++i) {
        res = grow_rows(db);
        if (res != ERR_OK) {
            fclose(fp);
            err_set(err, cap, "out of memory while loading rows");
            return res;
        }
        res = read_row(fp, &db->rows[db->len]);
        if (res != ERR_OK) {
            fclose(fp);
            err_set(err, cap, "failed to read row %zu", i + 1);
            return ERR_FMT;
        }
        db->len += 1;
    }
    if (fgetc(fp) != EOF) {
        fclose(fp);
        err_set(err, cap, "extra bytes after data rows");
        return ERR_FMT;
    }
    fclose(fp);
    if (db->next_id < 1) {
        err_set(err, cap, "bad next_id in data file");
        return ERR_FMT;
    }
    return db_reidx(db, err, cap);
}
