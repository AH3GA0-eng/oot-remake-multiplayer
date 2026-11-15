# Gets the Longshot files the kits download (the pins in tools/release/kit/hookshot/TOOLS.txt, hookshot/PIN and
# hookshot/LINUX.txt), checks every one against its pinned SHA-256, adds LICENSE (Longshot's own, from its source),
# README.md and SHA256SUMS, and prints the gh commands that publish them as the co-op mirror: a pre-release of
# daurosk/oot-remake-multiplayer tagged longshot-mirror-<release>, which the kits try after Longshot's own servers
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

# Longshot's LICENSE (GPL-3.0) from its own source archive
$lic = Join-Path $Out 'LICENSE'
$tmp = Join-Path $Out 'license.tmp'
Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
& "$env:SystemRoot\System32\tar.exe" -x -f (Join-Path $Out "longshot-$($pin.commit).zip") -C $tmp --strip-components=1 'longshot/LICENSE' 2>$null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath "$tmp\LICENSE")) {
    $top = @(& "$env:SystemRoot\System32\tar.exe" -t -f (Join-Path $Out "longshot-$($pin.commit).zip") | Where-Object { $_ -match '^[^/]+/(LICENSE|COPYING)(\.\w+)?$' } | Select-Object -First 1)
    if (-not $top) { Stop-Mirror "Longshot's source archive has no LICENSE at its top." }
    & "$env:SystemRoot\System32\tar.exe" -x -f (Join-Path $Out "longshot-$($pin.commit).zip") -C $tmp --strip-components=1 $top[0]
    Move-Item -LiteralPath (Join-Path $tmp (Split-Path -Leaf $top[0])) -Destination "$tmp\LICENSE"
}
Move-Item -LiteralPath "$tmp\LICENSE" -Destination $lic -Force
Remove-Item -LiteralPath $tmp -Recurse -Force

$utf8 = [System.Text.UTF8Encoding]::new($false)
$rows = ($assets | ForEach-Object { "| ``$($_.Name)`` | $($_.What) | ``$($_.Sha)`` |" }) -join "`n"
$readme = @"
# Longshot $($tp.release): co-op mirror

Exact copies of the files the OoT co-op kits download from Longshot's own servers, for players whose downloads from
git.magicbean.dev fail. The kits try Longshot's servers first and this mirror after them, and check every file against
the SHA-256 the kit pins (below), so a file here that differed would never be used.

These files are not ours. Longshot is by Team MagicBean, licensed GPL-3.0 (LICENSE, from its source):
$up (release $($tp.release), commit $($pin.commit)). The source the Linux builder and the tools are built from is
``longshot-$($pin.commit).zip`` here, the same archive as $api/archive/$($pin.commit).zip.
Each tool zip is unchanged as Longshot publishes it (tools.lock) and carries its own project's licence files.

| File | What | SHA-256 |
| --- | --- | --- |
$rows
"@
[System.IO.File]::WriteAllText((Join-Path $Out 'README.md'), $readme.Replace("`r`n", "`n"), $utf8)
[System.IO.File]::WriteAllText((Join-Path $Out 'SHA256SUMS'), ((($assets | ForEach-Object { "$($_.Sha)  $($_.Name)" }) -join "`n") + "`n"), $utf8)

$files = @($assets.Name) + 'LICENSE', 'README.md', 'SHA256SUMS'
$args1 = ($files | ForEach-Object { "`"$Out\$_`"" }) -join ' '
Write-Host ''
Write-Host "All $($assets.Count) files are in $Out and match their pins." -ForegroundColor Green
Write-Host 'To publish the mirror (as daurosk; outward-facing, so only after review):'
Write-Host "  & 'C:\Program Files\GitHub CLI\gh.exe' release create $tag --repo daurosk/oot-remake-multiplayer --target main --prerelease --title `"Longshot $($tp.release) mirror (for the co-op kits)`" --notes-file `"$Out\README.md`" $args1"
Write-Host 'Then check it from the mirror:'
Write-Host "  powershell -NoProfile -ExecutionPolicy Bypass -File tools\release\mirror_longshot.ps1 -Out `"$Out-check`" -Feed $($tp.mirror)"
