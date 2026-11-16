"""Bug-report bot: everything that does not talk to Discord.

Config loading, the attachment checks, the per-user rate limit, the report
folder name and the tracker row. Standard library only, so the unit tests run
without discord.py and without a network.
"""

import datetime as _dt
import json
import os
import re
import tempfile
import time
from pathlib import Path

MAX_ZIP_BYTES = 25 * 1024 * 1024
MAX_DESCRIPTION = 1500
# Local file headers / empty-archive marker. Anything else is not a zip.
ZIP_MAGICS = (b"PK\x03\x04", b"PK\x05\x06")
TRACKER_NAME = "player-reports.md"
STATUS_NEW = "NEW (bot)"

_ROW_RE = re.compile(r"^\|\s*(\d+)\s*\|")


class ConfigError(Exception):
    """The config file is missing or wrong. The message never contains the token."""


def load_config(path):
    """Read secrets/discord-bot.json. Returns a dict with typed values.

    The coordination folder is the config file's grandparent
    (coord-multiplayer/secrets/discord-bot.json) unless "coord_dir" is given.
    """
    path = Path(path)
    if not path.is_file():
        raise ConfigError(f"config file not found: {path}")
    try:
        raw = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as e:
        raise ConfigError(f"config file is not valid JSON: {path} ({type(e).__name__})")
    if not isinstance(raw, dict):
        raise ConfigError("config file must hold one JSON object")

    token = raw.get("token")
    if not isinstance(token, str) or len(token.strip()) < 20:
        raise ConfigError("config: 'token' is missing or too short")

    def snowflake(key, required):
        v = raw.get(key)
        if v in (None, "", 0):
            if required:
                raise ConfigError(f"config: '{key}' is missing")
            return None
        try:
            n = int(str(v).strip())
        except ValueError:
            raise ConfigError(f"config: '{key}' must be a Discord id (digits)")
        if n <= 0:
            raise ConfigError(f"config: '{key}' must be a Discord id (digits)")
        return n

    coord = raw.get("coord_dir") or str(path.resolve().parent.parent)
    return {
        "token": token.strip(),
        "guild_id": snowflake("guild_id", True),
        "dev_channel_id": snowflake("dev_channel_id", True),
        "report_channel_id": snowflake("report_channel_id", False),
        "coord_dir": Path(coord),
    }


def check_attachment_meta(filename, size, max_bytes=MAX_ZIP_BYTES):
    """Check an attachment before downloading it. Returns None if fine, else a reason
    the reporter can read."""
    if not filename or not filename.lower().endswith(".zip"):
        return "Only a .zip file can be attached (use the kit's collect-logs zip)."
    if size is None or size <= 0:
        return "The attached file is empty."
    if size > max_bytes:
        return f"The zip is too big ({size // (1024 * 1024)} MB); the limit is {max_bytes // (1024 * 1024)} MB."
    return None


def check_zip_bytes(data, max_bytes=MAX_ZIP_BYTES):
    """Check the downloaded bytes: size again and the zip signature. Never unzips."""
    if len(data) == 0:
        return "The attached file is empty."
    if len(data) > max_bytes:
        return f"The zip is too big; the limit is {max_bytes // (1024 * 1024)} MB."
    if not data.startswith(ZIP_MAGICS):
        return "The attached file is not a real zip."
    return None


def clean_description(text, limit=MAX_DESCRIPTION):
    """Strip control characters (keeping newlines and tabs) and cut to the limit."""
    text = "".join(c for c in (text or "") if c in "\n\t" or (ord(c) >= 32 and ord(c) != 127))
    text = text.strip()
    if len(text) > limit:
        text = text[: limit - 3].rstrip() + "..."
    return text


def safe_name(username, fallback="player"):
    """A Discord username as a folder-safe, lowercase name."""
    s = re.sub(r"[^a-z0-9._-]+", "-", (username or "").lower())
    s = re.sub(r"-{2,}", "-", s).strip("-._")
    return s[:32].rstrip("-._") or fallback


def folder_name(username, when):
    """<user>-<YYYYMMDD-HHMMSS> in UTC."""
    return f"{safe_name(username)}-{when.astimezone(_dt.timezone.utc):%Y%m%d-%H%M%S}"


def unique_dir(base, name):
    """base/name, or base/name-2, -3 ... if that folder exists. Creates it."""
    base = Path(base)
    base.mkdir(parents=True, exist_ok=True)
    n = 1
    while True:
        d = base / (name if n == 1 else f"{name}-{n}")
        try:
            d.mkdir()
            return d
        except FileExistsError:
            n += 1


def table_cell(text, limit=600):
    """One markdown table cell: no pipes, no newlines."""
    s = re.sub(r"\s+", " ", (text or "").replace("|", "/")).strip()
    if len(s) > limit:
        s = s[: limit - 3].rstrip() + "..."
    return s or "-"


def next_report_number(tracker_text):
    nums = [int(m.group(1)) for line in tracker_text.splitlines() if (m := _ROW_RE.match(line))]
    return max(nums, default=0) + 1


def format_row(number, when, player, version, description, logs_rel):
    """A row in the tracker's format:
    | # | Reported (UTC) | Player | Version | Report | Status | Worker / fix | GitHub # |"""
    report = table_cell(description)
    if logs_rel:
        report += f" (logs: {logs_rel})"
    else:
        report += " (no logs attached)"
    reported = f"{when.astimezone(_dt.timezone.utc):%m-%d %H:%M} UTC (Discord /report)"
    cells = [str(number), reported, table_cell(player, 64), table_cell(version, 32) if version else "?",
             report, STATUS_NEW, "-", "-"]
    return "| " + " | ".join(cells) + " |"


def append_row(tracker_path, build_row):
    """Add a row after the tracker's last numbered row.

    build_row(number) -> the row text. Returns the number used. Keeps the file's
    encoding (UTF-8, with the BOM if it had one) and appends in place when the
    table is at the end of the file (the usual case), so a concurrent edit of the
    rest of the file is never overwritten.
    """
    tracker_path = Path(tracker_path)
    raw = tracker_path.read_bytes() if tracker_path.exists() else b""
    bom = raw.startswith(b"\xef\xbb\xbf")
    text = raw.decode("utf-8-sig")
    number = next_report_number(text)
    row = build_row(number)

    lines = text.splitlines(keepends=True)
    last = max((i for i, l in enumerate(lines) if _ROW_RE.match(l)), default=None)
    if last is None:
        # No table yet (or no file): start one at the end.
        tail = ""
        head = text + ("" if (not text or text.endswith("\n")) else "\n")
    else:
        head = "".join(lines[: last + 1])
        tail = "".join(lines[last + 1:])
        if not head.endswith("\n"):
            head += "\n"
    if not tail:
        # The table ends the file (the usual case): append in place, so an edit
        # of the rest of the file made at the same moment is never overwritten.
        with open(tracker_path, "ab") as f:
            f.write((head[len(text):] + row + "\n").encode("utf-8"))
        return number
    _write_atomic(tracker_path, head + row + "\n" + tail, bom)
    return number


def _write_atomic(path, text, bom):
    data = (b"\xef\xbb\xbf" if bom else b"") + text.encode("utf-8")
    fd, tmp = tempfile.mkstemp(dir=str(path.parent), prefix=".tracker-", suffix=".tmp")
    try:
        with os.fdopen(fd, "wb") as f:
            f.write(data)
        os.replace(tmp, path)
    except BaseException:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def description_file(number, when, username, display_name, version, attachment_name, description):
    lines = [
        f"Report #{number}",
        f"Received (UTC): {when.astimezone(_dt.timezone.utc):%Y-%m-%d %H:%M:%S}",
        f"Discord user: {username}" + (f" ({display_name})" if display_name and display_name != username else ""),
        f"Kit version: {version or '?'}",
        f"Attachment: {attachment_name or '(none)'}" + (" -> logs.zip" if attachment_name else ""),
        "",
        description,
        "",
    ]
    return "\n".join(lines)


class RateLimiter:
    """At most `limit` reports per user in `window` seconds (sliding window)."""

    def __init__(self, limit=3, window=3600.0, clock=time.monotonic):
        self.limit = limit
        self.window = window
        self.clock = clock
        self._hits = {}

    def retry_after(self, user_id):
        """Seconds until this user may report again; 0 if allowed now."""
        now = self.clock()
        hits = [t for t in self._hits.get(user_id, []) if now - t < self.window]
        self._hits[user_id] = hits
        if len(hits) < self.limit:
            return 0.0
        return max(0.0, self.window - (now - hits[0]))

    def record(self, user_id):
        self._hits.setdefault(user_id, []).append(self.clock())
