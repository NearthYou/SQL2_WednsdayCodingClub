/* This file runs higher-level functional tests against batch execution. */
#include "app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pass = 0;
static int fail = 0;

#define CHECK(x)                                                               \
    do {                                                                       \
        if (!(x)) {                                                            \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);       \
            return 0;                                                          \
        }                                                                      \
    } while (0)

#define RUN(fn)                                                                \
    do {                                                                       \
        if (fn()) {                                                            \
            ++pass;                                                            \
            printf("ok - %s\n", #fn);                                          \
        } else {                                                               \
            ++fail;                                                            \
        }                                                                      \
    } while (0)

/* 요약: 기능 테스트 중간에 생긴 임시 파일을 지운다. */
static void clean_file(const char *path) {
    char tmp[PATH_LEN];
    char bak[PATH_LEN];

    remove(path);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    snprintf(bak, sizeof(bak), "%s.bak", path);
    remove(tmp);
    remove(bak);
}

/* 요약: 기능 테스트용 데이터 파일과 캐시를 준비한다. */
static int prep_db(Db *db, const char *path) {
    char err[256];

    clean_file(path);
    db_init(db);
    CHECK(db_set_path(db, path, err, sizeof(err)) == ERR_OK);
    CHECK(db_load(db, err, sizeof(err)) == ERR_OK);
    return 1;
}

/* 요약: id 조건 조회가 B+ 트리를 타는지 확인한다. */
static int t_select_id(void) {
    Db db;
    StrBuf out;
    char err[256];

    CHECK(prep_db(&db, "data/test_func_1.bin"));
    sb_init(&out);
    CHECK(run_batch(&db, "SELECT * FROM books WHERE id = 1;", 0, &out, err,
                    sizeof(err)) == ERR_OK);
    CHECK(strstr(out.buf, "Clean Code") != NULL);
    CHECK(strstr(out.buf, "scan=B+Tree") != NULL);
    sb_free(&out);
    db_free(&db);
    clean_file("data/test_func_1.bin");
    return 1;
}

/* 요약: id 범위 조회가 B+ 트리로 처리되고 양 끝을 포함하는지 본다. */
static int t_select_id_range(void) {
    Db db;
    StrBuf out;
    char err[256];

    CHECK(prep_db(&db, "data/test_func_range.bin"));
    sb_init(&out);
    CHECK(run_batch(&db, "SELECT * FROM books WHERE id BETWEEN 2 AND 4;", 0,
                    &out, err, sizeof(err)) == ERR_OK);
    CHECK(strstr(out.buf, "Clean Architecture") != NULL);
    CHECK(strstr(out.buf, "The Hobbit") != NULL);
    CHECK(strstr(out.buf, "The Lord of the Rings") != NULL);
    CHECK(strstr(out.buf, "rows=3") != NULL);
    CHECK(strstr(out.buf, "scan=B+Tree") != NULL);
    sb_free(&out);
    db_free(&db);
    clean_file("data/test_func_range.bin");
    return 1;
}

/* 요약: INSERT 뒤 같은 배치에서 다시 조회할 수 있는지 본다. */
static int t_insert_and_select(void) {
    Db db;
    StrBuf out;
    char err[256];

    CHECK(prep_db(&db, "data/test_func_2.bin"));
    sb_init(&out);
    CHECK(run_batch(&db,
                    "INSERT INTO books VALUES ('Book X','Author X','Genre X'); "
                    "SELECT * FROM books WHERE author = 'Author X';",
                    0, &out, err, sizeof(err)) == ERR_OK);
    CHECK(strstr(out.buf, "inserted id=") != NULL);
    CHECK(strstr(out.buf, "Book X") != NULL);
    sb_free(&out);
    db_free(&db);
    clean_file("data/test_func_2.bin");
    return 1;
}

/* 요약: 없는 테이블 이름을 명확히 거절하는지 본다. */
static int t_bad_table(void) {
    Db db;
    StrBuf out;
    char err[256];

    CHECK(prep_db(&db, "data/test_func_3.bin"));
    sb_init(&out);
    CHECK(run_batch(&db, "SELECT * FROM nope;", 0, &out, err, sizeof(err)) ==
          ERR_EXEC);
    CHECK(strstr(err, "unknown table") != NULL);
    sb_free(&out);
    db_free(&db);
    clean_file("data/test_func_3.bin");
    return 1;
}

/* 요약: 없는 컬럼 이름을 명확히 거절하는지 본다. */
static int t_bad_col(void) {
    Db db;
    StrBuf out;
    char err[256];

    CHECK(prep_db(&db, "data/test_func_4.bin"));
    sb_init(&out);
    CHECK(run_batch(&db, "SELECT nope FROM books;", 0, &out, err, sizeof(err)) ==
          ERR_EXEC);
    CHECK(strstr(err, "unknown column") != NULL);
    sb_free(&out);
    db_free(&db);
    clean_file("data/test_func_4.bin");
    return 1;
}

/* 요약: 결과가 없을 때 안내 문구가 나오는지 본다. */
static int t_zero_rows(void) {
    Db db;
    StrBuf out;
    char err[256];

    CHECK(prep_db(&db, "data/test_func_5.bin"));
    sb_init(&out);
    CHECK(run_batch(&db, "SELECT * FROM books WHERE genre = 'Nope';", 0, &out,
                    err, sizeof(err)) == ERR_OK);
    CHECK(strstr(out.buf, "(no rows)") != NULL);
    CHECK(strstr(out.buf, "rows=0") != NULL);
    sb_free(&out);
    db_free(&db);
    clean_file("data/test_func_5.bin");
    return 1;
}

/* 요약: summary 전용 출력이 표 대신 요약만 보여주는지 본다. */
static int t_summary_only(void) {
    Db db;
    StrBuf out;
    char err[256];

    CHECK(prep_db(&db, "data/test_func_8.bin"));
    sb_init(&out);
    CHECK(run_batch(&db, "SELECT * FROM books WHERE genre = 'SE';", 1, &out,
                    err, sizeof(err)) == ERR_OK);
    CHECK(strstr(out.buf, "[genre lookup]") != NULL);
    CHECK(strstr(out.buf, "rows : 4") != NULL);
    CHECK(strstr(out.buf, "scan : Linear") != NULL);
    CHECK(strstr(out.buf, "time : ") != NULL);
    CHECK(strstr(out.buf, "| id |") == NULL);
    sb_free(&out);
    db_free(&db);
    clean_file("data/test_func_8.bin");
    return 1;
}

/* 요약: summary-only 범위 조회가 B+ 트리 요약만 출력하는지 본다. */
static int t_summary_id_range(void) {
    Db db;
    StrBuf out;
    char err[256];

    CHECK(prep_db(&db, "data/test_func_range_sum.bin"));
    sb_init(&out);
    CHECK(run_batch(&db, "SELECT * FROM books WHERE id BETWEEN 3 AND 5;", 1,
                    &out, err, sizeof(err)) == ERR_OK);
    CHECK(strstr(out.buf, "[id range lookup]") != NULL);
    CHECK(strstr(out.buf, "rows : 3") != NULL);
    CHECK(strstr(out.buf, "scan : B+Tree") != NULL);
    CHECK(strstr(out.buf, "| id |") == NULL);
    sb_free(&out);
    db_free(&db);
    clean_file("data/test_func_range_sum.bin");
    return 1;
}

/* 요약: 마지막 세미콜론 누락을 입력 오류로 잡는지 본다. */
static int t_missing_semi(void) {
    Db db;
    StrBuf out;
    char err[256];

    CHECK(prep_db(&db, "data/test_func_6.bin"));
    sb_init(&out);
    CHECK(run_batch(&db, "SELECT * FROM books", 0, &out, err, sizeof(err)) ==
          ERR_INPUT);
    CHECK(strstr(err, "end with ';'") != NULL);
    sb_free(&out);
    db_free(&db);
    clean_file("data/test_func_6.bin");
    return 1;
}

/* 요약: INSERT 값 개수 오류를 실행 단계에서 막는지 본다. */
static int t_bad_insert_count(void) {
    Db db;
    StrBuf out;
    char err[256];

    CHECK(prep_db(&db, "data/test_func_7.bin"));
    sb_init(&out);
    CHECK(run_batch(&db, "INSERT INTO books VALUES ('A','B');", 0, &out, err,
                    sizeof(err)) == ERR_EXEC);
    CHECK(strstr(err, "needs 3 values") != NULL);
    sb_free(&out);
    db_free(&db);
    clean_file("data/test_func_7.bin");
    return 1;
}

/* 요약: 잘못된 id 범위는 실행 단계에서 거절되고 롤백된다. */
static int t_bad_id_range(void) {
    Db db;
    StrBuf out;
    char err[256];

    CHECK(prep_db(&db, "data/test_func_bad_range.bin"));
    sb_init(&out);
    CHECK(run_batch(&db,
                    "INSERT INTO books VALUES ('RB','Range','Tmp'); SELECT * "
                    "FROM books WHERE id BETWEEN 9 AND 1;",
                    0, &out, err, sizeof(err)) == ERR_EXEC);
    CHECK(strstr(err, "range start must be <=") != NULL);
    CHECK(run_batch(&db, "SELECT * FROM books WHERE author = 'Range';", 0, &out,
                    err, sizeof(err)) == ERR_OK);
    CHECK(strstr(out.buf, "(no rows)") != NULL);
    sb_free(&out);
    db_free(&db);
    clean_file("data/test_func_bad_range.bin");
    return 1;
}

/* 요약: BETWEEN 이 id 외 컬럼에 쓰이면 명확히 거절된다. */
static int t_between_non_id(void) {
    Db db;
    StrBuf out;
    char err[256];

    CHECK(prep_db(&db, "data/test_func_bad_between.bin"));
    sb_init(&out);
    CHECK(run_batch(&db, "SELECT * FROM books WHERE author BETWEEN 1 AND 2;", 0,
                    &out, err, sizeof(err)) == ERR_EXEC);
    CHECK(strstr(err, "BETWEEN is only supported for id") != NULL);
    sb_free(&out);
    db_free(&db);
    clean_file("data/test_func_bad_between.bin");
    return 1;
}

/* 요약: 기본 경로 텍스트 SQL 로딩이 동작하는지 본다. */
static int t_file_text_and_default(void) {
    char err[256];
    char *sql;
    FILE *fp;

    remove("data/input.qsql");
    fp = fopen("data/input.sql", "wb");
    CHECK(fp != NULL);
    CHECK(fwrite("SELECT * FROM books WHERE id = 2;", 1, 33, fp) == 33);
    fclose(fp);

    CHECK(load_default_sql(&sql, err, sizeof(err)) == ERR_OK);
    CHECK(strstr(sql, "id = 2") != NULL);
    free(sql);
    remove("data/input.sql");
    return 1;
}

/* 요약: 기본 경로 QSQL 로딩이 우선 동작하는지 본다. */
static int t_file_qsql(void) {
    char err[256];
    char *sql;

    remove("data/input.sql");
    remove("data/input.qsql");
    CHECK(save_qsql("data/input.qsql", "SELECT * FROM books WHERE id = 3;", err,
                    sizeof(err)) == ERR_OK);
    CHECK(load_default_sql(&sql, err, sizeof(err)) == ERR_OK);
    CHECK(strstr(sql, "id = 3") != NULL);
    free(sql);
    remove("data/input.qsql");
    return 1;
}

/* 요약: 기능 테스트 묶음을 차례로 실행한다. */
int main(void) {
    RUN(t_select_id);
    RUN(t_select_id_range);
    RUN(t_insert_and_select);
    RUN(t_bad_table);
    RUN(t_bad_col);
    RUN(t_zero_rows);
    RUN(t_summary_only);
    RUN(t_summary_id_range);
    RUN(t_missing_semi);
    RUN(t_bad_insert_count);
    RUN(t_bad_id_range);
    RUN(t_between_non_id);
    RUN(t_file_text_and_default);
    RUN(t_file_qsql);
    printf("func: pass=%d fail=%d\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
