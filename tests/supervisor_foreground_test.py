#!/usr/bin/env python3
"""Check foreground supervision using fake children; no database or sockets."""
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="dm-supervisor-test-") as directory:
    root = Path(directory)
    supervisor = root / ("dm-fg-test-" + str(os.getpid()))
    shutil.copy2(binary, supervisor)
    pidfile = Path("/tmp/." + supervisor.name + ".pid")
    children = []
    for name in ("dataman_srv", "dataman_con"):
        child = root / name
        child.write_text(f'''#!{sys.executable}
import os, pathlib, signal, time
root = pathlib.Path({str(root)!r})
(root / {name + '.pid'!r}).write_text(str(os.getpid()) + ' ' + str(os.getppid()))
def stop(sig, frame):
    (root / {name + '.stopped'!r}).touch()
    raise SystemExit(0)
signal.signal(signal.SIGTERM, stop)
while True: time.sleep(.05)
''')
        child.chmod(0o755)
    env = dict(os.environ, PATH=str(root) + ":" + os.environ["PATH"])
    process = subprocess.Popen([str(supervisor), "-f"], env=env,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               start_new_session=True)
    try:
        for _ in range(100):
            assert process.poll() is None, process.communicate()
            if all((root / (name + '.pid')).exists()
                   for name in ("dataman_srv", "dataman_con")):
                break
            time.sleep(.02)
        else:
            raise AssertionError("children did not start")
        assert int(pidfile.read_text()) == process.pid, "supervisor daemonized"
        for name in ("dataman_srv", "dataman_con"):
            child, parent = map(int, (root / (name + '.pid')).read_text().split())
            children.append(child)
            assert parent == process.pid
        contender = subprocess.run([str(supervisor), "-f"], env=env,
                                   capture_output=True, timeout=5)
        assert contender.returncode != 0, "duplicate supervisor admitted"
        assert int(pidfile.read_text()) == process.pid
        time.sleep(1.1)  # Wait past the supervisor's existing startup interval.
        assert process.poll() is None
        process.terminate()
        assert process.wait(timeout=5) == 0
        for _ in range(100):
            if all((root / (name + '.stopped')).exists()
                   for name in ("dataman_srv", "dataman_con")):
                break
            time.sleep(.02)
        else:
            raise AssertionError("children did not receive shutdown")
        assert not pidfile.exists()
        print("foreground start, ownership, and shutdown: PASS")
    finally:
        if process.poll() is None:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
        for child in children:
            try:
                os.kill(child, signal.SIGKILL)
            except ProcessLookupError:
                pass
        process.stdout.close()
        process.stderr.close()
