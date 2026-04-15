#include "parser.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static char *string_duplicate_range(const char *start, size_t len) {
    char *copy = malloc(len + 1);
    if (!copy) {
        return NULL;
    }
    memcpy(copy, start, len);
    copy[len] = '\0';
    return copy;
}

static bool is_identifier_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static void skip_spaces(const char **p) {
    while (isspace((unsigned char)**p)) {
        (*p)++;
    }
}

static bool keyword_matches(const char *p, const char *keyword) {
    size_t len = strlen(keyword);
    for (size_t i = 0; i < len; i++) {
        if (tolower((unsigned char)p[i]) != tolower((unsigned char)keyword[i])) {
            return false;
        }
    }
    return !is_identifier_char(p[len]);
}

static bool consume_keyword(const char **p, const char *keyword) {
    skip_spaces(p);
    if (!keyword_matches(*p, keyword)) {
        return false;
    }
    *p += strlen(keyword);
    return true;
}

static bool consume_char(const char **p, char value) {
    skip_spaces(p);
    if (**p != value) {
        return false;
    }
    (*p)++;
    return true;
}

static char *parse_identifier(const char **p) {
    skip_spaces(p);
    if (!isalpha((unsigned char)**p) && **p != '_') {
        return NULL;
    }
    const char *start = *p;
    while (is_identifier_char(**p)) {
        (*p)++;
    }
    return string_duplicate_range(start, (size_t)(*p - start));
}

static bool equals_ignore_case(const char *left, const char *right) {
    while (*left && *right) {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return false;
        }
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static bool parse_int64(const char **p, int64_t *out) {
    skip_spaces(p);
    errno = 0;
    char *end = NULL;
    long long value = strtoll(*p, &end, 10);
    if (end == *p || errno == ERANGE) {
        return false;
    }
    *p = end;
    *out = (int64_t)value;
    return true;
}

static bool parse_single_quoted_string(const char **p, char **out) {
    skip_spaces(p);
    if (**p != '\'') {
        return false;
    }
    (*p)++;
    const char *start = *p;
    while (**p && **p != '\'') {
        (*p)++;
    }
    if (**p != '\'') {
        return false;
    }
    *out = string_duplicate_range(start, (size_t)(*p - start));
    (*p)++;
    return *out != NULL;
}

static bool at_end(const char *p) {
    skip_spaces(&p);
    return *p == '\0';
}

void query_batch_init(QueryBatch *batch) {
    batch->items = NULL;
    batch->count = 0;
    batch->capacity = 0;
}

static void query_free(Query *query) {
    free(query->insert_name);
    query->insert_name = NULL;
}

void query_batch_free(QueryBatch *batch) {
    if (!batch) {
        return;
    }
    for (size_t i = 0; i < batch->count; i++) {
        query_free(&batch->items[i]);
    }
    free(batch->items);
    batch->items = NULL;
    batch->count = 0;
    batch->capacity = 0;
}

static bool query_batch_append(QueryBatch *batch, Query query, SqlError *error) {
    if (batch->count == batch->capacity) {
        size_t next_capacity = batch->capacity == 0 ? 4 : batch->capacity * 2;
        Query *next_items = realloc(batch->items, next_capacity * sizeof(Query));
        if (!next_items) {
            query_free(&query);
            error_set(error, ERROR_PARSE, 0, "failed to grow query batch");
            return false;
        }
        batch->items = next_items;
        batch->capacity = next_capacity;
    }
    batch->items[batch->count++] = query;
    return true;
}

static bool parse_records_table(const char **p, size_t statement_index, SqlError *error) {
    char *table = parse_identifier(p);
    if (!table) {
        error_set(error, ERROR_PARSE, statement_index, "expected table name");
        return false;
    }
    bool ok = equals_ignore_case(table, "records");
    free(table);
    if (!ok) {
        error_set(error, ERROR_PARSE, statement_index, "only the records table is supported");
        return false;
    }
    return true;
}

static bool parse_select(const char *statement, Query *query, size_t statement_index, SqlError *error) {
    const char *p = statement;
    if (!consume_keyword(&p, "SELECT") || !consume_char(&p, '*') || !consume_keyword(&p, "FROM")) {
        error_set(error, ERROR_PARSE, statement_index, "expected SELECT * FROM records");
        return false;
    }
    if (!parse_records_table(&p, statement_index, error)) {
        return false;
    }

    if (at_end(p)) {
        query->type = QUERY_SELECT_ALL;
        return true;
    }

    if (!consume_keyword(&p, "WHERE")) {
        error_set(error, ERROR_PARSE, statement_index, "expected WHERE clause or end of statement");
        return false;
    }

    char *column = parse_identifier(&p);
    if (!column) {
        error_set(error, ERROR_PARSE, statement_index, "expected WHERE column");
        return false;
    }

    if (equals_ignore_case(column, "id")) {
        free(column);
        if (consume_char(&p, '=')) {
            int64_t id = 0;
            if (!parse_int64(&p, &id) || !at_end(p)) {
                error_set(error, ERROR_PARSE, statement_index, "expected numeric id after '='");
                return false;
            }
            query->type = QUERY_SELECT_ID_EQ;
            query->id = id;
            return true;
        }
        if (consume_keyword(&p, "BETWEEN")) {
            int64_t min_id = 0;
            int64_t max_id = 0;
            if (!parse_int64(&p, &min_id) || !consume_keyword(&p, "AND") || !parse_int64(&p, &max_id) || !at_end(p)) {
                error_set(error, ERROR_PARSE, statement_index, "expected id BETWEEN A AND B");
                return false;
            }
            query->type = QUERY_SELECT_ID_RANGE;
            query->min_id = min_id;
            query->max_id = max_id;
            return true;
        }
        error_set(error, ERROR_PARSE, statement_index, "expected '=' or BETWEEN after id");
        return false;
    }

    if (equals_ignore_case(column, "value")) {
        free(column);
        int64_t value = 0;
        if (!consume_char(&p, '=') || !parse_int64(&p, &value) || !at_end(p)) {
            error_set(error, ERROR_PARSE, statement_index, "expected value = number");
            return false;
        }
        query->type = QUERY_SELECT_VALUE_EQ;
        query->value = value;
        return true;
    }

    free(column);
    error_set(error, ERROR_PARSE, statement_index, "only id and value WHERE columns are supported");
    return false;
}

static bool parse_insert(const char *statement, Query *query, size_t statement_index, SqlError *error) {
    const char *p = statement;
    if (!consume_keyword(&p, "INSERT") || !consume_keyword(&p, "INTO")) {
        error_set(error, ERROR_PARSE, statement_index, "expected INSERT INTO records VALUES (...)");
        return false;
    }
    if (!parse_records_table(&p, statement_index, error)) {
        return false;
    }
    if (!consume_keyword(&p, "VALUES") || !consume_char(&p, '(')) {
        error_set(error, ERROR_PARSE, statement_index, "expected VALUES (name, value)");
        return false;
    }

    char *name = NULL;
    int64_t value = 0;
    if (!parse_single_quoted_string(&p, &name) || !consume_char(&p, ',') || !parse_int64(&p, &value) || !consume_char(&p, ')') || !at_end(p)) {
        free(name);
        error_set(error, ERROR_PARSE, statement_index, "expected INSERT values ('name', number)");
        return false;
    }

    query->type = QUERY_INSERT;
    query->insert_name = name;
    query->insert_value = value;
    return true;
}

static bool parse_statement(const char *statement, Query *query, size_t statement_index, SqlError *error) {
    memset(query, 0, sizeof(*query));
    const char *p = statement;
    skip_spaces(&p);

    if (keyword_matches(p, "SELECT")) {
        return parse_select(statement, query, statement_index, error);
    }
    if (keyword_matches(p, "INSERT")) {
        return parse_insert(statement, query, statement_index, error);
    }

    error_set(error, ERROR_PARSE, statement_index, "unsupported SQL statement");
    return false;
}

bool parse_batch(const char *sql, QueryBatch *batch, SqlError *error) {
    query_batch_init(batch);
    const char *p = sql;
    size_t statement_index = 1;
    bool saw_statement = false;

    while (true) {
        skip_spaces(&p);
        if (*p == '\0') {
        if (!saw_statement) {
                error_set(error, ERROR_PARSE, 1, "empty SQL input");
                query_batch_free(batch);
                return false;
            }
            return true;
        }

        const char *semi = strchr(p, ';');
        if (!semi) {
            error_set(error, ERROR_PARSE, statement_index, "missing semicolon");
            query_batch_free(batch);
            return false;
        }

        const char *start = p;
        const char *end = semi;
        while (end > start && isspace((unsigned char)*(end - 1))) {
            end--;
        }
        if (end == start) {
            error_set(error, ERROR_PARSE, statement_index, "empty statement");
            query_batch_free(batch);
            return false;
        }

        char *statement = string_duplicate_range(start, (size_t)(end - start));
        if (!statement) {
            error_set(error, ERROR_PARSE, statement_index, "failed to copy statement");
            query_batch_free(batch);
            return false;
        }

        Query query;
        bool ok = parse_statement(statement, &query, statement_index, error);
        free(statement);
        if (!ok) {
            query_batch_free(batch);
            return false;
        }
        if (!query_batch_append(batch, query, error)) {
            query_batch_free(batch);
            return false;
        }

        saw_statement = true;
        statement_index++;
        p = semi + 1;
    }
}
