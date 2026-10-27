param([int]$TimeoutSeconds = 150, [switch]$VisualHold, [switch]$Nightmare)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$sdRoot = Join-Path $env:APPDATA 'Azahar/sdmc'
$scriptPath = Join-Path $sdRoot 'sh3ds_autotest.txt'
$logPath = Join-Path $sdRoot 'sh3ds_autotest.log'
if (Get-Process azahar -ErrorAction SilentlyContinue) { throw 'Close Azahar before this test.' }
$config = [IO.File]::ReadAllText((Join-Path $env:APPDATA 'Azahar/config/qt-config.ini'))
if (-not $config.Contains('is_new_3ds=false')) { throw 'Old 3DS configuration required.' }
$original = @{}
foreach ($path in @($scriptPath, $logPath)) {
    $original[$path] = if (Test-Path -LiteralPath $path) { [IO.File]::ReadAllBytes($path) } else { $null }
}
$steps = @('log 1', '60 NONE', '1 A', '900 NONE', '1 START', '3 NONE',
    'warp -6.2 -144 180', '390 NONE', '1 START', '600 NONE',
    'warp -55.5 -126 90', '600 NONE',
    'warp -57.25 -47 -90', '3 NONE', '1 A', '300 NONE',
    'warp -110 -224.6 180', '3 NONE', '1 A', '300 NONE',
    '60 NONE 0 154', '180 NONE', '1 A', '180 NONE', '1 A', '60 NONE', 'quit')
if ($VisualHold) { $steps = $steps[0..($steps.Count - 2)] + @('1800 NONE', 'quit') }
if ($Nightmare) {
    $steps = $steps[0..($steps.Count - 2)] + @('warp -252 -222 0', '900 NONE', 'quit')
}
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
            $destination = Join-Path $projectRoot 'build/alley-emulator-regression.log'
            [IO.File]::WriteAllText($destination, $log)
            if (-not $log.Contains('AUTOTEST END')) {
                if ($process.HasExited -and -not $log) {
                    $launch = [IO.File]::ReadAllText((Join-Path $env:APPDATA 'Azahar/log/azahar_log.txt'))
                    if ($launch.Contains('Failed to find title id for ROM')) { continue }
                }
                throw "Incomplete run; inspect $destination"
            }
            if ($log -notmatch 'alley=10/4 ') { throw 'First darkness message was not reached.' }
            if ($log -notmatch 'alley=10/8 torch=1/0 kf=98[0-9]') { throw 'Torch animation did not reach rendered frames.' }
            if ($log -notmatch 'alley=10/11 torch=1/1 kf=993') { throw 'Final torch pose and second message were not reached.' }
            if ($log -notmatch 'alley=9/0 torch=0/1 .*flags=11111') { throw 'Scene did not return control with original flags.' }
            if ($log -notmatch 'flame ready=1 visible=1 vertices=6') { throw 'Lighter texture/geometry did not reach the renderer.' }
            $flicker = [regex]::Matches($log, 'flame ready=1 visible=1 vertices=6[^\r\n]*flicker=(\d+) spot=(\d+)')
            $values = @($flicker | ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)
            if ($values.Count -lt 2) { throw 'Lighter environment flicker did not vary in gameplay.' }
            if ($Nightmare) {
                $bytes = (Get-Item (Join-Path $projectRoot 'dump/assets/BG/THRF908.IPD')).Length
                if ($log -notmatch "nightmare chunk=-7,6 header=-7,6 bytes=$bytes pending=0") {
                    throw 'Original corpse chunk was not loaded and relocated at the nightmare event.'
                }
                if ($log -notmatch 'alley=12/') { throw 'Nightmare did not return to the fight.' }
                if ($log -notmatch 'nightmare scene=3 path=\d+/\d+ camera=1 head=1 hidden=0') {
                    throw 'Original walking camera was not reached with a valid Harry head pose.'
                }
                if ($log -notmatch 'nightmare scene=4 path=0/2 camera=1 head=1 hidden=1 voices=2 text=28') {
                    throw 'Corpse view and original second voice page were not reached.'
                }
                Write-Output 'PASS: original corpse chunk, path, head camera and voice chain, then return to gameplay.'
            }
            Write-Output 'PASS: Old 3DS alley doors, darkness messages, rendered torch animation and return to gameplay.'
            $completed = $true
            break
        } finally {
            if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
            $process.Dispose()
        }
    }
    if (-not $completed) { throw 'Emulator did not recognize the 3dsx after three attempts.' }
} finally {
    foreach ($path in @($scriptPath, $logPath)) {
        if ($null -ne $original[$path]) { [IO.File]::WriteAllBytes($path, $original[$path]) }
        elseif (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path }
    }
}
