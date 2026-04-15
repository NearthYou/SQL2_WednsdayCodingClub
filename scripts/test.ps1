param()

$ErrorActionPreference = "Stop"
& "$PSScriptRoot/build.ps1"
& "build/test_unit.exe"
& "build/test_func.exe"
& "$PSScriptRoot/acceptance.ps1" -SkipBuild

$smoke = & "build/sql2_books.exe" --mode cli --batch "SELECT * FROM books WHERE id = 1;"
Write-Host $smoke

$default = "SELECT * FROM books WHERE id = 2;"
Set-Content -Path "data/input.sql" -Value $default -NoNewline -Encoding ascii
$fileOut = & "build/sql2_books.exe" --mode file --file "data/input.sql"
Remove-Item -LiteralPath "data/input.sql" -ErrorAction SilentlyContinue
Write-Host $fileOut

Write-Host "All local tests passed."
