param(
  [int]$Count = 1000000
)

$ErrorActionPreference = "Stop"
& "$PSScriptRoot/build.ps1"

$genTime = Measure-Command {
  & "build/gen_perf.exe" "data/perf_books.bin" $Count | Out-Null
}

$idLine = (& "build/sql2_books.exe" --mode cli --data "data/perf_books.bin" --batch "SELECT * FROM books WHERE id = $Count;") | Select-String "scan=" | ForEach-Object { $_.Line }
$authLine = (& "build/sql2_books.exe" --mode cli --data "data/perf_books.bin" --batch "SELECT * FROM books WHERE author = 'Author 999';") | Select-String "scan=" | ForEach-Object { $_.Line }
$genreLine = (& "build/sql2_books.exe" --mode cli --data "data/perf_books.bin" --batch "SELECT * FROM books WHERE genre = 'Genre 7';") | Select-String "scan=" | ForEach-Object { $_.Line }

Write-Host ("generate_time={0:N3} sec" -f $genTime.TotalSeconds)
Write-Host $idLine
Write-Host $authLine
Write-Host $genreLine

