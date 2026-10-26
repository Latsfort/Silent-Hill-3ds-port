$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$sdFolder = Join-Path $env:APPDATA 'Azahar/sdmc/3ds/silent-hill-3ds'
$sdGame = Join-Path $sdFolder 'silent-hill-3ds.3dsx'
$hadGame = Test-Path -LiteralPath $sdGame
$oldGame = if ($hadGame) { [IO.File]::ReadAllBytes($sdGame) } else { $null }
if (Get-Process azahar -ErrorAction SilentlyContinue) { throw 'Close Azahar before the SD mount regression.' }
try {
    New-Item -ItemType Directory -Force -Path $sdFolder | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectRoot 'silent-hill-3ds.3dsx') -Destination $sdGame -Force
    & (Join-Path $PSScriptRoot 'run_menu_regressions.ps1') -Only launch_points -GamePath (Join-Path $projectRoot 'silent-hill-3ds.elf')
    $diagnostic = [IO.File]::ReadAllText((Join-Path $env:APPDATA 'Azahar/sdmc/sh3ds_load.log'))
    if ($diagnostic -notmatch 'mount result=00000000' -or
        $diagnostic -notmatch 'scan textures=[1-9]\d* areas=[1-9]\d*' -or
        $diagnostic -notmatch 'loaded THR chunks=' -or $diagnostic -notmatch 'loaded SC chunks=') {
        throw 'The real SD fallback did not load both town and school.'
    }
    [IO.File]::WriteAllText((Join-Path $projectRoot 'build/menu-regressions/sd_mount.log'), $diagnostic)
    Write-Output 'PASS: real libctru SD fallback without launcher arguments, old/new menu points and repeated area loads.'
} finally {
    if ($hadGame) { [IO.File]::WriteAllBytes($sdGame, $oldGame) }
    elseif (Test-Path -LiteralPath $sdGame) { Remove-Item -LiteralPath $sdGame }
}
