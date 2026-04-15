#ifndef MINISQL_PARSER_H
#define MINISQL_PARSER_H

#include "error.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    QUERY_SELECT_ALL = 0,
    QUERY_SELECT_ID_EQ,
    QUERY_SELECT_ID_RANGE,
    QUERY_SELECT_VALUE_EQ,
    QUERY_INSERT
} QueryType;

typedef struct {
    QueryType type;
    int64_t id;
    int64_t min_id;
    int64_t max_id;
    int64_t value;
    char *insert_name;
    int64_t insert_value;
} Query;

typedef struct {
    Query *items;
    size_t count;
    size_t capacity;
} QueryBatch;

void query_batch_init(QueryBatch *batch);
bool parse_batch(const char *sql, QueryBatch *batch, SqlError *error);
void query_batch_free(QueryBatch *batch);

#endif
