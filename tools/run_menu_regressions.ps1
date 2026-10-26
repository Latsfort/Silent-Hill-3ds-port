param([string[]]$Only, [string]$GamePath)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $GamePath) { $GamePath = Join-Path $projectRoot 'silent-hill-3ds.3dsx' }
$testDir = Join-Path $projectRoot 'build/menu-regressions'
$configPath = Join-Path $env:APPDATA 'Azahar/config/qt-config.ini'
$scriptPath = Join-Path $env:APPDATA 'Azahar/sdmc/sh3ds_autotest.txt'
$logPath = Join-Path $env:APPDATA 'Azahar/sdmc/sh3ds_autotest.log'
if (Get-Process azahar -ErrorAction SilentlyContinue) {
    throw 'Close Azahar before running the regression scenarios.'
}
New-Item -ItemType Directory -Force -Path $testDir | Out-Null
$originalConfig = [IO.File]::ReadAllBytes($configPath)
$hadScript = Test-Path -LiteralPath $scriptPath
$originalScript = if ($hadScript) { [IO.File]::ReadAllBytes($scriptPath) } else { $null }
$hadLog = Test-Path -LiteralPath $logPath
$originalLog = if ($hadLog) { [IO.File]::ReadAllBytes($logPath) } else { $null }
$scenarios = [ordered]@{
    load_failure = @('log 15','60 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 A','90 NONE','1 B','10 NONE','1 A','90 NONE','1 START+SELECT','30 NONE','quit')
    launch_points = @('log 15','60 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 A','180 NONE','1 START+SELECT','10 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 A','180 NONE','1 START+SELECT','10 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 A','240 NONE','1 START+SELECT','10 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 A','240 NONE','quit')
    return_menu = @('log 1','60 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 A','180 NONE','1 SELECT','5 NONE','1 START+SELECT','10 NONE','1 A','180 NONE','1 START','1 START+SELECT','10 NONE','1 A','1 NONE','1 START+SELECT','30 NONE','quit')
    town_light = @('log 30','60 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 A','180 NONE','1 X','60 NONE','1 X','60 NONE','1 Y','60 NONE','1 X','60 NONE','1 Y','60 NONE','1 SELECT','60 NONE','1 SELECT','60 NONE','quit')
    cafe_map = @('log 30','60 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 A','120 NONE','1 Y','180 NONE','1 Y','90 NONE','quit')
    cafe_menu = @('log 30','60 NONE','1 DOWN','1 NONE','1 A','120 NONE','1 SELECT','120 NONE','1 START','300 NONE','1 SELECT','60 NONE','1 SELECT','60 NONE','quit')
    dpad = @('log 15','60 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 DOWN','1 NONE','1 A','180 NONE','60 RIGHT','30 NONE','60 UP','30 NONE','60 DOWN','30 NONE','60 UP+DOWN','30 NONE','60 LEFT+RIGHT','30 NONE','60 RIGHT 0 100','quit')
}
try {
    $testConfig = [IO.File]::ReadAllText($configPath).Replace('is_new_3ds\default=true','is_new_3ds\default=false').Replace('is_new_3ds=true','is_new_3ds=false')
    [IO.File]::WriteAllText($configPath, $testConfig)
    foreach ($scenario in $scenarios.GetEnumerator()) {
        if ($Only -and $scenario.Key -notin $Only) { continue }
        [IO.File]::WriteAllLines($scriptPath, [string[]]$scenario.Value)
        [IO.File]::WriteAllText($logPath, '')
        $process = Start-Process 'C:/Program Files/Azahar/azahar.exe' -ArgumentList ('"' + $GamePath + '"') -WindowStyle Hidden -PassThru
        try {
            $deadline = (Get-Date).AddSeconds(45)
            do {
                Start-Sleep -Milliseconds 200
                $stream = [IO.File]::Open($logPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
                $reader = New-Object IO.StreamReader($stream)
                try { $log = $reader.ReadToEnd() } finally { $reader.Dispose() }
            } while (-not $log.Contains('AUTOTEST END') -and -not $process.HasExited -and (Get-Date) -lt $deadline)
            [IO.File]::WriteAllText((Join-Path $testDir ($scenario.Key + '.log')), $log)
            if (-not $log.Contains('AUTOTEST END')) {
                throw "Incomplete: $($scenario.Key)"
            }
            Write-Output "Completed: $($scenario.Key)"
        } finally {
            if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
            $process.Dispose()
        }
    }
} finally {
    [IO.File]::WriteAllBytes($configPath, $originalConfig)
    if ($hadScript) { [IO.File]::WriteAllBytes($scriptPath, $originalScript) }
    else { Remove-Item -LiteralPath $scriptPath -ErrorAction SilentlyContinue }
    if ($hadLog) { [IO.File]::WriteAllBytes($logPath, $originalLog) }
    else { Remove-Item -LiteralPath $logPath -ErrorAction SilentlyContinue }
}
