# Gets the Longshot files the kits download (the pins in tools/release/kit/hookshot/TOOLS.txt, hookshot/PIN and
# hookshot/LINUX.txt), checks every one against its pinned SHA-256, adds the licence texts they lack, README.md
# (each file's licences and source) and SHA256SUMS, and prints the gh commands that publish them as the co-op
# mirror: a pre-release of daurosk/oot-remake-multiplayer tagged longshot-mirror-<release>, which the kits try
# after Longshot's own servers
# (kit-mirror, 0.11.1). It publishes nothing itself.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\release\mirror_longshot.ps1 [-Out <folder>] [-Feed <url>]
#
# -Feed: get the files from there (<feed>/<asset name>, e.g. an existing mirror, to check it) instead of Longshot's.
# Exits 1 when a file cannot be had with the right SHA-256.
param(
    [string]$Out = '',
    [string]$Feed = ''
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
. (Join-Path $PSScriptRoot 'kit\scripts\kit-paths.ps1')   # (Get-KitDownload, Set-KitTls)
Set-KitTls

function Read-Pin([string]$Path) {
    $map = @{}
    foreach ($line in Get-Content -LiteralPath $Path) { if ($line -match '^\s*([A-Za-z0-9_]+)\s*=\s*(.+?)\s*$') { $map[$Matches[1]] = $Matches[2] } }
    $map
}
function Stop-Mirror([string]$Text) { Write-Host "mirror_longshot: $Text" -ForegroundColor Red; exit 1 }

$tp = Read-Pin (Join-Path $PSScriptRoot 'kit\hookshot\TOOLS.txt')
$pin = Read-Pin (Join-Path $repo 'hookshot\PIN')
$lx = Read-Pin (Join-Path $repo 'hookshot\LINUX.txt')
foreach ($k in 'release', 'lock_sha256', 'source_commit', 'source_sha256', 'mirror') { if (-not $tp[$k]) { Stop-Mirror "TOOLS.txt has no $k=." } }
if ($tp.source_commit -ne $pin.commit) { Stop-Mirror "TOOLS.txt's source_commit ($($tp.source_commit)) is not hookshot/PIN's commit ($($pin.commit))." }
if ($lx.tag -ne $tp.release) { Stop-Mirror "hookshot/LINUX.txt (tag $($lx.tag)) and TOOLS.txt (release $($tp.release)) name different Longshot releases." }
$tag = "longshot-mirror-$($tp.release)"
foreach ($m in $tp.mirror, $lx.mirror) {
    if ($m -ne "https://github.com/daurosk/oot-remake-multiplayer/releases/download/$tag") { Stop-Mirror "mirror= ($m) is not this release's mirror tag ($tag) in TOOLS.txt and LINUX.txt." }
}
if ($pin.repo -notmatch '^https://git\.magicbean\.dev/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$') { Stop-Mirror "hookshot/PIN repo= is not on git.magicbean.dev." }
$up = $pin.repo
$api = $up -replace '^(https://[^/]+)/', '$1/api/v1/repos/'
if (-not $Out) { $Out = Join-Path ([System.IO.Path]::GetTempPath()) $tag }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
$Out = (Resolve-Path -LiteralPath $Out).Path
$Feed = $Feed.TrimEnd('/')

$assets = @()   # name, sha, what
function Get-Asset([string]$Name, [string]$Sha, [string[]]$Urls, [string]$What) {
    if ($Feed) { $Urls = @("$Feed/$Name") }
    $why = Get-KitDownload $Urls (Join-Path $Out $Name) $Sha
    if ($why) { Stop-Mirror "$Name ($What): $why" }
    $script:assets += [pscustomobject]@{ Name = $Name; Sha = $Sha.ToLowerInvariant(); What = $What }
}

# tools.lock, then the tools in it the Windows kit uses (hookshot-build.ps1's list), named <sha256>-<name>.zip
Get-Asset 'tools.lock' $tp.lock_sha256 @("$up/releases/download/$($tp.release)/tools.lock") "Longshot $($tp.release)'s tools.lock"
$wanted = @('llvm-mingw', 'python', 'cpm', 'wgpu-native', 'third-party')
$n = 0
foreach ($line in Get-Content -LiteralPath (Join-Path $Out 'tools.lock')) {
    $f = $line.Trim() -split '\s+'
    if ($f[0] -ne 'tool' -or $f.Count -lt 9 -or $f[3] -ne 'windows-x64' -or $wanted -notcontains $f[1]) { continue }
    Get-Asset "$($f[4].ToLowerInvariant())-$($f[1]).zip" $f[4] @($f[8..($f.Count - 1)] | Where-Object { $_ -match '^https://' }) "$($f[1]) $($f[2]) windows-x64, as tools.lock lists it"
    $n++
}
if ($n -ne $wanted.Count) { Stop-Mirror "tools.lock lists $n of the $($wanted.Count) windows-x64 tools the kit needs." }
Get-Asset "longshot-$($pin.commit).zip" $tp.source_sha256 @("$api/archive/$($pin.commit).zip", "$up/archive/$($pin.commit).zip") "Longshot's source at $($pin.commit) (tag $($tp.release))"
Get-Asset $lx.appimage $lx.sha256 @($lx.url) "Longshot $($lx.tag)'s Linux builder"


# --- Licences: every mirrored file names its licence(s) and source ---------------------------------------------------
# What each file is under, written for this pin from what the files hold (review-kit-mirror, 2026-11-15). Inside: the
# licence texts the file itself carries (zip entry regexes; each must be there, so a new pin that drops one stops
# here). Texts: licence files the file lacks, given beside it as mirror assets: from another mirrored zip (From, Entry)
# or, the LGPL, from FFmpeg's own tag, checked against a pinned SHA-256.
$tar = "$env:SystemRoot\System32\tar.exe"
$srcZip = "longshot-$($pin.commit).zip"
$texts = @(
    @{ Name = 'LICENSE'; From = $srcZip; Entry = '^[^/]+/LICENSE$' }
    @{ Name = 'LICENSE-llvm-Apache-2.0-with-LLVM-exception.txt'; Kind = 'llvm-mingw'; Entry = '^llvm-mingw-[^/]+/LICENSE\.TXT$' }
    @{ Name = 'LICENSE-sdl-zlib.txt'; From = $srcZip; Entry = '^[^/]+/third_party/sdl2/LICENSE\.txt$' }
    @{ Name = 'LICENSE-zbic-GPL-2.0.txt'; From = $srcZip; Entry = '^[^/]+/third_party/zbic/LICENSE$' }
    @{ Name = 'NOTICE-a64-kit.md'; From = $srcZip; Entry = '^[^/]+/third_party/a64-kit/NOTICE\.TOTKPort\.md$' }
    @{ Name = 'LICENSE-ffmpeg-LGPL-2.1.txt'; Url = 'https://raw.githubusercontent.com/FFmpeg/FFmpeg/n9.0.2/COPYING.LGPLv2.1'
       Sha = '246041b6ecf9bc32d718a62c57877c78b5eb397b6467e74ed7ae2626ab189c30' }
)
$lsLink = "$up/src/commit/$($pin.commit)"
$info = @{
    'tools.lock'  = @{ Licence = "Longshot's release file: GPL-3.0 (``LICENSE``)."; Source = "$up/releases/tag/$($tp.release)"; Inside = @() }
    'llvm-mingw'  = @{ Licence = 'llvm-mingw by Martin Storsjo. LLVM (clang, lld, ...): Apache-2.0 WITH LLVM-exception (``LICENSE.TXT`` inside, also ``LICENSE-llvm-Apache-2.0-with-LLVM-exception.txt`` here). mingw-w64 headers and runtime: their own licences (``*/share/mingw32/COPYING*`` inside). busybox-w32 (``busybox/``): GPL-2.0 (``busybox/LICENSE.txt`` inside). Python (``python/``): PSF-2.0 (``python/LICENSE.txt`` inside).'
                       Source = 'https://github.com/mstorsjo/llvm-mingw/releases/tag/20261104 (the same zip, unchanged); busybox-w32: https://github.com/rmyorston/busybox-w32 at 1f493261d (its version string: v1.38.0.git-5894-g1f493261d)'
                       Inside = @('^[^/]+/LICENSE\.TXT$', '/share/mingw32/COPYING', '/busybox/LICENSE\.txt$', '/python/LICENSE\.txt$') }
    'python'      = @{ Licence = 'CPython 3.14, PSF-2.0 (``LICENSE.txt`` inside); the packages in ``Lib/site-packages`` under their own licences (their ``*.dist-info`` folders, inside).'
                       Source = 'https://www.python.org/downloads/source/ (CPython 3.14); this build: Hookshot''s builder parts, https://github.com/NotRexed/Hookshot/releases/tag/builder-parts'
                       Inside = @('/python/LICENSE\.txt$', '\.dist-info/') }
    'cpm'         = @{ Licence = 'Libraries for the game''s build: dav1d 1.5.4, BSD-2-Clause (``dav1d/*/COPYING`` inside); SPIRV-Headers, MIT-style (``LICENSE`` inside); Vulkan-Headers 1.4.345, Apache-2.0 OR MIT (``LICENSE.md`` inside); VulkanMemoryAllocator 3.3.0, MIT (``LICENSE.txt`` inside); FFmpeg 9.0.2 static libraries, LGPL-2.1-or-later (configured without ``--enable-gpl``; no text inside, so ``LICENSE-ffmpeg-LGPL-2.1.txt`` here).'
                       Source = 'https://code.videolan.org/videolan/dav1d (1.5.4); https://github.com/KhronosGroup/SPIRV-Headers (vulkan-sdk-1.4.341.0); https://github.com/KhronosGroup/Vulkan-Headers (v1.4.345); https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator (v3.3.0); FFmpeg: https://github.com/FFmpeg/FFmpeg/tree/n9.0.2 (commit 946fcce07b, as the folder name says)'
                       Inside = @('/dav1d/[^/]+/COPYING$', '/spirv-headers/[^/]+/LICENSE$', '/vulkanheaders/[^/]+/LICENSE\.md$', '/vulkanmemoryallocator/[^/]+/LICENSE\.txt$', '/ffmpeg/9\.0\.2-[^/]*-946fcce07b-') }
    'wgpu-native' = @{ Licence = 'wgpu-native 29.0.1.1: MIT OR Apache-2.0 (``LICENSE.md`` inside).'
                       Source = 'https://github.com/gfx-rs/wgpu-native/releases/tag/v29.0.1.1'
                       Inside = @('/wgpu-native/LICENSE\.md$') }
    'third-party' = @{ Licence = 'Longshot''s prebuilt helpers (no licence text inside; given here): ``a64-kit/lift/a64emit.dll``, Longshot''s a64-kit lifter, GPL (its suyu translator is GPL-2.0-or-later; ``NOTICE-a64-kit.md`` here); ``a64-kit/tools/lld-link.exe``, LLVM''s lld 24.0.0git, Apache-2.0 WITH LLVM-exception (``LICENSE-llvm-Apache-2.0-with-LLVM-exception.txt`` here); ``sdl2/SDL2.dll`` and ``SDL3.dll``, sdl2-compat 2.32.74 and SDL 3.4.20, zlib (``LICENSE-sdl-zlib.txt`` here); ``zbic/zbic.dll``, zstd with Atmosphere-NX''s ZBIC parts, GPL-2.0 (``LICENSE-zbic-GPL-2.0.txt`` here).'
                       Source = "Longshot's source (``$srcZip`` here): ``third_party/a64-kit``, ``third_party/sdl2/PROVENANCE.md``, ``third_party/zbic``; lld: https://github.com/llvm/llvm-project at 510126255f89d693717c1ce7105b593f995f07c1 (its version string); SDL: https://github.com/libsdl-org/sdl2-compat (release-2.32.74), https://github.com/libsdl-org/SDL (release-3.4.20)"
                       Inside = @('/a64-kit/lift/a64emit\.dll$', '/a64-kit/tools/lld-link\.exe$', '/sdl2/SDL2\.dll$', '/sdl2/SDL3\.dll$', '/zbic/zbic\.dll$') }
    'source'      = @{ Licence = 'Longshot, GPL-3.0 (``LICENSE`` inside and here); the vendored parts under ``third_party/`` and ``gpu/`` keep their own notices inside.'
                       Source = "$lsLink (tag $($tp.release)); the same bytes as $api/archive/$($pin.commit).zip"; Inside = @('^[^/]+/LICENSE$') }
    'appimage'    = @{ Licence = "Longshot $($lx.tag)'s Linux builder, GPL-3.0 (``LICENSE`` here); what it bundles keeps its own licence, as Longshot publishes it."
                       Source = "``$srcZip`` here (the release tag's commit, $($pin.commit)); $up/releases/tag/$($lx.tag)"; Inside = @() }
}
function Get-AssetKind([string]$Name) {
    if ($Name -eq 'tools.lock') { return 'tools.lock' }
    if ($Name -eq $srcZip) { return 'source' }
    if ($Name -eq $lx.appimage) { return 'appimage' }
    if ($Name -match '^[0-9a-f]{64}-(.+)\.zip$') { return $Matches[1] }
    return ''
}
$entries = @{}
foreach ($a in $assets) {
    $k = Get-AssetKind $a.Name
    if (-not $info[$k]) { Stop-Mirror "$($a.Name): no licence and source written for '$k' in mirror_longshot.ps1. Look inside it and add them." }
    if ($a.Name -like '*.zip') { $entries[$a.Name] = @(& $tar -t -f (Join-Path $Out $a.Name)) }
    foreach ($re in $info[$k].Inside) {
        if (-not @($entries[$a.Name] | Where-Object { $_ -match $re })) { Stop-Mirror "$($a.Name) has no entry matching '$re': its licence line in mirror_longshot.ps1 is out of date." }
    }
}
$tmp = Join-Path $Out 'license.tmp'
foreach ($t in $texts) {
    $dest = Join-Path $Out $t.Name
    if ($t.Url) {
        $why = Get-KitDownload @($(if ($Feed) { "$Feed/$($t.Name)" } else { $t.Url })) $dest $t.Sha
        if ($why) { Stop-Mirror "$($t.Name): $why" }
    } else {
        $from = if ($t.From) { $t.From } else { @($assets | Where-Object { (Get-AssetKind $_.Name) -eq $t.Kind })[0].Name }
        if (-not $entries[$from]) { $entries[$from] = @(& $tar -t -f (Join-Path $Out $from)) }
        $entry = @($entries[$from] | Where-Object { $_ -match $t.Entry }) | Select-Object -First 1
        if (-not $entry) { Stop-Mirror "$from has no entry matching '$($t.Entry)' (for $($t.Name))." }
        Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
        New-Item -ItemType Directory -Force -Path $tmp | Out-Null
        & $tar -x -f (Join-Path $Out $from) -C $tmp $entry
        if ($LASTEXITCODE -ne 0) { Stop-Mirror "Could not take $entry out of $from." }
        Move-Item -LiteralPath (Join-Path $tmp ($entry -replace '/', '\')) -Destination $dest -Force
        Remove-Item -LiteralPath $tmp -Recurse -Force
    }
    $t.Sha256 = (Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash.ToLowerInvariant()
}

$utf8 = [System.Text.UTF8Encoding]::new($false)
$rows = ($assets | ForEach-Object { "| ``$($_.Name)`` | $($_.What) | ``$($_.Sha)`` |" }) -join "`n"
$each = ($assets | ForEach-Object {
    $i = $info[(Get-AssetKind $_.Name)]
    "### ``$($_.Name)```n`n- Licence: $($i.Licence)`n- Source: $($i.Source)"
}) -join "`n`n"
$textRows = ($texts | ForEach-Object { "| ``$($_.Name)`` | ``$($_.Sha256)`` |" }) -join "`n"
$readme = @"
# Longshot $($tp.release): co-op mirror

Exact copies of the files the OoT co-op kits download from Longshot's own servers, for players whose downloads from
git.magicbean.dev fail. The kits try Longshot's servers first and this mirror after them, and check every file against
the SHA-256 the kit pins (below), so a file here that differed would never be used. The files are unchanged.

None of these files are ours. Longshot is by Team MagicBean, GPL-3.0: $up (release $($tp.release), commit
$($pin.commit)). Each file's licences and source are listed below; licence texts a file does not carry itself are
given beside it here.

| File | What | SHA-256 |
| --- | --- | --- |
$rows

## Licences and sources

$each

## Licence texts given here

| File | SHA-256 |
| --- | --- |
$textRows
"@
[System.IO.File]::WriteAllText((Join-Path $Out 'README.md'), $readme.Replace("`r`n", "`n").Replace('``', '`'), $utf8)
$all = @($assets | ForEach-Object { "$($_.Sha)  $($_.Name)" }) + @($texts | ForEach-Object { "$($_.Sha256)  $($_.Name)" })
[System.IO.File]::WriteAllText((Join-Path $Out 'SHA256SUMS'), (($all -join "`n") + "`n"), $utf8)

$files = @($assets.Name) + @($texts | ForEach-Object { $_.Name }) + 'README.md', 'SHA256SUMS'
$args1 = ($files | ForEach-Object { "`"$Out\$_`"" }) -join ' '
Write-Host ''
Write-Host "All $($assets.Count) files are in $Out and match their pins, with $($texts.Count) licence texts, README.md and SHA256SUMS." -ForegroundColor Green
Write-Host 'To publish the mirror (as daurosk; outward-facing, so only after review). A pre-release that is never "latest":'
Write-Host "  & 'C:\Program Files\GitHub CLI\gh.exe' release create $tag --repo daurosk/oot-remake-multiplayer --target main --prerelease --latest=false --title `"Longshot $($tp.release) mirror (for the co-op kits)`" --notes-file `"$Out\README.md`" $args1"
Write-Host 'Then check it from the mirror:'
Write-Host "  powershell -NoProfile -ExecutionPolicy Bypass -File tools\release\mirror_longshot.ps1 -Out `"$Out-check`" -Feed $($tp.mirror)"
