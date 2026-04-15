/* This file runs focused unit tests for parser, storage, and rollback code. */
#include "sql2.h"

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

/* 요약: 테스트 중간에 생긴 임시 파일들을 지운다. */
static void clean_file(const char *path) {
    char tmp[PATH_LEN];
    char bak[PATH_LEN];

    remove(path);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    snprintf(bak, sizeof(bak), "%s.bak", path);
    remove(tmp);
    remove(bak);
}

/* 요약: 문자열 안 세미콜론을 포함한 정상 분리를 검사한다. */
static int t_split_ok(void) {
    StmtList lst;
    char err[256];
    Err res;

    res = split_sql("SELECT * FROM books WHERE title = 'A;B'; INSERT INTO books "
                    "VALUES ('C','D','E');",
                    &lst, err, sizeof(err));
    CHECK(res == ERR_OK);
    CHECK(lst.len == 2);
    CHECK(strstr(lst.list[0].txt, "A;B") != NULL);
    CHECK(strstr(lst.list[1].txt, "INSERT") != NULL);
    free_stmts(&lst);
    return 1;
}

/* 요약: 비어 있는 문장이 오류로 잡히는지 검사한다. */
static int t_split_empty_stmt(void) {
    StmtList lst;
    char err[256];
    Err res;

    res = split_sql("SELECT * FROM books;;", &lst, err, sizeof(err));
    CHECK(res == ERR_INPUT);
    return 1;
}

/* 요약: SELECT 문이 올바른 쿼리 구조로 파싱되는지 본다. */
static int t_lex_parse_select(void) {
    TokList toks;
    Qry qry;
    char err[256];
    Err res;

    res = lex_stmt("SELECT title,author FROM books WHERE id = 7", &toks, err,
                   sizeof(err));
    CHECK(res == ERR_OK);
    res = parse_stmt(&toks, &qry, err, sizeof(err));
    CHECK(res == ERR_OK);
    CHECK(qry.kind == Q_SELECT);
    CHECK(qry.cols.len == 2);
    CHECK(strcmp(qry.cols.cols[0], "title") == 0);
    CHECK(qry.cond.used == 1);
    CHECK(strcmp(qry.cond.col, "id") == 0);
    CHECK(qry.cond.val.kind == VAL_INT);
    CHECK(qry.cond.val.num == 7);
    free_toks(&toks);
    return 1;
}

/* 요약: INSERT 문이 올바른 값 목록으로 파싱되는지 본다. */
static int t_lex_parse_insert(void) {
    TokList toks;
    Qry qry;
    char err[256];
    Err res;

    res = lex_stmt("INSERT INTO books VALUES ('A','B','C')", &toks, err,
                   sizeof(err));
    CHECK(res == ERR_OK);
    res = parse_stmt(&toks, &qry, err, sizeof(err));
    CHECK(res == ERR_OK);
    CHECK(qry.kind == Q_INSERT);
    CHECK(qry.nval == 3);
    CHECK(qry.vals[0].kind == VAL_STR);
    CHECK(strcmp(qry.vals[1].str, "B") == 0);
    free_toks(&toks);
    return 1;
}

/* 요약: B+ 트리 삽입과 단건 검색이 맞는지 확인한다. */
static int t_bp_tree(void) {
    BpTree tree;
    int i;
    int val;

    bp_init(&tree);
    for (i = 1; i <= 200; ++i) {
        CHECK(bp_put(&tree, i, i * 10) == ERR_OK);
    }
    for (i = 1; i <= 200; ++i) {
        CHECK(bp_get(&tree, i, &val) == 1);
        CHECK(val == i * 10);
    }
    CHECK(bp_get(&tree, 9999, &val) == 0);
    bp_free(&tree);
    return 1;
}

/* 요약: QSQL 저장과 다시 읽기가 같은지 확인한다. */
static int t_qsql_roundtrip(void) {
    char err[256];
    char *sql;
    const char *path = "data/test_query.qsql";
    Err res;

    clean_file(path);
    res = save_qsql(path, "SELECT * FROM books;", err, sizeof(err));
    CHECK(res == ERR_OK);
    res = load_sql_file(path, &sql, err, sizeof(err));
    CHECK(res == ERR_OK);
    CHECK(strcmp(sql, "SELECT * FROM books;") == 0);
    free(sql);
    clean_file(path);
    return 1;
}

/* 요약: 데이터 저장 후 다시 로드해도 값이 유지되는지 본다. */
static int t_db_save_load(void) {
    Db a;
    Db b;
    char err[256];
    const char *path = "data/test_store.bin";
    Err res;

    clean_file(path);
    db_init(&a);
    db_init(&b);
    CHECK(db_set_path(&a, path, err, sizeof(err)) == ERR_OK);
    CHECK(db_seed(&a, err, sizeof(err)) == ERR_OK);
    CHECK(db_save(&a, err, sizeof(err)) == ERR_OK);

    CHECK(db_set_path(&b, path, err, sizeof(err)) == ERR_OK);
    res = db_load(&b, err, sizeof(err));
    CHECK(res == ERR_OK);
    CHECK(b.len == a.len);
    CHECK(b.next_id == a.next_id);
    CHECK(strcmp(b.rows[0].title, "Clean Code") == 0);

    db_free(&a);
    db_free(&b);
    clean_file(path);
    return 1;
}

/* 요약: 손상된 데이터 헤더를 안전하게 막는지 본다. */
static int t_bad_data_hdr(void) {
    Db db;
    FILE *fp;
    char err[256];
    const char *path = "data/test_bad_hdr.bin";

    clean_file(path);
    fp = fopen(path, "wb");
    CHECK(fp != NULL);
    CHECK(fwrite("NOPE", 1, 4, fp) == 4);
    fclose(fp);

    db_init(&db);
    CHECK(db_set_path(&db, path, err, sizeof(err)) == ERR_OK);
    CHECK(db_load(&db, err, sizeof(err)) == ERR_FMT);
    db_free(&db);
    clean_file(path);
    return 1;
}

/* 요약: 실패한 배치가 길이와 데이터 상태를 되돌리는지 본다. */
static int t_rollback(void) {
    Db db;
    StrBuf out;
    char err[256];
    const char *path = "data/test_roll.bin";
    size_t len;

    clean_file(path);
    db_init(&db);
    CHECK(db_set_path(&db, path, err, sizeof(err)) == ERR_OK);
    CHECK(db_load(&db, err, sizeof(err)) == ERR_OK);
    len = db.len;

    sb_init(&out);
    CHECK(run_batch(&db,
                    "INSERT INTO books VALUES ('RB','Who','Tmp'); SELECT * FROM "
                    "nope;",
                    0, &out, err, sizeof(err)) == ERR_EXEC);
    CHECK(db.len == len);
    CHECK(run_batch(&db, "SELECT * FROM books WHERE author = 'Who';", 0, &out,
                    err, sizeof(err)) == ERR_OK);
    CHECK(strstr(out.buf, "(no rows)") != NULL);

    sb_free(&out);
    db_free(&db);
    clean_file(path);
    return 1;
}

/* 요약: 단위 테스트 묶음을 차례로 실행한다. */
int main(void) {
    RUN(t_split_ok);
    RUN(t_split_empty_stmt);
    RUN(t_lex_parse_select);
    RUN(t_lex_parse_insert);
    RUN(t_bp_tree);
    RUN(t_qsql_roundtrip);
    RUN(t_db_save_load);
    RUN(t_bad_data_hdr);
    RUN(t_rollback);
    printf("unit: pass=%d fail=%d\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
