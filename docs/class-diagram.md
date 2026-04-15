# 클래스 다이어그램

이 문서는 머메이드 렌더러에서 바로 그림으로 보이도록, 실제 구조체를
렌더 안정성이 높은 형태로 단순화한 클래스 다이어그램입니다.

```mermaid
classDiagram
    class Opts {
        mode
        batch
        file
        data
        help
        sum_only
    }

    class StrBuf {
        buf
        len
        cap
    }

    class Stmt {
        txt
        start
        no
    }

    class StmtList {
        list
        len
        cap
    }

    class Tok {
        kind
        kw
        txt
        num
        pos
    }

    class TokList {
        list
        len
        cap
    }

    class Val {
        kind
        num
        str
    }

    class Cond {
        used
        col
        val
    }

    class ColList {
        all
        len
        cols
    }

    class Qry {
        kind
        table
        cols
        cond
        nval
        vals
        pos
    }

    class Book {
        id
        title
        author
        genre
    }

    class BpNode {
        leaf
        nkey
        keys
        vals
        kid
        next
    }

    class BpTree {
        root
    }

    class Db {
        rows
        len
        cap
        next_id
        path
        idx
    }

    class RowSet {
        list
        len
        cap
        scan
        ms
    }

    class QHdr {
        data
        ver
        len
    }

    class DHdr {
        data
        ver
        rec_sz
        keep
        cnt
        next_id
    }

    StmtList *-- Stmt : contains
    TokList *-- Tok : contains
    Cond *-- Val : where value
    Qry *-- ColList : select cols
    Qry *-- Cond : where
    Qry *-- Val : insert values
    Db *-- Book : rows
    Db *-- BpTree : id index
    BpTree *-- BpNode : root
    BpNode --> BpNode : next
    RowSet ..> Db : row indexes
    RowSet ..> Qry : result of
    QHdr ..> StmtList : query file
    DHdr ..> Db : data file
    Opts ..> StrBuf : output option
```

## 읽는 순서 추천
1. `StmtList -> TokList -> Qry`
2. `Db -> Book -> BpTree`
3. `RowSet`
4. `QHdr / DHdr`

## 핵심 포인트
- `Qry`는 parser가 만든 내부 쿼리 표현입니다.
- `Db`는 메모리 캐시와 `id` 전용 B+ 트리를 함께 들고 있습니다.
- `RowSet`은 실제 행 복사본이 아니라, 캐시 행 인덱스와 탐색 결과 요약입니다.
- `QHdr`와 `DHdr`는 각각 query/data 바이너리 파일 헤더입니다.
