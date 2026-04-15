param()

$ErrorActionPreference = "Stop"
& "$PSScriptRoot/build.ps1"
& "build/sql2_books.exe" --mode file --file "data/demo_queries.sql"

