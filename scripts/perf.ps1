param(
  [int]$Count = 1000000
)

$ErrorActionPreference = "Stop"
& "$PSScriptRoot/build.ps1"

$genTime = Measure-Command {
  & "build/gen_perf.exe" "data/perf_books.bin" $Count | Out-Null
}

$idOut = & "build/sql2_books.exe" --mode cli --data "data/perf_books.bin" --summary-only --batch "SELECT * FROM books WHERE id = $Count;"
$rangeStart = [Math]::Max(1, $Count - 999)
$rangeOut = & "build/sql2_books.exe" --mode cli --data "data/perf_books.bin" --summary-only --batch "SELECT * FROM books WHERE id BETWEEN $rangeStart AND $Count;"
$authOut = & "build/sql2_books.exe" --mode cli --data "data/perf_books.bin" --summary-only --batch "SELECT * FROM books WHERE author = 'Author 999';"
$genreOut = & "build/sql2_books.exe" --mode cli --data "data/perf_books.bin" --summary-only --batch "SELECT * FROM books WHERE genre = 'Genre 7';"

Write-Host "[dataset build]"
Write-Host ("count : {0:N0}" -f $Count)
Write-Host ("time  : {0:N3} sec" -f $genTime.TotalSeconds)
Write-Host ""
Write-Host ($idOut -join "`n")
Write-Host ""
Write-Host ($rangeOut -join "`n")
Write-Host ""
Write-Host ($authOut -join "`n")
Write-Host ""
Write-Host ($genreOut -join "`n")
