# 클래스 다이어그램과 요청 흐름

이 문서는 `books` SQL 데모가 사용자 요청을 어떻게 받아서 처리하는지, 각 구조체가
어떻게 연결되는지, 그리고 어떤 데이터를 주고받는지 한 번에 보이도록 다시 정리한
그림이다.

## 요청 처리 흐름

```mermaid
flowchart TD
    U["User Request\nCLI batch or file path"]
    M["main.c\nmain"]
    O["Opts\nmode, batch, file, data, sum_only"]
    B["batch.c\nsplit_sql / load_sql_file"]
    SL["StmtList\nStmt[]"]
    L["lex.c\nlex_stmt"]
    TL["TokList\nTok[]"]
    P["parse.c\nparse_stmt"]
    Q["Qry\nSELECT or INSERT"]
    E["exec.c\nrun_qry / run_batch"]
    RS["RowSet\nmatched row indexes\nscan kind / time"]
    DB["Db\nrows / next_id / path / idx"]
    BT["BpTree\nid index"]
    DF[(BKDB data file)]
    QF[(SQL or QSQL file)]
    OUT["StrBuf\nbuffered output"]

    U --> M
    M --> O
    O -->|batch text or file mode| B
    O -->|data path| DB
    QF -->|SQL text| B
    B -->|split statements| SL
    SL -->|one statement text| L
    L -->|tokens| TL
    TL -->|parsed query| P
    P -->|Qry| Q
    Q --> E
    DB -->|books rows| E
    BT -->|id lookup result| E
    E -->|selected row indexes| RS
    E -->|formatted result| OUT
    E -->|INSERT updates rows and id index| DB
    DB -->|rebuild / lookup| BT
    DB -->|load / save| DF
```

## 핵심 데이터 이동

```mermaid
flowchart LR
    A["SQL text"] --> B["StmtList"]
    B --> C["TokList"]
    C --> D["Qry"]
    D --> E["run_qry"]

    E --> F["SELECT"]
    E --> G["INSERT"]

    F --> H["RowSet"]
    H --> I["StrBuf output"]

    G --> J["Book row"]
    J --> K["Db.rows"]
    J --> L["BpTree id -> row index"]
    K --> M["BKDB file"]
```

## 구조체 관계

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

    class RowSet {
        list
        len
        cap
        scan
        ms
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

    Opts ..> StmtList : input source decides batch
    StmtList *-- Stmt : contains
    TokList *-- Tok : contains
    Cond *-- Val : where value
    Qry *-- ColList : select columns
    Qry *-- Cond : where clause
    Qry *-- Val : insert values
    Db *-- Book : stores rows
    Db *-- BpTree : owns id index
    BpTree *-- BpNode : tree nodes
    BpNode --> BpNode : leaf next
    RowSet ..> Db : row indexes refer to rows
    RowSet ..> Qry : result of SELECT
    QHdr ..> StmtList : query file header
    DHdr ..> Db : data file header
    StrBuf ..> Qry : renders query result
```

## 읽는 순서

1. 사용자 요청이 `Opts`로 정리되고 `StmtList -> TokList -> Qry`로 바뀌는 흐름을 본다.
2. `Qry`가 `run_qry`로 들어가서 `SELECT`면 `RowSet`, `INSERT`면 `Db` 변경으로 가는
   흐름을 본다.
3. `Db`가 `Book[]`와 `BpTree`를 함께 들고 있고, 저장 시 `DHdr + row data`로 파일에
   기록된다는 점을 본다.

## 핵심 해석 포인트

- 사용자 입력의 중심 데이터는 `SQL text`이고, 실행 직전의 중심 데이터는 `Qry`다.
- 조회 실행의 중심 데이터는 `RowSet`이다. 실제 `Book` 복사본이 아니라 행 인덱스와
  탐색 요약을 담는다.
- 저장 계층의 중심 데이터는 `Db`다. 메모리 캐시 `rows`와 `id` 전용 인덱스 `BpTree`
  를 함께 관리한다.
- 파일 계층은 두 종류다.
  `QHdr`는 `QSQL` 배치 파일용, `DHdr`는 `BKDB` 데이터 파일용이다.
