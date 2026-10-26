param([string]$Emulator = 'C:/Program Files/Azahar/azahar.exe', [switch]$PrepareOnly)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$sourcePath = Join-Path $projectRoot 'silent-hill-3ds.3dsx'
if (-not (Test-Path -LiteralPath $sourcePath)) { throw 'Le jeu compile est introuvable : silent-hill-3ds.3dsx.' }
if (-not $PrepareOnly -and -not (Test-Path -LiteralPath $Emulator)) { throw 'Azahar est introuvable.' }
if (-not $PrepareOnly -and (Get-Process azahar -ErrorAction SilentlyContinue)) {
    throw 'Ferme Azahar avant de relancer le jeu avec Jouer.cmd.'
}
$ready = $false
for ($attempt = 0; $attempt -lt 3 -and -not $ready; $attempt++) {
    $hash = (Get-FileHash -LiteralPath $sourcePath -Algorithm SHA256).Hash
    $folder = Join-Path $projectRoot ('build/playable/' + $hash)
    New-Item -ItemType Directory -Force -Path $folder | Out-Null
    $snapshot = Join-Path $folder 'silent-hill-3ds.3dsx'
    if (Test-Path -LiteralPath $snapshot) {
        if ((Get-FileHash -LiteralPath $snapshot -Algorithm SHA256).Hash -ne $hash) {
            throw 'La copie de jeu existante est endommagee.'
        }
    } else {
        $temporary = Join-Path $folder ('copy-' + [guid]::NewGuid().ToString('N') + '.partial')
        try {
            Copy-Item -LiteralPath $sourcePath -Destination $temporary
            if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $hash) { continue }
            Move-Item -LiteralPath $temporary -Destination $snapshot
        } finally {
            if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
        }
    }
    $ready = $true
}
if (-not $ready) { throw 'Une compilation est en cours. Relance Jouer.cmd dans quelques instants.' }
Write-Output $snapshot
if (-not $PrepareOnly) {
    Start-Process -FilePath $Emulator -ArgumentList ('"' + $snapshot + '"') -WindowStyle Normal | Out-Null
}
