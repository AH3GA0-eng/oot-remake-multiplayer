"""Tests for tools/problem_reports/problem_reports.py (run: python tools/problem_reports/tests/test_problem_reports.py -v).

Every log line here is made up for the test (no game output)."""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import problem_reports as pr  # noqa: E402

LOG = """[hos host] run_id: 0123456789abcdef0123456789abcdef
[hos sead::MainThread] coop: build hookshot 0.11.3, protocol 12
[hos sead::MainThread] coop: on, server 192.168.1.20:27510, name 'Saria', join password set
[hos host] coop: host: player 2 joined: Mido (client: hookshot 0.11.3)
[hos sead::MainThread] coop: pose: Player_Child 309 bones, 0 moving; 0 sent/s, 0 received/s, 0 refused/s
[hos Presentation Thread] coop: boss: could not move into the boss room of InsideTheDekuTree after 5 tries; giving up
[hos Presentation Thread] coop: boss: could not move into the boss room of InsideTheDekuTree after 5 tries; giving up
[hos host] coop: host: player 2 left: Mido (timed out: nothing heard for 10003 ms)
[hos host] warning: coop: save: could not write C:\\Users\\Saria\\Games\\coop\\user\\save\\slot_01
[hos host] coop: guard: a fault in co-op at 0x80021234 was caught, the game goes on (peer fe80::1c2b:3a4d:5e6f:7a8b%12)
"""


def write_log(text, folder):
    path = os.path.join(folder, "longshot_log.txt")
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)
    return path


class MaskTest(unittest.TestCase):
    def test_every_ip_form_is_masked(self):
        for text in ["server 192.168.1.20:27510", "peer [2001:db8:85a3::8a2e:370:7334]:27510", "fe80::1c2b:3a4d:5e6f:7a8b%12",
                     "addr ::1", "mapped ::ffff:203.0.113.9 port 9", "http://10.0.0.5:8080/x",
                     "full 2001:0db8:0000:0000:0000:ff00:0042:8329", "ip=100.64.12.3", "vpn 26.123.45.67"]:
            masked = pr.mask(text)
            self.assertIn("[ip]", masked, text)
            self.assertIsNone(pr.LEAK.search(masked), masked)

    def test_names_paths_codes_and_passwords_are_masked(self):
        self.assertEqual(pr.mask("player 2 joined: Mido (client: x)"), "player 2 joined: [player] (client: x)")
        self.assertEqual(pr.mask("coop: player 2 'Mido' joined"), "coop: player 2 [player] joined")
        self.assertEqual(pr.mask("coop: notice: Mido got the Fairy Ocarina"), "coop: notice: [player] got the Fairy Ocarina")
        self.assertIn("C:\\Users\\[user]\\Games", pr.mask("C:\\Users\\Saria\\Games\\coop"))
        self.assertIn("/home/[user]/coop", pr.mask("/home/saria/coop"))
        self.assertEqual(pr.mask("invite code: ABCD-EFGH-JKLM-NPQR copied"), "invite code: [code] copied")
        self.assertEqual(pr.mask("join password = hunter2"), "join password = [hidden]")

    def test_the_port_alone_is_not_an_address(self):
        self.assertEqual(pr.mask("coop: host: listening on port 27510"), "coop: host: listening on port 27510")


class RunTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()

    def test_only_problems_are_taken_and_counted_once_each(self):
        run = pr.read_run(write_log(LOG, self.dir))
        self.assertEqual(run["key"], "0123456789abcdef0123456789abcdef")
        self.assertEqual((run["module"], run["protocol"]), ("0.11.3", "12"))
        lines = {line: n for line, n in run["events"].values()}
        self.assertTrue(any("giving up" in l and n == 2 for l, n in lines.items()))
        self.assertTrue(any("left: [player] (timed out" in l for l in lines))
        self.assertFalse(any("coop: pose:" in l for l in lines), "a routine counter is not a problem")
        self.assertFalse(any("joined:" in l for l in lines), "a join is not a problem")

    def test_nothing_sent_holds_an_ip_name_or_user(self):
        run = pr.read_run(write_log(LOG, self.dir))
        sent = "\n".join(pr.messages(run))
        for secret in ["192.168", "fe80", "Saria", "Mido", "::"]:
            self.assertNotIn(secret, sent)

    def test_a_line_that_still_looks_like_an_address_is_dropped(self):
        run = pr.read_run(write_log("[hos host] coop: could not reach 1.2.3.4.5.6\n", self.dir))
        self.assertEqual(run["events"], {})

    def test_fingerprints_ignore_numbers_and_addresses(self):
        a = pr.fingerprint(pr.mask("[t] coop: x could not load slot 3 at 0x10abc after 12 ms"))
        b = pr.fingerprint(pr.mask("[u] coop: x could not load slot 5 at 0x20def after 40 ms"))
        self.assertEqual(a, b)


class SendTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.state = os.path.join(self.dir, "state.json")
        self.sent = []

    def send(self, url, content):
        self.sent.append(content)
        return True

    def test_each_problem_is_sent_once_across_runs_and_sessions(self):
        log = write_log(LOG, self.dir)
        first = pr.report([log], self.state, "http://hook", send=self.send, out=lambda m: None)
        self.assertGreater(first, 0)
        n = len(self.sent)
        self.assertEqual(pr.report([log], self.state, "http://hook", send=self.send, out=lambda m: None), 0)
        self.assertEqual(len(self.sent), n, "the same log is never sent again")
        other = os.path.join(self.dir, "other")
        os.makedirs(other)
        again = write_log(LOG.replace("0123456789abcdef0123456789abcdef", "fedcba9876543210fedcba9876543210"), other)
        self.assertEqual(pr.report([again], self.state, "http://hook", send=self.send, out=lambda m: None), 0)
        self.assertEqual(len(self.sent), n, "the same problems from another session are not sent again")

    def test_no_webhook_or_dry_run_sends_nothing(self):
        log = write_log(LOG, self.dir)
        printed = []
        self.assertGreater(pr.report([log], self.state, "", send=self.send, out=printed.append), 0)
        self.assertGreater(pr.report([log], self.state, "http://hook", dry_run=True, send=self.send, out=printed.append), 0)
        self.assertEqual(self.sent, [])
        self.assertFalse(os.path.exists(self.state), "nothing recorded as sent")

    def test_an_unreachable_webhook_is_tried_again_next_time(self):
        log = write_log(LOG, self.dir)
        pr.report([log], self.state, "http://hook", send=lambda u, c: False, out=lambda m: None)
        self.assertGreater(pr.report([log], self.state, "http://hook", send=self.send, out=lambda m: None), 0)


if __name__ == "__main__":
    unittest.main()
