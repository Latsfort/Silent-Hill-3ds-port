param([int]$TimeoutSeconds = 90)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$sdRoot = Join-Path $env:APPDATA 'Azahar/sdmc'
$scriptPath = Join-Path $sdRoot 'sh3ds_autotest.txt'
$logPath = Join-Path $sdRoot 'sh3ds_autotest.log'
if (Get-Process azahar -ErrorAction SilentlyContinue) { throw 'Close Azahar before running the save regression.' }
$config = [IO.File]::ReadAllText((Join-Path $env:APPDATA 'Azahar/config/qt-config.ini'))
if (-not $config.Contains('is_new_3ds=false')) { throw 'Old 3DS configuration required.' }
$paths = @($scriptPath, $logPath,
    (Join-Path $sdRoot 'sh3ds-0-000.sav'), (Join-Path $sdRoot 'sh3ds-0-000.sav.bak'), (Join-Path $sdRoot 'sh3ds-0-000.sav.tmp'))
$original = @{}
foreach ($path in $paths) {
    $original[$path] = if (Test-Path -LiteralPath $path) { [IO.File]::ReadAllBytes($path) } else { $null }
}
$steps = @('log 1', '60 NONE', '1 DOWN', '1 NONE', '1 DOWN', '1 NONE', '1 A', '240 NONE',
    'warp 4.65 -265.9 -90', '3 NONE', '1 A', '30 NONE', '1 A', '3 NONE', '1 A', '30 NONE',
    '1 B', '3 NONE', '1 B', '30 NONE', '1 START+SELECT', '15 NONE')
for ($i = 0; $i -lt 3; $i++) { $steps += @('1 UP', '1 NONE') }
$steps += @('1 A', '30 NONE', '1 A', '30 NONE', '1 A', '240 NONE', 'quit')
try {
    foreach ($path in $paths[2..4]) {
        if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
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
        $destination = Join-Path $projectRoot 'build/save-emulator-regression.log'
        [IO.File]::WriteAllText($destination, $log)
        if (-not $log.Contains('AUTOTEST END')) { throw "Incomplete emulator run; inspect $destination" }
        if ($log -notmatch 'save_menu step=3 writing=1 result=0') { throw 'Save-point write did not succeed.' }
        if ($log -notmatch 'save_menu step=3 writing=0 result=0.*loaded=1') { throw 'Load did not succeed.' }
        $frames = [regex]::Matches($log, '(?m)^f=.*$')
        $last = $frames[$frames.Count - 1].Value
        if ($last -notmatch 'phase=3 map=1 ' -or $last -notmatch 'ground=1') { throw 'Loaded cafe did not return to grounded gameplay.' }
        Write-Output 'PASS: Old 3DS cafe save point writes to SD and LOAD restores grounded gameplay.'
    } finally {
        if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
} finally {
    foreach ($path in $paths) {
        if ($null -ne $original[$path]) { [IO.File]::WriteAllBytes($path, $original[$path]) }
        elseif (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
}
