#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT_DIR"

mkdir -p build/acceptance

DB="build/acceptance/default.msqldb"
SQL_FILE="build/acceptance/batch.sql"
OUT="build/acceptance/out.txt"
ERR="build/acceptance/err.txt"

./build/make_fixture "$DB" >/dev/null

printf '%s\n%s\n%s\n' "$DB" "1" '"SELECT * FROM records WHERE id = 1; SELECT * FROM records WHERE id BETWEEN 1 AND 3; SELECT * FROM records WHERE value = 10;"' \
  | ./bin/minisql --order 4 >"$OUT" 2>"$ERR"

grep -q "alice" "$OUT"
grep -q "carol" "$OUT"
grep -q "TRANSACTION COMMITTED" "$OUT"

printf "INSERT INTO records VALUES ('dave', 40); SELECT * FROM records WHERE id = 4;\n" > "$SQL_FILE"
printf '%s\n%s\n%s\n' "$DB" "2" "$SQL_FILE" | ./bin/minisql --order 4 >"$OUT" 2>"$ERR"
grep -q "dave" "$OUT"
grep -q "TRANSACTION COMMITTED" "$OUT"

printf '%s\n%s\n%s\n' "$DB" "1" '"INSERT INTO records VALUES ('\''eve'\'', 50); SELECT * FROM records WHERE id BETWEEN 9 AND 1;"' \
  | ./bin/minisql --order 4 >"$OUT" 2>"$ERR" && {
    echo "expected rollback case to fail" >&2
    exit 1
  }

grep -q "TRANSACTION ROLLED BACK" "$ERR"

printf '%s\n%s\n%s\n' "$DB" "1" '"SELECT * FROM records WHERE id = 5;"' | ./bin/minisql --order 4 >"$OUT" 2>"$ERR"
grep -q "0 row(s)" "$OUT"

printf "bad" > build/acceptance/corrupt.msqldb
printf '%s\n%s\n%s\n' "build/acceptance/corrupt.msqldb" "1" '"SELECT * FROM records;"' \
  | ./bin/minisql --order 4 >"$OUT" 2>"$ERR" && {
    echo "expected corrupt DB case to fail" >&2
    exit 1
  }
grep -q "ERROR \[storage\]" "$ERR"

echo "acceptance: ok"
