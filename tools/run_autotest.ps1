# Runs the .3dsx in Azahar with an autotest script (debug only).
# Usage: powershell -File tools/run_autotest.ps1 -Script "line1|line2" [-Seconds 60] [-Keep]
param([string]$Script, [int]$Seconds = 60, [switch]$Keep)

$exe = "C:\Program Files\Azahar\azahar.exe"
$sd = "$env:APPDATA\Azahar\sdmc"
$rom = Join-Path (Split-Path $PSScriptRoot -Parent) "silent-hill-3ds.3dsx"

Get-Process azahar -ErrorAction SilentlyContinue | Stop-Process -Force
while (Get-Process azahar -ErrorAction SilentlyContinue) { Start-Sleep -Milliseconds 300 }
Start-Sleep 2
[IO.File]::WriteAllLines("$sd\sh3ds_autotest.txt", [string[]]($Script -split '\|'))
Remove-Item "$sd\sh3ds_autotest.log" -ErrorAction SilentlyContinue

for ($try = 0; $try -lt 4; $try++) {
    Start-Process $exe -ArgumentList "`"$rom`""
    Start-Sleep 10
    if (Get-Process azahar -ErrorAction SilentlyContinue) { break }
}
$deadline = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $deadline -and (Get-Process azahar -ErrorAction SilentlyContinue)) {
    Start-Sleep 2
    if (-not $Keep -and (Select-String -Path "$sd\sh3ds_autotest.log" -Pattern "AUTOTEST END" -Quiet -ErrorAction SilentlyContinue)) { break }
}
if (-not $Keep) {
    Get-Process azahar -ErrorAction SilentlyContinue | Stop-Process -Force
}
Get-Content "$sd\sh3ds_autotest.log" -ErrorAction SilentlyContinue
