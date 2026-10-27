param([switch]$Skip, [int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$sdRoot = Join-Path $env:APPDATA 'Azahar/sdmc'
$scriptPath = Join-Path $sdRoot 'sh3ds_autotest.txt'
$logPath = Join-Path $sdRoot 'sh3ds_autotest.log'
if (Get-Process azahar -ErrorAction SilentlyContinue) { throw 'Close Azahar before running the movie regression.' }
$config = [IO.File]::ReadAllText((Join-Path $env:APPDATA 'Azahar/config/qt-config.ini'))
if (-not $config.Contains('is_new_3ds=false')) { throw 'Old 3DS configuration required.' }
$original = @{}
foreach ($path in @($scriptPath, $logPath)) {
    $original[$path] = if (Test-Path -LiteralPath $path) { [IO.File]::ReadAllBytes($path) } else { $null }
}
$steps = @('log 1', '60 NONE', '1 A', '900 NONE', '1 START', '3 NONE',
    'warp -6.2 -144 180', '5 NONE')
if ($Skip) { $steps += @('390 NONE', '1 START', '180 NONE') }
else { $steps += @('1000 NONE') }
$steps += 'quit'
try {
    $completed = $false
    for ($attempt = 0; $attempt -lt 3; $attempt++) {
    [IO.File]::WriteAllLines($scriptPath, [string[]]$steps)
    [IO.File]::WriteAllText($logPath, '')
    $process = Start-Process 'C:/Program Files/Azahar/azahar.exe' -ArgumentList ('"' + (Join-Path $projectRoot 'silent-hill-3ds.3dsx') + '"') -WindowStyle Hidden -PassThru
    try {
        $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
        do {
            Start-Sleep -Milliseconds 300
            $stream = [IO.File]::Open($logPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
            $reader = [IO.StreamReader]::new($stream)
            try { $log = $reader.ReadToEnd() } finally { $reader.Dispose() }
        } while (-not $log.Contains('AUTOTEST END') -and -not $process.HasExited -and (Get-Date) -lt $deadline)
        $name = if ($Skip) { 'skip' } else { 'full' }
        $destination = Join-Path $projectRoot "build/movie-emulator-$name.log"
        [IO.File]::WriteAllText($destination, $log)
        if (-not $log.Contains('AUTOTEST END')) {
            if ($process.HasExited -and -not $log) {
                $launchLog = [IO.File]::ReadAllText((Join-Path $env:APPDATA 'Azahar/log/azahar_log.txt'))
                if ($launchLog.Contains('Failed to find title id for ROM')) { continue }
            }
            throw "Incomplete emulator run; inspect $destination"
        }
        $frames = [regex]::Matches($log, '(?m)^movie frame=(\d+) count=119 size=320x208 fps=15/1 failed=0')
        if ($frames.Count -eq 0) { throw 'Movie playback did not start.' }
        $maxFrame = ($frames | ForEach-Object { [int]$_.Groups[1].Value } | Measure-Object -Maximum).Maximum
        if (-not $Skip -and $maxFrame -ne 118) { throw 'Movie did not display its last frame.' }
        if ($Skip -and $maxFrame -ge 118) { throw 'Movie was not skipped early.' }
        $states = [regex]::Matches($log, '(?m)^f=.*$')
        if ($states[$states.Count - 1].Value -notmatch 'phase=3 map=0 ') { throw 'Movie did not return to the opening map.' }
        Write-Output "PASS: Old 3DS movie $name playback, last frame $maxFrame, returns to the opening map."
        $completed = $true
        break
    } finally {
        if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
    }
    if (-not $completed) { throw 'Emulator failed to recognize the 3dsx after three launch attempts.' }
} finally {
    foreach ($path in @($scriptPath, $logPath)) {
        if ($null -ne $original[$path]) { [IO.File]::WriteAllBytes($path, $original[$path]) }
        elseif (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
}
