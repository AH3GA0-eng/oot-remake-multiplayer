"""Problem reports: the co-op module's problems from a player's finished game sessions, masked, each problem sent once.

The kit's collect-logs (tools/release/kit/scripts/collect-logs.ps1) zips whole logs when a player asks. This is the small,
automatic side: from each game log (the game's longshot_log*.txt, or any launcher's log of this module), only the
module's problems (warnings, errors, faults the guard caught, refusals, crashes; routine counters left out), every line
masked (IP addresses in every form, the Windows user in paths, invite and answer codes, join passwords, player names),
each turned into a fingerprint (numbers and addresses blanked). Only fingerprints never sent before go out, with how
many times each came, so the same problem is never sent twice, across sessions too. A last gate drops any line or
message in which anything still looks like an IP address: nothing that may be one ever leaves the PC.

Off unless a player runs it with a webhook (Discord's JSON form, or any endpoint taking {"content": ...}):

    python -I tools/problem_reports/problem_reports.py --dry-run LOG [LOG ...]
    python -I tools/problem_reports/problem_reports.py --webhook URL --state FILE LOG [LOG ...]

--dry-run prints what would be sent and sends nothing. --state keeps the logs read and the problems sent (JSON), so a
second run sends nothing new. A log is read once: by its run_id line when it has one, else by its size and first bytes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import sys
import urllib.request

MAX_RUNS_KEPT = 200
MAX_FINGERPRINTS_KEPT = 5000
MAX_EVENTS_PER_RUN = 60
MESSAGE_LIMIT = 1900   # (Discord: 2000 characters a message)

# The problem lines: the module's own warnings and errors, faults the guard caught, refusals and crashes.
PROBLEM = re.compile(
    r"(warning: coop|error: coop|coop: .*(fail|could not|couldn't|refus|not taken|gave up|giving up|stuck|lost|timed out|"
    r"not written|not sent|damaged|mismatch|wrong)|coop: guard: a fault|CRASH|crash record|fatal error|unhandled exception)",
    re.IGNORECASE)
# Routine status lines that only contain such words (counters a second or every 10 s, notes of normal states)
ROUTINE = re.compile(r"coop: pose: |coop: actors: in \d+ s|invite code is made|camera lost, markers off|"
                     r"coop: flags: (by the game's events|native)|so far; |coop: objects: list head|/s.*/s")
RUN_ID = re.compile(r"run_id: ([0-9a-f]{16,})")
BUILD = re.compile(r"coop: build (?:hookshot )?(\S+), protocol (\d+)")

# IPv4 (any dotted quad, with or without a port) and IPv6 in every written form (full, "::" shortened, "::1", IPv4-mapped
# "::ffff:1.2.3.4", with a "%zone"), in brackets or URLs too. Over-masking (a version number, a clock time) is harmless.
IPV4 = re.compile(r"(?<![\d.])(?:\d{1,3}\.){3}\d{1,3}(?![\d.])(?::\d{1,5})?")
IPV6 = re.compile(r"(?<![\w:.])(?:[0-9A-Fa-f]{0,4}:){2,7}(?:(?:\d{1,3}\.){3}\d{1,3}|[0-9A-Fa-f]{0,4})(?:%[\w.]+)?(?![\w:])")
# The last gate: a line or message in which anything may still be an IP address is dropped whole (fail closed)
LEAK = re.compile(r"\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}|::|[0-9A-Fa-f]{1,4}:[0-9A-Fa-f]{1,4}:[0-9A-Fa-f]{1,4}")
USER_PATH = re.compile(r"([A-Za-z]:[\\/]+Users[\\/]+)[^\\/\s'\"]+", re.IGNORECASE)
HOME_PATH = re.compile(r"(/home/|/Users/)[^/\s'\"]+")
# invite and answer codes (letters of the codes' alphabet in groups), relay join codes, and long secrets
CODE = re.compile(r"\b[A-Z2-9]{4,}(?:-[A-Z2-9]{4,}){2,}\b|\b[A-Za-z0-9_\-]{24,}\b")
PASSWORD = re.compile(r"(password[^:=\n]*[:=]\s*)\S+", re.IGNORECASE)
PLAYER = [
    re.compile(r"(player \d+ )'[^']*'"),
    re.compile(r"(player \d+ (?:joined|left): )[^\s(]+"),
    re.compile(r"(name )'[^']*'"),
    re.compile(r"(notice: )\S+( got)"),
    re.compile(r"(P\d+ \()[^)]*(\))"),
]


def mask(line: str) -> str:
    """A log line with no IP address, user name, code, password or player name left in it."""
    s = USER_PATH.sub(r"\1[user]", line)
    s = HOME_PATH.sub(r"\1[user]", s)
    s = PASSWORD.sub(r"\1[hidden]", s)
    s = IPV6.sub("[ip]", s)   # (first: an IPv4-mapped one keeps its dotted quad inside)
    s = IPV4.sub("[ip]", s)
    s = CODE.sub("[code]", s)
    for p in PLAYER:
        s = p.sub(lambda m: m.group(1) + "[player]" + (m.group(2) if m.lastindex and m.lastindex >= 2 else ""), s)
    return s


def fingerprint(masked: str) -> str:
    """The problem a masked line is, the same for every occurrence: the thread tag, numbers and addresses blanked."""
    s = re.sub(r"^\[[^\]]*\]\s*", "", masked)
    s = re.sub(r"0x[0-9a-fA-F]+|\b[0-9a-f]{8,}\b", "#", s)
    s = re.sub(r"\d+(\.\d+)?", "#", s)
    return hashlib.sha1(s.encode("utf-8", "replace")).hexdigest()[:16]


def load_state(path):
    try:
        with open(path, encoding="utf-8") as f:
            st = json.load(f)
        if isinstance(st, dict):
            return {"runs": list(st.get("runs", [])), "sent": dict(st.get("sent", {}))}
    except (OSError, ValueError, TypeError):
        pass
    return {"runs": [], "sent": {}}


def save_state(path, st):
    st["runs"] = st["runs"][-MAX_RUNS_KEPT:]
    if len(st["sent"]) > MAX_FINGERPRINTS_KEPT:
        st["sent"] = dict(list(st["sent"].items())[-MAX_FINGERPRINTS_KEPT:])
    tmp = path + ".new"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(st, f)
    os.replace(tmp, path)


def read_run(log_path: str) -> dict | None:
    """One session's log: its key, the module's build and its problems by fingerprint ([masked line, count])."""
    try:
        with open(log_path, "rb") as f:
            raw = f.read()
    except OSError:
        return None
    text = raw.decode("utf-8", "replace")
    m = RUN_ID.search(text[:20000])
    key = m.group(1) if m else "file-" + hashlib.sha1(str(len(raw)).encode() + raw[:65536]).hexdigest()[:20]
    build = BUILD.search(text)
    events: dict[str, list] = {}
    for line in text.splitlines():
        if PROBLEM.search(line) and not ROUTINE.search(line):
            masked = mask(line.strip())
            if LEAK.search(masked):   # (anything still like an address: the line is not sent at all)
                continue
            fp = fingerprint(masked)
            if fp in events:
                events[fp][1] += 1
            else:
                events[fp] = [masked[:300], 1]
    return {"key": key, "module": build.group(1) if build else "?", "protocol": build.group(2) if build else "?",
            "events": events}


def messages(run: dict) -> list[str]:
    head = mask("Problem report: module %s (protocol %s), %d new problem(s)" % (run["module"], run["protocol"],
                                                                                len(run["events"])))
    lines = ["%dx %s" % (n, line) for line, n in list(run["events"].values())[:MAX_EVENTS_PER_RUN]]
    msgs, cur = [], head
    for l in lines:
        piece = "\n`" + l.replace("`", "'")[:MESSAGE_LIMIT - 10] + "`"
        if len(cur) + len(piece) > MESSAGE_LIMIT:
            msgs.append(cur)
            cur = head + " (continued)"
        cur += piece
    msgs.append(cur)
    return [m for m in msgs if not LEAK.search(m)]


def post(url: str, content: str) -> bool:
    body = json.dumps({"content": content, "username": "Co-op problem reports", "allowed_mentions": {"parse": []}}).encode()
    req = urllib.request.Request(url, data=body, headers={"Content-Type": "application/json", "User-Agent": "coop-reports"})
    try:
        with urllib.request.urlopen(req, timeout=15) as r:
            return 200 <= r.status < 300
    except OSError:
        return False


def report(logs, state_path=None, url="", dry_run=False, out=print, send=post) -> int:
    """The new problems of the logs not read before: printed (dry run) or sent to `url`. Returns how many problems."""
    st = load_state(state_path) if state_path else {"runs": [], "sent": {}}
    total = 0
    for path in logs:
        run = read_run(path)
        if not run or run["key"] in st["runs"]:
            continue
        run["events"] = {fp: e for fp, e in run["events"].items() if fp not in st["sent"]}
        if run["events"]:
            for msg in messages(run):
                if dry_run or not url:
                    out(msg)
                elif not send(url, msg):
                    out("problem reports: the report address did not answer; the rest are tried again next time")
                    if state_path:
                        save_state(state_path, st)
                    return total
        if not dry_run and url:
            for fp in run["events"]:
                st["sent"][fp] = run["key"]
            st["runs"].append(run["key"])
        total += len(run["events"])
    if state_path and not dry_run and url:
        save_state(state_path, st)
    return total


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("logs", nargs="+", help="game logs (longshot_log*.txt or another launcher's log of this module)")
    ap.add_argument("--webhook", default="", help="where reports go (Discord's JSON form); none: nothing is sent")
    ap.add_argument("--state", help="JSON file of the logs read and the problems sent (no second send)")
    ap.add_argument("--dry-run", action="store_true", help="print what would be sent, send nothing")
    a = ap.parse_args(argv)
    n = report(a.logs, a.state, a.webhook, a.dry_run)
    print("problem reports: %d new problem(s) %s" % (n, "found (dry run)" if a.dry_run or not a.webhook else "sent"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
