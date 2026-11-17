# Tests for the kit's problem reports (tools/release/kit/scripts/problem-report.ps1): which lines are problems, the
# masking (collect-logs' Protect-LogText) and the last gate for addresses, one fingerprint per problem, a problem in one
# report only (across sessions too), the zip's contents, and the answer kept. Made-up log lines only (no game output).
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\release\tests\problem_report_test.ps1
# Exits 1 when a check fails.
$ErrorActionPreference = 'Stop'
$scripts = Join-Path (Split-Path -Parent $PSScriptRoot) 'kit\scripts'
. (Join-Path $scripts 'log-mask.ps1')
. (Join-Path $scripts 'problem-report.ps1')

$failed = 0
function Check([bool]$Ok, [string]$What) {
    if ($Ok) { Write-Host "ok   $What" } else { Write-Host "FAIL $What"; $script:failed++ }
}

$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("problem-report-test-" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp -Force | Out-Null
function New-Log([string]$Name, [string]$RunId, [string[]]$Lines) {
    $path = Join-Path $tmp $Name
    $text = (@("[hos host] run_id: $RunId") + $Lines) -join "`r`n"
    [System.IO.File]::WriteAllText($path, $text, (New-Object System.Text.UTF8Encoding $false))
    $path
}
$problems = @(
    '[hos sead::MainThread] coop: on, server 192.168.1.20:27510, name ''Saria'', join password set',
    '[hos sead::MainThread] coop: pose: Player_Child 309 bones, 0 moving; 0 sent/s, 0 received/s, 0 refused/s',
    '[hos Presentation Thread] coop: boss: could not move into the boss room of InsideTheDekuTree after 5 tries; giving up',
    '[hos Presentation Thread] coop: boss: could not move into the boss room of InsideTheDekuTree after 5 tries; giving up',
    '[hos host] coop: host: player 2 left: Mido (timed out: nothing heard for 10003 ms from 203.0.113.9:27510)',
    '[hos host] warning: coop: save: could not write C:\Users\Testuser\Games\coop\user\save\slot_01',
    '[hos host] coop: guard: a fault in co-op at 0x80021234 was caught (peer fe80::1c2b:3a4d:5e6f:7a8b%12)',
    '[hos host] coop: invite: could not copy: invite code: ABCDE-FGHJK-LMNPQ-RSTUV'
)

# Which lines are problems
Check (Test-ProblemLine $problems[2]) 'a failure is a problem'
Check (-not (Test-ProblemLine $problems[1])) 'a routine counter (refused/s) is not a problem'
Check (-not (Test-ProblemLine '[hos host] coop: host: player 2 joined: Mido')) 'a join is not a problem'

# One session: masked, one fingerprint per problem
$log1 = New-Log 'longshot_log.1.txt' '0123456789abcdef0123456789abcdef' $problems
$run = Read-ProblemRun $log1 (New-MaskState) 'Testuser' 'TESTPC-01'
Check ($run.Key -eq '0123456789abcdef0123456789abcdef') 'a session is known by its run_id'
$all = ($run.Events.Values | ForEach-Object { $_.Line }) -join "`n"
Check ($all -notmatch '192\.168|203\.0\.113|fe80|1c2b') 'no address is left in what is reported'
Check ($all -notmatch 'Testuser' -and $all -notmatch 'ABCDE-FGHJK') 'no user name or invite code is left'
Check ($all -match '<public-1>') 'an address becomes its kind (as collect-logs masks it)'
$boss = @($run.Events.Values | Where-Object { $_.Line -match 'giving up' })
Check ($boss.Count -eq 1 -and $boss[0].Count -eq 2) 'the same problem twice is one problem counted twice'
Check ((Get-ProblemFingerprint '[a] coop: x could not load slot 3 from <lan-1> after 12 ms') -eq
       (Get-ProblemFingerprint '[b] coop: x could not load slot 5 from <lan-2> after 40 ms')) 'a fingerprint ignores numbers and which address'

# The last gate: anything still an address drops the line
Check (Test-ProblemHasAddress 'coop: could not reach 10.0.0.5') 'a raw address is caught by the gate'
Check (Test-ProblemHasAddress 'coop: could not reach 2001:db8::1') 'a raw IPv6 address is caught by the gate'
Check (-not (Test-ProblemHasAddress 'coop: could not reach <lan-1> (version 1.1.1, 10.0.26200.0)')) 'masks and versions pass the gate'

# Reports: the new problems in a zip, each problem in one report only
$state = @{ choice = 'on'; runs = @(); sent = @{} }
$first = New-ProblemReport @($log1) $state $tmp 'Squadala! 0.11.3' 'Testuser' 'TESTPC-01'
Check ($null -ne $first -and (Test-Path -LiteralPath $first.Zip)) 'the first report is a zip'
Check ($first.Count -ge 5) "the first report has the problems ($($first.Count))"
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::OpenRead($first.Zip)
try {
    $entry = $zip.GetEntry('problem-report.txt')
    $reader = New-Object System.IO.StreamReader($entry.Open())
    try { $text = $reader.ReadToEnd() } finally { $reader.Dispose() }
} finally { $zip.Dispose() }
Check ($text -match 'Send with /report') 'the report says how to send it'
Check ($text -notmatch '192\.168|203\.0\.113|fe80|Testuser|ABCDE-FGHJK|Mido left') 'the zip holds no address, user name or code'
Check ($null -eq (New-ProblemReport @($log1) $state $tmp '' 'Testuser' 'TESTPC-01')) 'the same session is never reported again'
$log2 = New-Log 'longshot_log.2.txt' 'fedcba9876543210fedcba9876543210' $problems
Check ($null -eq (New-ProblemReport @($log2) $state $tmp '' 'Testuser' 'TESTPC-01')) 'the same problems from another session are not reported again'
$log3 = New-Log 'longshot_log.3.txt' '11112222333344445555666677778888' @('[hos host] coop: flags: could not apply a new flag')
$third = New-ProblemReport @($log3) $state $tmp '' 'Testuser' 'TESTPC-01'
Check ($null -ne $third -and $third.Count -eq 1) 'a new problem makes a new report with only it'

# The answer and what was reported are kept
$file = Join-Path $tmp 'logs\problem-reports.json'
Write-ProblemState $file $state
$back = Read-ProblemState $file
Check ($back.choice -eq 'on') 'the answer is kept'
Check ($back.runs.Count -eq 3 -and $back.sent.Count -eq $state.sent.Count) 'the sessions read and the problems reported are kept'
Check ((Read-ProblemState (Join-Path $tmp 'none.json')).choice -eq '') 'never asked: no answer (the start asks once)'

Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
if ($failed) { Write-Host "$failed check(s) failed"; exit 1 }
Write-Host 'problem report tests: all passed'
