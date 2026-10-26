param([int[]]$Only = @(0, 1, 2, 3, 4, 5, 6, 7))

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$testDir = Join-Path $projectRoot 'build/checkpoint-regressions'
$sdPath = Join-Path $env:APPDATA 'Azahar/sdmc'
$scriptPath = Join-Path $sdPath 'sh3ds_autotest.txt'
$logPath = Join-Path $sdPath 'sh3ds_autotest.log'
if (Get-Process azahar -ErrorAction SilentlyContinue) { throw 'Close Azahar before running checkpoints.' }
$config = [IO.File]::ReadAllText((Join-Path $env:APPDATA 'Azahar/config/qt-config.ini'))
if (-not $config.Contains('is_new_3ds=false')) { throw 'Old 3DS configuration required.' }
New-Item -ItemType Directory -Force -Path $testDir | Out-Null
$hadScript = Test-Path -LiteralPath $scriptPath
$oldScript = if ($hadScript) { [IO.File]::ReadAllBytes($scriptPath) } else { $null }
$hadLog = Test-Path -LiteralPath $logPath
$oldLog = if ($hadLog) { [IO.File]::ReadAllBytes($logPath) } else { $null }
try {
    foreach ($index in $Only) {
        if ($index -lt 0 -or $index -gt 7) { throw 'Invalid checkpoint index.' }
        $steps = @('log 15', '60 NONE')
        for ($i = 0; $i -lt 9 + $index; $i++) { $steps += @('1 DOWN', '1 NONE') }
        $steps += @('1 A', '300 NONE', 'quit')
        $complete = $false
        for ($attempt = 0; $attempt -lt 3 -and -not $complete; $attempt++) {
            [IO.File]::WriteAllLines($scriptPath, [string[]]$steps)
            [IO.File]::WriteAllText($logPath, '')
            $process = Start-Process 'C:/Program Files/Azahar/azahar.exe' -ArgumentList ('"' + (Join-Path $projectRoot 'silent-hill-3ds.3dsx') + '"') -WindowStyle Hidden -PassThru
            try {
                $deadline = (Get-Date).AddSeconds(60)
                do {
                    Start-Sleep -Milliseconds 200
                    $stream = [IO.File]::Open($logPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
                    $reader = [IO.StreamReader]::new($stream)
                    try { $log = $reader.ReadToEnd() } finally { $reader.Dispose() }
                } while (-not $log.Contains('AUTOTEST END') -and -not $process.HasExited -and (Get-Date) -lt $deadline)
                if ($log.Contains('AUTOTEST END')) {
                    $null = $process.WaitForExit(5000)
                    $log = [IO.File]::ReadAllText($logPath)
                    $complete = $true
                } elseif ($process.HasExited -and -not $log) {
                    $launchStream = [IO.File]::Open((Join-Path $env:APPDATA 'Azahar/log/azahar_log.txt'), [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
                    $launchReader = [IO.StreamReader]::new($launchStream)
                    try { $launchLog = $launchReader.ReadToEnd() } finally { $launchReader.Dispose() }
                    if ($launchLog -notmatch 'Failed to find title id for ROM') { throw 'Emulator launch failed.' }
                } else { throw "Incomplete checkpoint $index" }
            } finally {
                if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
                $process.Dispose()
            }
        }
        if (-not $complete) { throw 'Emulator launch failed three times.' }
        [IO.File]::WriteAllText((Join-Path $testDir "$index.log"), $log)
        $frames = [regex]::Matches($log, '(?m)^f=.*$')
        $lastFrame = $frames[$frames.Count - 1].Value
        if ($lastFrame -notmatch 'phase=3 .*stage=2 scr=0/' -or $lastFrame -notmatch 'ground=1') {
            throw "Checkpoint $index did not reach grounded gameplay."
        }
        if ($log -notmatch "checkpoint=$index ") { throw "Wrong menu checkpoint: $index" }
        Write-Output "PASS: Old 3DS menu checkpoint $index loaded with restored state and grounded gameplay."
    }
} finally {
    if ($hadScript) { [IO.File]::WriteAllBytes($scriptPath, $oldScript) }
    else { Remove-Item -LiteralPath $scriptPath -ErrorAction SilentlyContinue }
    if ($hadLog) { [IO.File]::WriteAllBytes($logPath, $oldLog) }
    else { Remove-Item -LiteralPath $logPath -ErrorAction SilentlyContinue }
}
