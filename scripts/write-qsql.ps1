param(
  [Parameter(Mandatory = $true)][string]$Sql,
  [Parameter(Mandatory = $true)][string]$Out
)

$ErrorActionPreference = "Stop"
$sqlBytes = [System.Text.Encoding]::UTF8.GetBytes($Sql)
$ver = [System.BitConverter]::GetBytes([UInt32]1)
$len = [System.BitConverter]::GetBytes([UInt32]$sqlBytes.Length)
$buf = New-Object byte[] (12 + $sqlBytes.Length)
[Array]::Copy([System.Text.Encoding]::ASCII.GetBytes("QSQL"), 0, $buf, 0, 4)
[Array]::Copy($ver, 0, $buf, 4, 4)
[Array]::Copy($len, 0, $buf, 8, 4)
[Array]::Copy($sqlBytes, 0, $buf, 12, $sqlBytes.Length)
[System.IO.File]::WriteAllBytes($Out, $buf)
Write-Host "Wrote $Out"

