<img width="400" height="400" alt="image" src="https://github.com/user-attachments/assets/327b1c2f-ea0f-47e5-b412-9cab0476266c" />

# B+Tree SQL 데모

> `books` 단일 테이블을 대상으로,  
> `id` 조건은 **B+Tree**, 그 외 조건은 **Linear Scan**으로 처리하는 미니 SQL 엔진입니다.

---

## 핵심 요약

| 내용 |
| --- |
| `SELECT`, `INSERT`, 배치 실행, 롤백, 바이너리 저장 |
| `WHERE id = ...`, `WHERE id BETWEEN ... AND ...`는 B+Tree 사용 |
| **같은 1,000건 조회도 B+Tree가 Linear보다 약 4,900배 빠름** |

### 성능 하이라이트

| 쿼리 | rows | scan | time |
| --- | ---: | --- | ---: |
| `WHERE id = 1000000` | 1 | `B+Tree` | `0.001 ms` |
| `WHERE id BETWEEN 999001 AND 1000000` | 1000 | `B+Tree` | `0.021 ms` |
| `WHERE author = 'Author 999'` | 1000 | `Linear` | `103.440 ms` |

> 같은 `1000건` 결과 기준으로 보면  
> `id BETWEEN` 조회는 `author` 선형 탐색보다 약 `4,925x` 빠릅니다.

---

## 왜 빨라지는가

```mermaid
flowchart LR
    Q["SELECT ... WHERE ..."] --> C{"조건이 id 인가?"}
    C -->|YES| B["B+Tree로 리프 탐색"]
    C -->|NO| L["rows 전체 선형 탐색"]
    B --> R["결과 RowSet"]
    L --> R
    R --> O["rows / scan / time 출력"]
```

- `id = n`은 B+Tree 단건 조회
- `id BETWEEN a AND b`는 B+Tree 리프 링크 범위 조회
- `author`, `genre`, `title`, 전체 조회는 선형 탐색

---

## 처리 흐름

```mermaid
flowchart TD
    A["SQL batch"] --> B["split_sql"]
    B --> C["lex_stmt"]
    C --> D["parse_stmt"]
    D --> E["run_qry"]
    E --> F{"SELECT / INSERT"}
    F --> G["B+Tree 또는 Linear 조회"]
    F --> H["row 추가 + id 인덱스 반영"]
    G --> I["RowSet"]
    H --> J["Db rows"]
    J --> K["BKDB file 저장"]
    I --> L["출력 버퍼"]
```

### 지원 문법

- `SELECT * FROM books;`
- `SELECT * FROM books WHERE id = 3;`
- `SELECT * FROM books WHERE id BETWEEN 10 AND 20;`
- `SELECT title,genre FROM books WHERE author = 'George Orwell';`
- `INSERT INTO books VALUES ('Book','Author','Genre');`

---

## 실패해도 안전한 이유

```mermaid
flowchart LR
    S["배치 시작"] --> M["len / next_id 저장"]
    M --> X["문장 순차 실행"]
    X --> Y{"모두 성공?"}
    Y -->|YES| Z["db_save + 최종 출력"]
    Y -->|NO| R["len / next_id 복원"]
    R --> T["db_reidx로 B+Tree 재구성"]
```

- 출력은 바로 찍지 않고 버퍼에 모아 둡니다.
- 실패하면 `len`, `next_id`를 되돌리고 B+Tree를 다시 만듭니다.
- 저장은 `.tmp` 파일 교체 방식이라 부분 저장 위험을 줄였습니다.

---
