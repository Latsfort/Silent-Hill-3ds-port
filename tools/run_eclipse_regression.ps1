param([int]$TimeoutSeconds = 60, [ValidateSet('Eclipse', 'School', 'SchoolUpper', 'SchoolAcid', 'SchoolAcidSkip', 'SchoolPianoObjects', 'SchoolSilver', 'SchoolPiano', 'SchoolPianoSkip', 'SchoolPianoCancel', 'SchoolPianoDecline', 'SchoolLadders')][string]$Scenario = 'Eclipse')

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$testDir = Join-Path $projectRoot 'build/story-regressions'
$sdPath = Join-Path $env:APPDATA 'Azahar/sdmc'
$scriptPath = Join-Path $sdPath 'sh3ds_autotest.txt'
$logPath = Join-Path $sdPath 'sh3ds_autotest.log'
if (Get-Process azahar -ErrorAction SilentlyContinue) {
    throw 'Close Azahar before running the story regression.'
}
$config = [IO.File]::ReadAllText((Join-Path $env:APPDATA 'Azahar/config/qt-config.ini'))
if (-not $config.Contains('is_new_3ds=false')) {
    throw 'The story regression requires the emulator configured as Old 3DS.'
}
New-Item -ItemType Directory -Force -Path $testDir | Out-Null
$hadScript = Test-Path -LiteralPath $scriptPath
$oldScript = if ($hadScript) { [IO.File]::ReadAllBytes($scriptPath) } else { $null }
$hadLog = Test-Path -LiteralPath $logPath
$oldLog = if ($hadLog) { [IO.File]::ReadAllBytes($logPath) } else { $null }
$process = $null
try {
    $steps = @('log 15', '60 NONE')
    for ($i = 0; $i -lt 5; $i++) {
        $steps += @('1 DOWN', '1 NONE')
    }
    # MAP_EVENTS[72] is the retail LoadRoom that completes flag 158 before the Eclipse tick event.
    $steps += @('1 A', '240 NONE', 'event 72', '900 NONE', '1 A', '2 NONE', '1 A', '180 NONE', 'quit')
    if ($Scenario -ne 'Eclipse') {
        $steps = [IO.File]::ReadAllLines((Join-Path $testDir ($Scenario.ToLowerInvariant() + '.txt')))
    }
    [IO.File]::WriteAllLines($scriptPath, [string[]]$steps)
    [IO.File]::WriteAllText($logPath, '')
    $process = Start-Process 'C:/Program Files/Azahar/azahar.exe' -ArgumentList ('"' + (Join-Path $projectRoot 'silent-hill-3ds.3dsx') + '"') -WindowStyle Hidden -PassThru
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    do {
        Start-Sleep -Milliseconds 200
        $stream = [IO.File]::Open($logPath, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        $reader = [IO.StreamReader]::new($stream)
        try { $log = $reader.ReadToEnd() } finally { $reader.Dispose() }
    } while (-not $log.Contains('AUTOTEST END') -and -not $process.HasExited -and (Get-Date) -lt $deadline)
    if ($log.Contains('AUTOTEST END')) {
        $null = $process.WaitForExit(5000)
        $log = [IO.File]::ReadAllText($logPath)
    }
    $scenarioLog = Join-Path $testDir ($Scenario.ToLowerInvariant() + '.log')
    [IO.File]::WriteAllText($scenarioLog, $log)
    if (-not $log.Contains('AUTOTEST END')) {
        throw "Incomplete story emulator run; inspect $scenarioLog."
    }
    if ($Scenario -eq 'SchoolLadders') {
        $frames = [regex]::Matches($log, '(?m)^f=.*$')
        if ($log -notmatch 'harry=87/' -or $log -notmatch 'harry=88/' -or
            $log -notmatch 'ladder=0,1,0,0,1,0,0' -or
            $log -notmatch 'ladder=0,0,1,0,1,0,0' -or
            $frames[$frames.Count - 1].Value -notmatch 'phase=3 map=3 stage=2 scr=0/') {
            throw 'Both ladder routes, original animations or restored gameplay were not reached.'
        }
        Write-Output 'PASS: Old 3DS emulator completed both original ladder entrance/exit routes.'
        return
    }
    if ($Scenario -in @('SchoolPianoCancel', 'SchoolPianoDecline')) {
        $frames = [regex]::Matches($log, '(?m)^f=.*$')
        if ($log -match 'piano=1' -or $log -match 'silver_motion=1' -or
            $frames[$frames.Count - 1].Value -notmatch 'phase=3 map=4 stage=2 scr=0/') {
            throw 'Piano decline/cancel did not restore gameplay with the puzzle unsolved.'
        }
        Write-Output "PASS: Old 3DS $Scenario restored gameplay without solving the puzzle or moving silver."
        return
    }
    if ($Scenario -in @('SchoolPiano', 'SchoolPianoSkip')) {
        if ($Scenario -eq 'SchoolPianoSkip' -and $log -notmatch 'scr=12/17 ') {
            throw 'START did not reach the retail piano skip step 17.'
        }
        $frames = [regex]::Matches($log, '(?m)^f=.*$')
        if ($log -notmatch 'image=PIANO2.TIM' -or $log -notmatch 'piano=1 notes=5' -or
            $log -notmatch 'silver_motion=1 moving_mesh=[1-9]\d*' -or
            $log -notmatch 'checkpoint=5 .*silver=1' -or
            $frames[$frames.Count - 1].Value -notmatch 'phase=3 map=4 stage=2 scr=0/') {
            throw 'Piano input solution, actual medallion drawing or gameplay restoration was not reached.'
        }
        Write-Output "PASS: Old 3DS $Scenario solved the puzzle with directional input, drew moving silver, acquired it and restored gameplay."
        return
    }
    if ($Scenario -eq 'SchoolSilver') {
        $frames = [regex]::Matches($log, '(?m)^f=.*$')
        $lastFrame = $frames[$frames.Count - 1].Value
        if ($log -notmatch 'objects=4 mask=144' -or $log -notmatch 'objects=4 mask=16' -or
            $log -notmatch 'school=11/0' -or $log -notmatch 'harry=59/887' -or
            $log -notmatch 'harry=60/' -or $log -notmatch 'checkpoint=8 .*silver=1' -or
            $lastFrame -notmatch 'phase=3 map=4 stage=2 scr=0/') {
            throw 'Silver arrival, pickup animations, item or restored gameplay was not reached.'
        }
        Write-Output 'PASS: Old 3DS emulator loaded the pre-silver checkpoint, declined then acquired the medallion and restored gameplay.'
        return
    }
    if ($Scenario -eq 'SchoolPianoObjects') {
        if ($log -notmatch 'objects=[1-9]\d* mask=80' -or $log -notmatch 'phase=3 map=4 stage=2 scr=0/') {
            throw 'Piano-room source lid/suspended silver state or playable arrival was not reached.'
        }
        Write-Output 'PASS: Old 3DS emulator loaded the piano checkpoint with the source lid/silver visibility state.'
        return
    }
    if ($Scenario -in @('SchoolAcid', 'SchoolAcidSkip')) {
        $frames = [regex]::Matches($log, '(?m)^f=.*$')
        $lastFrame = $frames[$frames.Count - 1].Value
        if ($log -notmatch 'acid=0 smoke=1 mesh=[1-9]\d*' -or
            $log -notmatch 'acid=1 smoke=0 mesh=0 border=1/0 harry=0/' -or
            $log -notmatch 'chemical=1 gold=1' -or
            $lastFrame -notmatch 'phase=3 map=4 stage=2 scr=0/') {
            throw 'Acid inventory use, actual smoke draw, scene cleanup or gold pickup was not reached.'
        }
        Write-Output "PASS: Old 3DS $Scenario used the chemical through inventory, drew smoke, restored gameplay and obtained gold."
        return
    }
    if ($Scenario -eq 'SchoolUpper') {
        $frames = [regex]::Matches($log, '(?m)^f=.*$')
        $lastFrame = $frames[$frames.Count - 1].Value
        if ($log -notmatch 'area=SC school=4/4 image=HAND.TIM shade=128' -or
            $log -notmatch 'area=SC school=5/2 image=- shade=128' -or
            $log -notmatch 'image=HAND.TIM shade=128 objects=4 mask=1' -or
            $log -notmatch 'school=5/2 image=- shade=128 objects=4 mask=8' -or
            $log -notmatch 'chemical=1 gold=0' -or $lastFrame -notmatch 'phase=3 map=4 stage=2 scr=0/') {
            throw 'Upper school transition, statue, chemical prompt or restored gameplay was not reached.'
        }
        Write-Output 'PASS: Old 3DS emulator used the real stairs, inspected HAND and declined then took the chemical upstairs.'
        return
    }
    if ($Scenario -eq 'School') {
        $frames = [regex]::Matches($log, '(?m)^f=.*$')
        $lastFrame = $frames[$frames.Count - 1].Value
        if ($log -notmatch 'area=SC school=12/4 image=PAPER1.TIM shade=128' -or
            $log -notmatch 'area=SC school=12/5 image=PAPER1.TIM shade=48' -or
            $log -notmatch 'area=SC school=10/0 image=CLOCK.TIM shade=128' -or
            $log -notmatch 'area=SC school=19/0 image=- shade=128 objects=[34] mask=2' -or
            $log -notmatch 'area=SC school=20/3 image=MP_SCL1F.TIM shade=88' -or
            $log -notmatch 'paper_flags=8160 school_map_taken=1' -or
            $lastFrame -notmatch 'phase=3 map=3 stage=2 scr=0/') {
            throw 'School entry, document inspection, dimming or restored gameplay was not reached.'
        }
        Write-Output 'PASS: Old 3DS emulator read the school documents, inspected the clock/boiler and declined then took the school map.'
        return
    }
    if (-not $log.Contains('EVENT 72 fired=1')) {
        throw 'The retail room transition did not execute.'
    }
    if ($log -notmatch 'stage=3 scr=\d+/3 pos=147\.70,[^,]+,-376\.50') {
        throw 'The Eclipse dialogue step and retail player warp were not reached.'
    }
    if ($log -notmatch 'stage=2 scr=0/6 pos=147\.70,[^,]+,-376\.50') {
        throw 'The Eclipse scene did not restore gameplay at the retail position.'
    }
    Write-Output 'PASS: Old 3DS emulator executed the Eclipse transition, dialogue, lamp and return to gameplay.'
} finally {
    if ($process) {
        if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
    if ($hadScript) { [IO.File]::WriteAllBytes($scriptPath, $oldScript) }
    else { Remove-Item -LiteralPath $scriptPath -ErrorAction SilentlyContinue }
    if ($hadLog) { [IO.File]::WriteAllBytes($logPath, $oldLog) }
    else { Remove-Item -LiteralPath $logPath -ErrorAction SilentlyContinue }
}
