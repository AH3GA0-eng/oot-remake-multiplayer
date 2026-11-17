# Problem reports (docs/problem-reports.md): the co-op module's problems from the player's finished game sessions, masked
# as collect-logs masks them (log-mask.ps1 Protect-LogText: addresses, MAC addresses, the user and PC names, invite codes,
# passwords), each problem in one report only, written as coop-report-<date>.zip in the kit folder for the player to send
# with the /report bot command. Nothing is sent from here: no webhook, no network.
#
# Opt-in, asked once: start.ps1 calls it with -AtStart, which asks the first time (default no) and remembers the answer in
# logs\problem-reports.json; with yes, each start makes a zip when the sessions since the last one had a new problem.
# problem-report.cmd (-Now) makes one at once, whatever the answer (the player asked for it).
# Dot-source it for its functions (tools\release\tests\problem_report_test.ps1); run as a script for the above.
param([switch]$AtStart, [switch]$Now)

# The module's problems: its own warnings and errors, lines that say something failed, was refused, gave up, timed out or
# was lost, faults the guard caught, and crashes.
$script:ProblemLine = '(?i)(warning: coop|error: coop|coop: .*(fail|could not|couldn''t|refus|not taken|gave up|giving up|' +
    'stuck|lost|timed out|not written|not sent|damaged|mismatch|wrong)|coop: guard: a fault|CRASH|crash record|' +
    'fatal error|unhandled exception)'
# Routine status lines that only contain such words (counters a second or every 10 s, notes of normal states)
$script:ProblemRoutine = 'coop: pose: |coop: actors: in \d+ s|invite code is made|camera lost, markers off|' +
    'coop: flags: (by the game''s events|native)|so far; |coop: objects: list head|/s.*/s'

function Get-ProblemStateFile([string]$Kit) { Join-Path $Kit 'logs\problem-reports.json' }

# The answer (on, off, or '' when never asked), the sessions read and the problems reported, from the state file.
function Read-ProblemState([string]$Path) {
    $state = @{ choice = ''; runs = @(); sent = @{} }
    try {
        $j = Get-Content -LiteralPath $Path -Raw -ErrorAction Stop | ConvertFrom-Json
        if ($j.choice -in @('on', 'off')) { $state.choice = $j.choice }
        if ($j.runs) { $state.runs = @($j.runs) }
        if ($j.sent) { foreach ($p in $j.sent.PSObject.Properties) { $state.sent[$p.Name] = [string]$p.Value } }
    } catch { }
    $state
}

function Write-ProblemState([string]$Path, [hashtable]$State) {
    $dir = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $runs = @($State.runs | Select-Object -Last 200)
    $sent = [ordered]@{}
    foreach ($k in @($State.sent.Keys | Select-Object -Last 5000)) { $sent[$k] = $State.sent[$k] }
    $json = [ordered]@{ choice = $State.choice; runs = $runs; sent = $sent } | ConvertTo-Json -Depth 3
    $tmp = "$Path.new"
    [System.IO.File]::WriteAllText($tmp, $json, (New-Object System.Text.UTF8Encoding $false))
    Move-Item -LiteralPath $tmp -Destination $Path -Force
}

function Test-ProblemLine([string]$Line) { $Line -match $script:ProblemLine -and $Line -notmatch $script:ProblemRoutine }

# The problem a masked line is, the same for every occurrence: its thread tag, the masks' numbers (<lan-1>, <lan-2>),
# addresses and numbers blanked.
function Get-ProblemFingerprint([string]$Masked) {
    $s = $Masked -replace '^\[[^\]]*\]\s*', ''
    $s = $s -replace '<([a-z]+)-\d+>', '<$1>'
    $s = $s -replace '0x[0-9a-fA-F]+|\b[0-9a-f]{8,}\b', '#'
    $s = $s -replace '\d+(\.\d+)?', '#'
    $sha = [System.Security.Cryptography.SHA1]::Create()
    try { -join ($sha.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($s))[0..7] | ForEach-Object { $_.ToString('x2') }) }
    finally { $sha.Dispose() }
}

# A last gate after masking: anything Protect-LogText would still call an address (a mask it missed) drops the line.
function Test-ProblemHasAddress([string]$Masked) {
    foreach ($m in [regex]::Matches($Masked, '(?<![\d.])\d{1,3}(?:\.\d{1,3}){3}(?![\d])')) {
        if (Get-MaskKind4 $m.Value) { return $true }
    }
    foreach ($m in [regex]::Matches($Masked, '(?<![\w:.])(?:[0-9A-Fa-f]{0,4}:){2,7}(?:\d{1,3}(?:\.\d{1,3}){3}|[0-9A-Fa-f]{0,4})(?:%[\w.-]+)?')) {
        if (Get-MaskKind6 $m.Value) { return $true }
    }
    $false
}

# One session's log: its key (its run_id line, else its size and first bytes) and its problems, masked.
function Read-ProblemRun([string]$Path, [hashtable]$Mask, [string]$UserName = $env:USERNAME, [string]$PcName = $env:COMPUTERNAME) {
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    $text = [System.Text.Encoding]::UTF8.GetString($bytes)
    $head = $text.Substring(0, [Math]::Min(20000, $text.Length))
    if ($head -match 'run_id: ([0-9a-f]{16,})') { $key = $Matches[1] }
    else {
        $sha = [System.Security.Cryptography.SHA1]::Create()
        try {
            $part = $bytes[0..([Math]::Min(65535, $bytes.Length - 1))]
            $key = 'file-' + $bytes.Length + '-' + (-join ($sha.ComputeHash([byte[]]$part)[0..9] | ForEach-Object { $_.ToString('x2') }))
        } finally { $sha.Dispose() }
    }
    $events = [ordered]@{}
    foreach ($line in ($text -split "`r?`n")) {
        if (-not (Test-ProblemLine $line)) { continue }
        $masked = (Protect-LogText $line.Trim() $Mask $UserName $PcName)
        if (Test-ProblemHasAddress $masked) { continue }
        $fp = Get-ProblemFingerprint $masked
        if ($events.Contains($fp)) { $events[$fp].Count++ }
        else { $events[$fp] = @{ Line = $masked.Substring(0, [Math]::Min(300, $masked.Length)); Count = 1 } }
    }
    @{ Key = $key; Events = $events }
}

# The new problems of the logs not read before, as a zip in $OutDir (coop-report-<date>.zip), or $null when there is none.
# The state's runs and sent are brought up to date (the caller writes it).
function New-ProblemReport([string[]]$Logs, [hashtable]$State, [string]$OutDir, [string]$Version = '',
                           [string]$UserName = $env:USERNAME, [string]$PcName = $env:COMPUTERNAME) {
    $mask = New-MaskState
    $new = [ordered]@{}
    $read = @()
    foreach ($log in $Logs) {
        if (-not (Test-Path -LiteralPath $log)) { continue }
        $run = Read-ProblemRun $log $mask $UserName $PcName
        if ($State.runs -contains $run.Key -or $read -contains $run.Key) { continue }
        $read += $run.Key
        foreach ($fp in $run.Events.Keys) {
            if ($State.sent.ContainsKey($fp)) { continue }
            if ($new.Contains($fp)) { $new[$fp].Count += $run.Events[$fp].Count }
            else { $new[$fp] = $run.Events[$fp] }
        }
    }
    $State.runs = @($State.runs) + $read
    if ($new.Count -eq 0) { return $null }
    $stage = Join-Path ([System.IO.Path]::GetTempPath()) ("coop-report-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    try {
        $lines = @("Squadala! problem report: $($new.Count) new problem(s) from $($read.Count) session(s)",
            "Kit: $(Protect-LogText $Version $mask $UserName $PcName)",
            'Addresses, names, codes and passwords are masked (as collect-logs masks them). Send with /report.', '')
        foreach ($e in @($new.Values | Sort-Object { -$_.Count })) { $lines += ('{0}x {1}' -f $e.Count, $e.Line) }
        [System.IO.File]::WriteAllText((Join-Path $stage 'problem-report.txt'), ($lines -join "`r`n") + "`r`n",
            (New-Object System.Text.UTF8Encoding $false))
        $zip = Join-Path $OutDir ('coop-report-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '.zip')
        Write-LogZip $stage $zip
    } finally {
        Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    }
    foreach ($fp in $new.Keys) { $State.sent[$fp] = (Get-Date -Format 'yyyy-MM-dd') }
    @{ Zip = $zip; Count = $new.Count }
}

# The game's finished sessions' logs, where collect-logs looks (Get-KitLogGameExes, Get-KitHookshotLogDirs): the running
# session's own longshot_log.txt is left for next time (it is still being written).
function Get-ProblemLogs([string]$Kit) {
    $files = @()
    foreach ($exe in @(Get-KitLogGameExes $Kit)) {
        foreach ($dir in @(Get-KitHookshotLogDirs $exe)) {
            $files += @(Get-KitHookshotLogFiles $dir | ForEach-Object { $_.File })
        }
    }
    $files += @(Get-ChildItem (Join-Path $Kit 'logs') -Filter 'longshot_log*.txt' -File -ErrorAction SilentlyContinue)
    $running = @(Get-Process longshot, hookshot -ErrorAction SilentlyContinue).Count -gt 0
    @($files | Where-Object { -not ($running -and $_.Name -match '^(longshot|hookshot)_log\.txt$') } |
        Sort-Object LastWriteTime | ForEach-Object { $_.FullName } | Select-Object -Unique)
}

if ($MyInvocation.InvocationName -eq '.') { return }   # (dot-sourced: the functions only)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'log-mask.ps1')
. (Join-Path $PSScriptRoot 'kit-paths.ps1')
$kit = if ($env:OOT_COOP_KIT) { $env:OOT_COOP_KIT } else { Split-Path -Parent $PSScriptRoot }
$stateFile = Get-ProblemStateFile $kit
$state = Read-ProblemState $stateFile
if (-not $state.choice) {
    $interactive = [Environment]::UserInteractive -and -not [Console]::IsInputRedirected
    if (-not $interactive) { return }
    Write-Host ''
    Write-Host 'Problem reports: after your games, the kit can put the co-op problems it finds in your logs into a small zip'
    Write-Host 'for you to send with /report in the Squadala! Discord. Addresses, names and codes are masked; nothing is'
    Write-Host 'sent by itself. You can change this later in logs\problem-reports.json (choice: on or off).'
    $answer = Read-Host 'Make problem reports? [y/N]'
    $state.choice = if ($answer -and $answer.Trim() -match '^(?i)(y|yes)$') { 'on' } else { 'off' }
    Write-ProblemState $stateFile $state
}
if ($AtStart -and $state.choice -ne 'on') { return }
$version = ''
try { $version = (Get-Content -LiteralPath (Join-Path $kit 'VERSION.txt') -TotalCount 1 -ErrorAction Stop) } catch { }
$report = New-ProblemReport @(Get-ProblemLogs $kit) $state $kit $version
Write-ProblemState $stateFile $state
if ($report) {
    Write-Host "Problem report: $($report.Count) new problem(s) in $($report.Zip)"
    Write-Host 'Send it with /report in the Squadala! Discord (addresses, names and codes are masked in it).'
} elseif ($Now) {
    Write-Host 'Problem report: no new problem in your game logs since the last report.'
}
