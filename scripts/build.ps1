param()

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path build | Out-Null

function Run-Step {
  param([string[]]$CmdArgs)
  & gcc @CmdArgs
  if ($LASTEXITCODE -ne 0) {
    throw "gcc failed with exit code $LASTEXITCODE"
  }
}

$common = @(
  "src/util.c",
  "src/batch.c",
  "src/lex.c",
  "src/parse.c",
  "src/bpt.c",
  "src/store.c",
  "src/exec.c"
)

$app = $common + @("src/main.c", "src/gen_perf.c")
$unit = @("tests/test_unit.c") + $common + @("src/main.c", "src/gen_perf.c")
$func = @("tests/test_func.c") + $common + @("src/main.c", "src/gen_perf.c")
$perf = $common + @("src/gen_perf.c")

$appArgs = @("-std=c11", "-Wall", "-Wextra", "-pedantic", "-Iinclude") + $app + @("-o", "build/sql2_books.exe")
$unitArgs = @("-std=c11", "-Wall", "-Wextra", "-pedantic", "-DNO_MAIN", "-Iinclude") + $unit + @("-o", "build/test_unit.exe")
$funcArgs = @("-std=c11", "-Wall", "-Wextra", "-pedantic", "-DNO_MAIN", "-Iinclude") + $func + @("-o", "build/test_func.exe")
$perfArgs = @("-std=c11", "-Wall", "-Wextra", "-pedantic", "-DPERF_MAIN", "-Iinclude") + $perf + @("-o", "build/gen_perf.exe")

Run-Step -CmdArgs $appArgs
Run-Step -CmdArgs $unitArgs
Run-Step -CmdArgs $funcArgs
Run-Step -CmdArgs $perfArgs

Write-Host "Build complete."
