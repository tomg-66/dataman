#!/usr/bin/env python3
"""Test host provisioning with disposable files and mocked account commands."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]


class SetupTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="dataman-setup-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.services = self.root / "services"
        self.services.write_text("ssh 22/tcp\n")
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.env = dict(os.environ, PATH=str(self.bin) + ":" + os.environ["PATH"],
                        FIXTURE=str(self.root), DESTDIR="")
        self.command("id", 'echo 0')
        self.command("uname", 'echo Linux')
        self.command("getent", 'test -f "$FIXTURE/$1-created"')
        self.command("groupadd", 'touch "$FIXTURE/group-created"')
        self.command("useradd", 'printf "%s\\n" "$@" > "$FIXTURE/user-args"; touch "$FIXTURE/passwd-created"')
        self.command("nologin", 'exit 1')
        self.command("install", '''shift 7
for directory do
    mkdir -p "$directory"
    chmod 0700 "$directory"
done''')
        source = (REPO / "scripts/dataman-system-setup").read_text()
        source = source.replace("/etc/services", str(self.services))
        source = source.replace("/var/lib/dataman", str(self.root / "state"))
        self.script = self.root / "setup"
        self.script.write_text(source)

    def command(self, name, body):
        path = self.bin / name
        path.write_text("#!/bin/sh\nset -eu\n" + body + "\n")
        path.chmod(0o755)

    def run_setup(self, success=True):
        result = subprocess.run(["sh", str(self.script)], env=self.env,
                                capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)

    def test_create_and_repeat(self):
        self.run_setup()
        self.assertIn("--system", (self.root / "user-args").read_text())
        self.assertIn("--no-create-home", (self.root / "user-args").read_text())
        self.assertEqual((self.root / "state/journal").stat().st_mode & 0o777, 0o700)
        self.run_setup()
        self.assertEqual(self.services.read_text().count("8758/tcp"), 1)
        self.assertIn("ssh 22/tcp", self.services.read_text())

    def test_alias_preserved(self):
        contents = "custom 8758/tcp dataman # existing alias\n"
        self.services.write_text(contents)
        self.run_setup()
        self.assertEqual(self.services.read_text(), contents)

    def test_conflicts_fail_before_account_changes(self):
        for contents in ("other 8758/tcp\n", "dataman 9999/tcp\n"):
            self.services.write_text(contents)
            self.run_setup(False)
            self.assertEqual(self.services.read_text(), contents)
            self.assertFalse((self.root / "group-created").exists())

    def test_staging_and_nonroot_skip(self):
        self.env["DESTDIR"] = str(self.root / "stage")
        self.run_setup()
        self.env["DESTDIR"] = ""
        self.command("id", 'echo 1000')
        self.run_setup()
        self.assertFalse((self.root / "group-created").exists())
        self.assertEqual(self.services.read_text(), "ssh 22/tcp\n")


if __name__ == "__main__":
    unittest.main()
