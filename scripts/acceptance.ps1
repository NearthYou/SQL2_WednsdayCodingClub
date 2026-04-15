param(
  [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
if (-not $SkipBuild) {
  & "$PSScriptRoot/build.ps1"
}

$root = Split-Path -Parent $PSScriptRoot
$dir = Join-Path $root "build\acceptance"
New-Item -ItemType Directory -Force -Path $dir | Out-Null

function Require-Contains {
  param(
    [string]$Text,
    [string]$Needle,
    [string]$Label
  )
  if ($Text -notlike "*$Needle*") {
    throw "$Label missing: $Needle"
  }
}

function Remove-TestFiles {
  param([string]$Path)
  Remove-Item -LiteralPath $Path -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath ($Path + ".tmp") -ErrorAction SilentlyContinue
  Remove-Item -LiteralPath ($Path + ".bak") -ErrorAction SilentlyContinue
}

function Invoke-ExpectFail {
  param(
    [scriptblock]$Action,
    [string]$Label
  )

  try {
    & $Action *> $null
  } catch {
    if ($LASTEXITCODE -eq 0) {
      throw "$Label failed unexpectedly: $($_.Exception.Message)"
    }
  }

  if ($LASTEXITCODE -eq 0) {
    throw "$Label should fail"
  }
}

$db1 = Join-Path $dir "case1.bin"
Remove-TestFiles $db1
$out = & "build/sql2_books.exe" --mode cli --data $db1 --batch "SELECT * FROM books WHERE id = 1; SELECT * FROM books WHERE id BETWEEN 2 AND 4; SELECT * FROM books WHERE author = 'George Orwell';"
Require-Contains ($out -join "`n") "Clean Code" "case1 output"
Require-Contains ($out -join "`n") "The Hobbit" "case1 output"
Require-Contains ($out -join "`n") "Animal Farm" "case1 output"
Require-Contains ($out -join "`n") "scan=B+Tree" "case1 output"
Require-Contains ($out -join "`n") "scan=Linear" "case1 output"

$db2 = Join-Path $dir "case2.bin"
Remove-TestFiles $db2
$out = & "build/sql2_books.exe" --mode cli --data $db2 --batch "INSERT INTO books VALUES ('Acceptance Book','Acceptance Author','Acceptance'); SELECT * FROM books WHERE id BETWEEN 13 AND 13;"
Require-Contains ($out -join "`n") "inserted id=13" "case2 output"
Require-Contains ($out -join "`n") "Acceptance Book" "case2 output"
Require-Contains ($out -join "`n") "scan=B+Tree" "case2 output"

$db3 = Join-Path $dir "case3.bin"
Remove-TestFiles $db3
Invoke-ExpectFail { & "build/sql2_books.exe" --mode cli --data $db3 --batch "INSERT INTO books VALUES ('Rollback Book','Rollback Author','Tmp'); SELECT * FROM books WHERE id BETWEEN 9 AND 1;" } "case3"
$out = & "build/sql2_books.exe" --mode cli --data $db3 --batch "SELECT * FROM books WHERE author = 'Rollback Author';"
Require-Contains ($out -join "`n") "(no rows)" "case3 rollback check"

$db4 = Join-Path $dir "corrupt.bin"
Set-Content -Path $db4 -NoNewline -Value "bad"
Invoke-ExpectFail { & "build/sql2_books.exe" --mode cli --data $db4 --batch "SELECT * FROM books;" } "corrupt DB"

Write-Host "Acceptance tests passed."
