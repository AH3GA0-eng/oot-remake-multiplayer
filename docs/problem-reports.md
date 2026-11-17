# Problem reports

`tools/problem_reports/problem_reports.py` sends the co-op module's problems from a player's finished game sessions to
one place the maintainer chooses (a Discord webhook, or any endpoint that takes `{"content": ...}`), so desyncs and
faults reach the project without a player zipping and sending whole logs. It is off unless it is run with a webhook.

It sits beside the kit's collect-logs (`tools/release/kit/scripts/collect-logs.ps1`), which a player runs to zip whole
logs, masked, for one report. Problem reports are the small, automatic side: a few lines a session, and only new ones.

## What is sent

From each game log (`longshot_log*.txt`, or another launcher's log of this module), only the module's problems: its
warnings and errors (`warning: coop`, `error: coop`), lines of the module that say something failed, could not be done,
was refused, gave up, timed out or was lost, faults the guard caught, and crashes. Routine status lines that only contain
such words (the pose and actor counters, `camera lost, markers off`, the invite note) are left out.

Each line is masked before anything else:

| In the log | Sent as |
|---|---|
| IP addresses, every form (IPv4 with or without a port; IPv6 full, `::` shortened, `::1`, IPv4-mapped, with a zone; in brackets or URLs) | `[ip]` |
| the Windows user in paths (`C:\Users\<name>\...`), a home folder (`/home/<name>/...`) | `[user]` |
| invite codes, answer codes, relay join codes, long secrets | `[code]` |
| join passwords | `[hidden]` |
| player names (joins and leaves, notices, puppet lines) | `[player]` |

A last gate, after masking: a line in which anything still looks like an IP address (a dotted quad, `::`, hex groups with
colons) is dropped whole, and so is any message that would contain one. Nothing that may be an address leaves the PC.

## Sent once

Each masked line becomes a fingerprint (its thread tag, numbers and addresses blanked), so the same problem at another
address, frame or count is the same fingerprint. A message lists the new fingerprints of one session with how many times
each came. `--state FILE` keeps the sessions read (by the log's `run_id`, else its size and first bytes) and the
fingerprints sent: the same session is never read twice, and a problem sent once is never sent again, from any later
session. An unreachable webhook keeps the rest for the next run.

## Running it

```
python -I tools/problem_reports/problem_reports.py --dry-run longshot_log.txt
python -I tools/problem_reports/problem_reports.py --webhook URL --state reports.json logs/longshot_log*.txt
```

`--dry-run` (or no `--webhook`) prints what would be sent and sends nothing.

A webhook address inside a kit or a launcher can be read by anyone who has it, who could then post to that channel; the
channel's owner can delete and make a new one if that happens. Players turn reports on themselves: nothing is sent unless
a player's kit or launcher runs this with the webhook.

Tests: `tools/problem_reports/tests/test_problem_reports.py` (made-up log lines only).
