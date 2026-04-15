#include "storage.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DB_MAGIC "MSQLDB1"
#define DB_MAGIC_LEN 7
#define DB_VERSION 1u
#define MAX_NAME_BYTES (1024u * 1024u)

static bool read_exact(FILE *file, void *buffer, size_t size) {
    return fread(buffer, 1, size, file) == size;
}

static bool write_exact(FILE *file, const void *buffer, size_t size) {
    return fwrite(buffer, 1, size, file) == size;
}

static bool read_u32(FILE *file, uint32_t *value) {
    return read_exact(file, value, sizeof(*value));
}

static bool read_u64(FILE *file, uint64_t *value) {
    return read_exact(file, value, sizeof(*value));
}

static bool read_i64(FILE *file, int64_t *value) {
    return read_exact(file, value, sizeof(*value));
}

static bool write_u32(FILE *file, uint32_t value) {
    return write_exact(file, &value, sizeof(value));
}

static bool write_u64(FILE *file, uint64_t value) {
    return write_exact(file, &value, sizeof(value));
}

static bool write_i64(FILE *file, int64_t value) {
    return write_exact(file, &value, sizeof(value));
}

bool storage_load(const char *path, Database *db, size_t index_order, SqlError *error) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        error_set(error, ERROR_STORAGE, 0, "failed to open DB file '%s': %s", path, strerror(errno));
        return false;
    }

    if (!database_init(db, index_order, error)) {
        fclose(file);
        return false;
    }

    char magic[DB_MAGIC_LEN];
    uint32_t version = 0;
    int64_t next_id = 1;
    uint64_t row_count = 0;

    if (!read_exact(file, magic, sizeof(magic)) || memcmp(magic, DB_MAGIC, DB_MAGIC_LEN) != 0) {
        error_set(error, ERROR_STORAGE, 0, "binary DB magic mismatch");
        goto fail;
    }
    if (!read_u32(file, &version) || version != DB_VERSION) {
        error_set(error, ERROR_STORAGE, 0, "unsupported binary DB version");
        goto fail;
    }
    if (!read_i64(file, &next_id) || next_id < 1) {
        error_set(error, ERROR_STORAGE, 0, "invalid next_id in DB file");
        goto fail;
    }
    if (!read_u64(file, &row_count)) {
        error_set(error, ERROR_STORAGE, 0, "failed to read row count");
        goto fail;
    }

    for (uint64_t i = 0; i < row_count; i++) {
        int64_t id = 0;
        uint64_t name_len = 0;
        int64_t value = 0;

        if (!read_i64(file, &id) || !read_u64(file, &name_len)) {
            error_set(error, ERROR_STORAGE, 0, "failed to read row header");
            goto fail;
        }
        if (name_len > MAX_NAME_BYTES) {
            error_set(error, ERROR_STORAGE, 0, "row name is too large or corrupt");
            goto fail;
        }

        char *name = calloc((size_t)name_len + 1, 1);
        if (!name) {
            error_set(error, ERROR_STORAGE, 0, "failed to allocate row name");
            goto fail;
        }
        if (!read_exact(file, name, (size_t)name_len) || !read_i64(file, &value)) {
            free(name);
            error_set(error, ERROR_STORAGE, 0, "failed to read row payload");
            goto fail;
        }
        if (!table_append_loaded(&db->records, id, name, value, error)) {
            free(name);
            goto fail;
        }
        free(name);
    }

    if (next_id < db->records.next_id) {
        error_set(error, ERROR_STORAGE, 0, "next_id is lower than existing row ids");
        goto fail;
    }

    db->records.next_id = next_id;
    if (!database_rebuild_indexes(db, error)) {
        goto fail;
    }

    fclose(file);
    return true;

fail:
    fclose(file);
    database_free(db);
    return false;
}

bool storage_save(const char *path, const Database *db, SqlError *error) {
    size_t tmp_len = strlen(path) + 5;
    char *tmp_path = malloc(tmp_len);
    if (!tmp_path) {
        error_set(error, ERROR_SAVE, 0, "failed to allocate temp path");
        return false;
    }
    snprintf(tmp_path, tmp_len, "%s.tmp", path);

    FILE *file = fopen(tmp_path, "wb");
    if (!file) {
        error_set(error, ERROR_SAVE, 0, "failed to open temp DB file '%s': %s", tmp_path, strerror(errno));
        free(tmp_path);
        return false;
    }

    const Table *table = &db->records;
    bool ok =
        write_exact(file, DB_MAGIC, DB_MAGIC_LEN) &&
        write_u32(file, DB_VERSION) &&
        write_i64(file, table->next_id) &&
        write_u64(file, (uint64_t)table->row_count);

    for (size_t i = 0; ok && i < table->row_count; i++) {
        const Record *row = &table->rows[i];
        uint64_t name_len = (uint64_t)strlen(row->name);
        ok =
            write_i64(file, row->id) &&
            write_u64(file, name_len) &&
            write_exact(file, row->name, (size_t)name_len) &&
            write_i64(file, row->value);
    }

    if (fclose(file) != 0) {
        ok = false;
    }

    if (!ok) {
        error_set(error, ERROR_SAVE, 0, "failed to write complete DB file");
        remove(tmp_path);
        free(tmp_path);
        return false;
    }

    if (rename(tmp_path, path) != 0) {
        error_set(error, ERROR_SAVE, 0, "failed to replace DB file '%s': %s", path, strerror(errno));
        remove(tmp_path);
        free(tmp_path);
        return false;
    }

    free(tmp_path);
    return true;
}
