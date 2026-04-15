param()

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path build | Out-Null

function Resolve-GccPath {
  $fromPath = Get-Command gcc -ErrorAction SilentlyContinue
  if ($fromPath) {
    return $fromPath.Source
  }

  $candidates = @(
    "C:\Program Files (x86)\Dev-Cpp\MinGW64\bin\gcc.exe"
  )

  foreach ($candidate in $candidates) {
    if (Test-Path $candidate) {
      return $candidate
    }
  }

  throw "gcc was not found on PATH or in the expected Dev-Cpp MinGW location."
}

function Run-Step {
  param(
    [string]$Gcc,
    [string[]]$CmdArgs
  )
  & $Gcc @CmdArgs
  if ($LASTEXITCODE -ne 0) {
    throw "gcc failed with exit code $LASTEXITCODE"
  }
}

$gcc = Resolve-GccPath

$common = @(
  "src/util.c",
  "src/batch.c",
  "src/lex.c",
  "src/parse.c",
  "src/bpt.c",
  "src/store.c",
  "src/exec.c",
  "src/cli.c"
)

$app = $common + @("src/main.c", "src/gen_perf.c")
$unit = @("tests/test_unit.c") + $common + @("src/main.c", "src/gen_perf.c")
$func = @("tests/test_func.c") + $common + @("src/main.c", "src/gen_perf.c")
$perf = $common + @("src/gen_perf.c")

$appArgs = @("-std=c11", "-Wall", "-Wextra", "-pedantic", "-Iinclude") + $app + @("-o", "build/sql2_books.exe")
$unitArgs = @("-std=c11", "-Wall", "-Wextra", "-pedantic", "-DNO_MAIN", "-Iinclude") + $unit + @("-o", "build/test_unit.exe")
$funcArgs = @("-std=c11", "-Wall", "-Wextra", "-pedantic", "-DNO_MAIN", "-Iinclude") + $func + @("-o", "build/test_func.exe")
$perfArgs = @("-std=c11", "-Wall", "-Wextra", "-pedantic", "-DPERF_MAIN", "-Iinclude") + $perf + @("-o", "build/gen_perf.exe")

Run-Step -Gcc $gcc -CmdArgs $appArgs
Run-Step -Gcc $gcc -CmdArgs $unitArgs
Run-Step -Gcc $gcc -CmdArgs $funcArgs
Run-Step -Gcc $gcc -CmdArgs $perfArgs

Write-Host "Build complete."
