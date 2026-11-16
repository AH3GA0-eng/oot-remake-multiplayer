"""Unit tests for the bug-report bot's core (no discord.py, no network)."""

import datetime as dt
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import report_core as core  # noqa: E402

WHEN = dt.datetime(2026, 11, 16, 21, 5, 9, tzinfo=dt.timezone.utc)
HEADER = (
    "\ufeff# Player reports tracker\n\n"
    "| # | Reported (UTC) | Player | Version | Report | Status | Worker / fix | GitHub # |\n"
    "|---|---|---|---|---|---|---|---|\n"
    "| 1 | 11-15 ~02:40 | A | 0.10.1 | first | IN 0.11.1 | x | - |\n"
    "| 122 | 11-16 ~15:06 UTC | Geno | 0.11.3 | q | ANSWERED | y | #245 |\n"
)


class TempDir(unittest.TestCase):
    def setUp(self):
        self._td = tempfile.TemporaryDirectory()
        self.dir = Path(self._td.name)

    def tearDown(self):
        self._td.cleanup()


class RowFormat(unittest.TestCase):
    def test_row_has_eight_cells_and_status(self):
        row = core.format_row(123, WHEN, "bean_seller", "0.11.3", "Gohma | stuck\nafter intro", "player-logs/x")
        self.assertTrue(row.startswith("| 123 | 11-16 21:05 UTC (Discord /report) | bean_seller | 0.11.3 | "))
        cells = row.strip("|").split(" | ")
        self.assertEqual(len(cells), 8)
        self.assertIn("Gohma / stuck after intro (logs: player-logs/x)", row)
        self.assertEqual(cells[5].strip(), "NEW (bot)")
        self.assertNotIn("\n", row)

    def test_no_version_no_logs(self):
        row = core.format_row(5, WHEN, "p", None, "desc", None)
        self.assertIn("| p | ? | desc (no logs attached) | NEW (bot) | - | - |", row)

    def test_long_description_cut(self):
        row = core.format_row(5, WHEN, "p", "v", "x" * 2000, None)
        self.assertLess(len(row), 800)
        self.assertIn("...", row)

    def test_next_number(self):
        self.assertEqual(core.next_report_number(HEADER), 123)
        self.assertEqual(core.next_report_number("no table"), 1)


class AppendRow(TempDir):
    def test_append_at_end_keeps_bom_and_rows(self):
        p = self.dir / "player-reports.md"
        p.write_bytes(HEADER.encode("utf-8"))
        n = core.append_row(p, lambda n: core.format_row(n, WHEN, "u", "0.11.3", "d", None))
        self.assertEqual(n, 123)
        raw = p.read_bytes()
        self.assertTrue(raw.startswith(b"\xef\xbb\xbf"))
        self.assertEqual(raw.count(b"\xef\xbb\xbf"), 1)
        text = raw.decode("utf-8-sig")
        self.assertTrue(text.startswith(HEADER[1:]))
        self.assertTrue(text.endswith(" | NEW (bot) | - | - |\n"))
        self.assertEqual(core.append_row(p, lambda n: f"| {n} | r |"), 124)
        self.assertTrue(p.read_text(encoding="utf-8-sig").endswith("| 123 |" + text.split("| 123 |")[1] + "| 124 | r |\n"))

    def test_insert_before_text_after_table(self):
        p = self.dir / "player-reports.md"
        p.write_text(HEADER[1:] + "\nNotes below the table.\n", encoding="utf-8")
        core.append_row(p, lambda n: f"| {n} | r |")
        lines = p.read_text(encoding="utf-8").splitlines()
        i = lines.index("| 123 | r |")
        self.assertTrue(lines[i - 1].startswith("| 122 |"))
        self.assertEqual(lines[-1], "Notes below the table.")
        self.assertFalse(p.read_bytes().startswith(b"\xef\xbb\xbf"))

    def test_no_trailing_newline(self):
        p = self.dir / "player-reports.md"
        p.write_text(HEADER[1:].rstrip("\n"), encoding="utf-8")
        core.append_row(p, lambda n: f"| {n} | r |")
        self.assertTrue(p.read_text(encoding="utf-8").endswith("| #245 |\n| 123 | r |\n"))

    def test_missing_file(self):
        p = self.dir / "player-reports.md"
        self.assertEqual(core.append_row(p, lambda n: f"| {n} | r |"), 1)
        self.assertEqual(p.read_text(encoding="utf-8"), "| 1 | r |\n")


class Folders(TempDir):
    def test_folder_name(self):
        self.assertEqual(core.folder_name("Bean.Seller", WHEN), "bean.seller-20261116-210509")
        local = WHEN.astimezone(dt.timezone(dt.timedelta(hours=-5)))
        self.assertEqual(core.folder_name("x", local), "x-20261116-210509")

    def test_safe_name_blocks_paths(self):
        for bad in ("../../etc", "..\\..\\win", "a/b", "C:\\x", "con:", "  "):
            s = core.safe_name(bad)
            self.assertNotIn("/", s)
            self.assertNotIn("\\", s)
            self.assertNotIn(":", s)
            self.assertFalse(s.startswith("."))
            self.assertTrue(s)
        self.assertEqual(core.safe_name("Chu!"), "chu")
        self.assertEqual(core.safe_name("日本"), "player")
        self.assertLessEqual(len(core.safe_name("a" * 100)), 32)

    def test_unique_dir(self):
        a = core.unique_dir(self.dir / "logs", "u-1")
        b = core.unique_dir(self.dir / "logs", "u-1")
        self.assertEqual(a.name, "u-1")
        self.assertEqual(b.name, "u-1-2")
        self.assertTrue(a.is_dir() and b.is_dir())


class Attachments(unittest.TestCase):
    def test_meta(self):
        self.assertIsNone(core.check_attachment_meta("logs.ZIP", 1000))
        self.assertIsNotNone(core.check_attachment_meta("logs.exe", 1000))
        self.assertIsNotNone(core.check_attachment_meta("logs.zip.exe", 1000))
        self.assertIsNotNone(core.check_attachment_meta("logs.7z", 1000))
        self.assertIsNotNone(core.check_attachment_meta("", 1000))
        self.assertIsNotNone(core.check_attachment_meta("logs.zip", 0))
        self.assertIsNone(core.check_attachment_meta("logs.zip", core.MAX_ZIP_BYTES))
        self.assertIn("too big", core.check_attachment_meta("logs.zip", core.MAX_ZIP_BYTES + 1))

    def test_bytes(self):
        self.assertIsNone(core.check_zip_bytes(b"PK\x03\x04rest"))
        self.assertIsNone(core.check_zip_bytes(b"PK\x05\x06" + b"\0" * 18))
        self.assertIsNotNone(core.check_zip_bytes(b"MZ\x90\x00"))  # an exe renamed .zip
        self.assertIsNotNone(core.check_zip_bytes(b""))
        self.assertIn("too big", core.check_zip_bytes(b"PK\x03\x04" + b"0" * 20, max_bytes=10))

    def test_clean_description(self):
        self.assertEqual(core.clean_description(" a\x00b\x1b[31m\nc\t "), "ab[31m\nc")
        self.assertEqual(len(core.clean_description("x" * 5000)), core.MAX_DESCRIPTION)


class Rate(unittest.TestCase):
    def test_limit_and_window(self):
        now = [1000.0]
        r = core.RateLimiter(limit=3, window=3600, clock=lambda: now[0])
        for _ in range(3):
            self.assertEqual(r.retry_after(7), 0)
            r.record(7)
            now[0] += 10
        self.assertAlmostEqual(r.retry_after(7), 3600 - 30)
        self.assertEqual(r.retry_after(8), 0)  # other users unaffected
        now[0] = 1000.0 + 3600
        self.assertEqual(r.retry_after(7), 0)


class Config(TempDir):
    def write(self, obj):
        d = self.dir / "coord" / "secrets"
        d.mkdir(parents=True, exist_ok=True)
        p = d / "discord-bot.json"
        p.write_text(json.dumps(obj), encoding="utf-8")
        return p

    def test_good(self):
        tok = "x" * 60
        p = self.write({"token": tok, "guild_id": "123", "dev_channel_id": 456, "report_channel_id": ""})
        c = core.load_config(p)
        self.assertEqual((c["guild_id"], c["dev_channel_id"], c["report_channel_id"]), (123, 456, None))
        self.assertEqual(c["coord_dir"], (self.dir / "coord").resolve())

    def test_errors_never_show_token(self):
        tok = "SECRET" * 10
        cases = [
            {"token": tok, "dev_channel_id": 1},
            {"token": tok, "guild_id": "abc", "dev_channel_id": 1},
            {"token": tok, "guild_id": 1},
            {"token": "short", "guild_id": 1, "dev_channel_id": 1},
        ]
        for obj in cases:
            with self.assertRaises(core.ConfigError) as cm:
                core.load_config(self.write(obj))
            self.assertNotIn("SECRET", str(cm.exception))
        with self.assertRaises(core.ConfigError):
            core.load_config(self.dir / "missing.json")
        bad = self.write({})
        bad.write_text("{" + tok, encoding="utf-8")
        with self.assertRaises(core.ConfigError) as cm:
            core.load_config(bad)
        self.assertNotIn("SECRET", str(cm.exception))


class DescriptionFile(unittest.TestCase):
    def test_contents(self):
        t = core.description_file(9, WHEN, "u", "U Name", "0.11.3", "my logs.zip", "it broke")
        self.assertIn("Report #9", t)
        self.assertIn("Received (UTC): 2026-11-16 21:05:09", t)
        self.assertIn("Discord user: u (U Name)", t)
        self.assertIn("Attachment: my logs.zip -> logs.zip", t)
        self.assertTrue(t.rstrip().endswith("it broke"))


if __name__ == "__main__":
    unittest.main()
