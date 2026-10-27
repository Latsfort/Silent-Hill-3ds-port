# Extracts your own Silent Hill (USA) disc image and copies the textures into romfs.
# Usage: put "Silent Hill (USA).bin" + ".cue" in dump/, then run:
#   powershell -ExecutionPolicy Bypass -File scripts\prepare_assets.ps1
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$decomp = "vendor\silent-hill-decomp"
$image = Get-ChildItem dump -Filter *.bin | Select-Object -First 1
if (-not $image) {
    throw "No .bin disc image found in dump\"
}

Write-Host "1/3 Extracting disc filesystem from $($image.Name)..."
& "$decomp\tools\psxiso\dumpsxiso.exe" -x dump\iso -s dump\layout.xml $image.FullName | Out-Null

Write-Host "2/3 Splitting SILENT/HILL archives (extract.py from the decomp)..."
cmd /c "python $decomp\tools\silentassets\extract.py -exe dump\iso\SLUS_007.07 -fs dump\iso\SILENT -fh dump\iso\HILL dump\assets > dump\extract.log 2>&1"
if ($LASTEXITCODE -ne 0) {
    throw "extract.py failed, see dump\extract.log"
}

Write-Host "3/3 Copying textures, maps, Harry + enemy stubs to romfs\game..."
New-Item -ItemType Directory -Force romfs\game\tim, romfs\game\bg, romfs\game\chara, romfs\game\anim | Out-Null
Copy-Item dump\assets\1ST\*.TIM, dump\assets\TIM\*.TIM romfs\game\tim -Force
Copy-Item dump\assets\BG\*.IPD, dump\assets\BG\*_GLB.PLM, dump\assets\BG\*.TIM romfs\game\bg -Force
# Common pickup world models (WorldObjects_Add item library) + their TIMs, looked up by material name
Copy-Item dump\assets\BG\BG_ITEM.PLM, dump\assets\ITEM\TIM00.TIM, dump\assets\TIM\BG_ETC.TIM romfs\game\bg -Force
# Harry (required)
Copy-Item dump\assets\CHARA\HERO.ILM, dump\assets\CHARA\HERO.TIM romfs\game\chara -Force
Copy-Item dump\assets\ANIM\HB_BASE.ANM romfs\game\anim -Force
# Cafe intro + common enemies / NPCs (ignore if missing)
$charaExtra = @(
    'BIRD.ILM','REBIRD.TIM','SIBYL.ILM','SIBYL.TIM','SRL.ILM','SRL.TIM','DOG.ILM','DOG.TIM','FAT.ILM','FAT.TIM',
    'AIR_SCRE.ILM','AIR_SCRE.TIM','STLKER.ILM','STLKER.TIM','CREEPER.ILM','CREEPER.TIM',
    'ROMPER.ILM','ROMPER.TIM','B_SUCKER.ILM','B_SUCKER.TIM'
)
foreach ($f in $charaExtra) {
    $src = Join-Path 'dump\assets\CHARA' $f
    if (Test-Path $src) { Copy-Item $src romfs\game\chara -Force }
}
foreach ($f in @('BIRD.ANM','DOG.ANM','FAT.ANM','HB_M0S00.ANM','HB_M0S01.ANM','HB_M1S00.ANM','HB_M1S01.ANM','HB_M1S02.ANM','HB_M1S03.ANM','HB_M2S00.ANM','HB_WEPAL.ANM','HB_WEP3.ANM','HB_WEP4.ANM','SBL.ANM','SRL.ANM','OPEN.DMS','CAFE.DMS','CAFE2.DMS')) {
    $src = Join-Path 'dump\assets\ANIM' $f
    if (Test-Path $src) { Copy-Item $src romfs\game\anim -Force }
}
Get-ChildItem dump\assets\ANIM -Filter 'AIR*.ANM' -ErrorAction SilentlyContinue | Copy-Item -Destination romfs\game\anim -Force
Get-ChildItem dump\assets\ANIM -Filter 'ST*.ANM' -ErrorAction SilentlyContinue | Select-Object -First 4 | Copy-Item -Destination romfs\game\anim -Force
if (Test-Path 'dump\assets\TIM\MP_0TOWN.TIM') {
    Copy-Item 'dump\assets\TIM\MP_0TOWN.TIM' romfs\game\tim -Force
}

# Inventory / status UI textures (2D sheets + health portrait)
New-Item -ItemType Directory -Force romfs\game\item | Out-Null
foreach ($f in @('HEROPIC2.TIM','TIM00.TIM','TIM01.TIM','TIM02.TIM','TIM03.TIM','TIM04.TIM','TIM05.TIM','TIM06.TIM','TIM07.TIM')) {
    $src = Join-Path 'dump\assets\ITEM' $f
    if (Test-Path $src) { Copy-Item $src romfs\game\item -Force }
}
# Pickup close-up models (Gfx_PickupItemAnimate): knife, flashlight, radio, health drink
foreach ($f in @('HANDGUN.PLM','KNIFE.PLM','PHONE.PLM','PHONE.TIM','PIPE.PLM','HPRGUN.TIM','UNQ80.TMD','UNQE0.TMD','UNQE1.TMD','UNQ20.TMD','UNQ42.TMD','UNQ43.TMD','UNQ44.TMD','UNQ55.TMD','UNQ63.TMD','UNQA2.TMD','UNQ45.TMD','UNQ46.TMD','UNQ60.TMD','UNQ61.TMD')) {
    $src = Join-Path 'dump\assets\ITEM' $f
    if (Test-Path $src) { Copy-Item $src romfs\game\item -Force }
}
if (Test-Path 'dump\assets\1ST\FONT16.TIM') {
    Copy-Item 'dump\assets\1ST\FONT16.TIM' romfs\game\tim -Force
}

# Sound driver data (source\sound), same sub-paths as on disc.
$sndFiles = @(
    '1ST\BASE.VAB', 'SND\FIRST.VAB', 'SND\PISTOL.VAB', 'SND\FUEL.VAB', 'SND\MAP001.VAB',
    'SND\MEP100.VAB',          # map1_s00 ambient task 172
    'SND\MEP101.VAB',          # map1_s01 ambient task 173
    'SND\E.VAB', 'SND\E.KDT',   # BGM track 9 (Cybil cutscene)
    'SND\T2.VAB', 'SND\T2.KDT', # BGM track 30 (Air Screamer)
    'SND\C2.VAB', 'SND\C2.KDT'  # BGM track 6
)
foreach ($f in $sndFiles) {
    $dst = Join-Path 'romfs\game\snd' $f
    New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
    Copy-Item (Join-Path 'dump\assets' $f) $dst -Force
}

# XA voice clips (gSDXATable index -> romfs\game\xa\NNN.xac), built with tools\xa_extract.c.
$hostGcc = Join-Path $env:DEVKITPRO 'msys2\usr\bin\gcc.exe'
if (-not $env:DEVKITPRO -or -not (Test-Path $hostGcc)) { $hostGcc = 'C:\devkitPro\msys2\usr\bin\gcc.exe' }
New-Item -ItemType Directory -Force build, romfs\game\xa | Out-Null
& $hostGcc -std=gnu11 -O2 -Iinclude -Isource\sound tools\xa_extract.c source\sound\sd_data.c -o build\xa_extract.exe
if ($LASTEXITCODE -ne 0) { throw "xa_extract build failed" }
$xaCmds = @(
    4127, 4130, 4097, 4136, 4099, 4112, 4105, 4125, 4123, 4144, 4121, 4104, 4143, 4147, 4117,
    4129, 4120, 4103, 4135, 4134, 4142, 4148, 4140, 4141, 4111, 4106, 4118, 4146, 4114, 4139,
    4122, 4102, 4116, 4137, 4128, 4107, 4133, 4110, 4115, 4113, 4145, 4124, 4098,
    4108, 4138, 4100, 4101
)
& build\xa_extract.exe dump\assets\XA romfs\game\xa @xaCmds
if ($LASTEXITCODE -ne 0) { throw "xa_extract failed" }

# SysState_Fmv 16: original STR video and unmodified XA audio packets.
& python tools\prepare_movie.py 16
if ($LASTEXITCODE -ne 0) { throw 'Movie preparation failed. Install FFmpeg or imageio-ffmpeg in build/video-tools.' }

$textures = (Get-ChildItem romfs\game\tim -Filter *.TIM).Count
$chunks = (Get-ChildItem romfs\game\bg -Filter *.IPD).Count
$charaN = (Get-ChildItem romfs\game\chara -Filter *.ILM).Count
$sndN = @(Get-ChildItem romfs\game\snd -Recurse -File).Count
$xaN = @(Get-ChildItem romfs\game\xa -Filter *.xac).Count
$harry = Test-Path romfs\game\chara\HERO.ILM
Write-Host "Done: $textures textures, $chunks map chunks, $charaN chara ILM, $sndN sound banks, $xaN voice clips, Harry=$(if ($harry) { 'OK' } else { 'MISSING' }). Now run 'make' in the devkitPro shell."
