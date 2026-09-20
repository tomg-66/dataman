#!/usr/bin/env python3
"""Crash/restart the isolated test servers; never uses an installed server.

Requires Linux, CMake's recovery_srv/recovery_con targets, and mkdf.
All database files, logs, and journals are disposable. The linked test wrappers
redirect the port, PID locks, and message queue away from production resources.
"""
import ctypes
import errno
import os
from pathlib import Path
import secrets
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time

from protocol_commands import (
    COMMIT, DELETE, FLUSH, GET, GET_FIRST, GET_LAST, GET_NEXT, GET_PRIOR,
    ICLOSE, INCLUDE, INSERT, IOPEN, MKIDX, REMOVE, ROLLBACK, SORT, START_XACT,
)

from transaction_protocol_test import Client


def check(condition, message):
    if not condition:
        raise AssertionError(message)


class Servers:
    def __init__(self, directory, srv, con):
        self.directory = directory
        self.srv_binary, self.con_binary = srv, con
        self.srv = self.con = None
        self.clients = []
        self.log = (directory / "servers.log").open("w+b")
        self.libc = ctypes.CDLL(None, use_errno=True)
        # Adopt killed connection workers so the test can reap its children.
        check(self.libc.prctl(36, 1, 0, 0, 0) == 0, "cannot become child subreaper")
        while True:
            self.key = secrets.randbelow(0x3fffffff) + 0x40000000
            self.queue = self.libc.msgget(self.key, 0o600 | 0o1000 | 0o2000)
            if self.queue >= 0:
                break
            if ctypes.get_errno() != errno.EEXIST:
                raise OSError(ctypes.get_errno(), "cannot reserve private message queue")
        self.journal = directory / "journal"
        self.journal.mkdir(mode=0o700)
        self.root = directory / "database"
        self.env = dict(os.environ, DM_TEST_MSGKEY=str(self.key),
                        DATAMAN_JOURNAL_DIR=str(self.journal))

    def start(self):
        self.srv = subprocess.Popen([self.srv_binary], env=self.env,
                                    stdout=self.log, stderr=self.log, start_new_session=True)
        # Workers are created only after startup recovery and queue draining.
        deadline = time.monotonic() + 10
        while len(list(Path(f"/proc/{self.srv.pid}/task").glob("*"))) < 2:
            check(self.srv.poll() is None, "storage server failed during startup")
            check(time.monotonic() < deadline, "storage server startup timed out")
            time.sleep(.01)
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            self.port = reservation.getsockname()[1]
        self.con = subprocess.Popen([self.con_binary],
                                    env=dict(self.env, DM_TEST_PORT=str(self.port)),
                                    stdout=self.log, stderr=self.log, start_new_session=True)
        deadline = time.monotonic() + 10
        while True:
            check(self.con.poll() is None, "connection server failed during startup")
            try:
                return self.client()
            except ConnectionRefusedError:
                check(time.monotonic() < deadline, "connection server startup timed out")
                time.sleep(.01)

    def client(self):
        client = Client("127.0.0.1", self.port, self.root)
        self.clients.append(client)
        return client

    def stop(self):
        # Kill storage FIRST while the client remains connected. Otherwise
        # disconnect cleanup could roll back before the crash being tested.
        if self.srv is not None:
            if self.srv.poll() is None:
                self.srv.kill()
            self.srv.wait(timeout=5)
        workers = []
        if self.con is not None and self.con.poll() is None:
            children = Path(f"/proc/{self.con.pid}/task/{self.con.pid}/children")
            workers = [int(pid) for pid in children.read_text().split()]
            os.killpg(self.con.pid, signal.SIGKILL)
            self.con.wait(timeout=5)
        for client in self.clients:
            client.close()
        self.clients.clear()
        for pid in workers:
            try:
                os.waitpid(pid, 0)
            except ChildProcessError:
                pass
            # Only IPC keyed by the test's own connection workers is removed.
            shmid = self.libc.shmget(pid, 1, 0)
            if shmid >= 0:
                check(self.libc.shmctl(shmid, 0, None) == 0, "shm cleanup failed")
            semid = self.libc.semget(pid, 1, 0)
            if semid >= 0:
                check(self.libc.semctl(semid, 0, 0, 0) == 0, "semaphore cleanup failed")
        for process in (self.srv, self.con):
            if process is not None:
                Path(f"/tmp/.dm-recovery-{process.pid}.pid").unlink(missing_ok=True)
        self.srv = self.con = None

    def close(self):
        self.stop()
        check(self.libc.msgctl(self.queue, 0, None) == 0, "queue cleanup failed")
        self.log.close()


def command(client, text):
    reply = client.command(text)
    check(int(reply.split(b"|", 1)[0]) >= 0, f"command {text!r}: {reply!r}")
    return reply


def payload(key, value):
    return key.encode() + value.encode().ljust(16, b" ")


def snapshot(root):
    return {str(p.relative_to(root)): p.read_bytes()
            for folder in ("files", "index", "blobs")
            for p in (root / folder).iterdir() if p.is_file()}


def lookup(client, index, key):
    reply = command(client, f"{GET}|{index}|{key}|")
    if int(reply.split(b"|", 1)[0]) == 0:
        return None
    fields = reply.split(b"|", 5)
    check(len(fields) == 6 and int(fields[0]) == 23, f"bad get response: {reply!r}")
    body = fields[5]
    check(len(body) == 7 + 9 + 23, "bad key/record length")
    record = struct.unpack_from("!q", body, 8)[0]
    return record, body[16:]


def open_indexes(client, root):
    result = []
    for name in ("alpha", "beta"):
        reply = command(client, f"{IOPEN}|{name}_idx|{root}|").split(b"|")
        check(int(reply[0]) > 0, f"cannot open {name}: {reply}")
        result.append(int(reply[1]))
    return result


def verify(client, indexes, expected):
    for index in indexes:
        for key, data in expected.items():
            found = lookup(client, index, key)
            check(found is not None and found[1] == data, f"index {index}, key {key}: {found}")
        for key in {"KEEP001", "DROP001", "REN0001", "NEW0001"} - expected.keys():
            check(lookup(client, index, key) is None, f"unexpected key {key} in {index}")
        for first, step, reverse in ((GET_FIRST, GET_NEXT, False), (GET_LAST, GET_PRIOR, True)):
            reply = command(client, f"{first}|{index}|")
            seen = []
            while int(reply.split(b"|", 1)[0]) > 0:
                fields = reply.split(b"|", 5)
                body = fields[5]
                key = body[:7].decode()
                seen.append(key)
                check(len(seen) <= len(expected), "index traversal loop or extra entry")
                check(key in expected and body[16:] == expected[key], "wrong traversal record")
                cursor = b"|".join(fields[2:5])
                reply = command(client, f"{step}|{index}|".encode() + cursor + b"|" + body[:16])
            check(seen == sorted(expected, reverse=reverse), "index traversal differs")


def mutate(client, indexes):
    check(command(client, f"{START_XACT}|") == b"1|", "begin failed")
    for index in indexes:
        current = lookup(client, index, "KEEP001")
        check(current is not None, "missing update record")
        record, _ = current
        data = payload("REN0001", "changed")
        check(command(client, f"{FLUSH}|{index}|0|{record}|1|23|".encode() + data) == b"1|",
              "record update failed")
        check(command(client, f"{REMOVE}|{index}|1|KEEP001") == b"1|", "old key removal failed")
        command(client, f"{INCLUDE}|{index}|0|{index}|0|{record}|REN0001|")
        inserted = command(client, f"{INSERT}|1|1|{index}|0|{record}|").split(b"|")
        check(int(inserted[0]) == 23, f"bad insert response: {inserted}")
        new_record = int(inserted[1])
        check(command(client, f"{FLUSH}|{index}|0|{new_record}|1|23|".encode()
                      + payload("NEW0001", "inserted")) == b"1|", "new record flush failed")
        command(client, f"{INCLUDE}|{index}|0|{index}|0|{new_record}|NEW0001|")
        deleted = lookup(client, index, "DROP001")
        check(deleted is not None, "missing deletion record")
        check(command(client, f"{REMOVE}|{index}|1|DROP001") == b"1|", "delete key removal failed")
        command(client, f"{DELETE}|{index}|0|{deleted[0]}|1|")


def fixtures(servers, mkdf):
    for folder in ("files", "index", "blobs"):
        (servers.root / folder).mkdir(parents=True)
    expected = {"KEEP001": payload("KEEP001", "original"),
                "DROP001": payload("DROP001", "delete me")}
    expected.update({f"PAD{i:04d}": payload(f"PAD{i:04d}", "unchanged") for i in range(30)})
    initial = servers.directory / "records.i"
    initial.write_text("".join(f"1:{key}:{data[7:].decode().strip()}:\n"
                               for key, data in expected.items()))
    for name in ("alpha", "beta"):
        subprocess.run([mkdf, name, str(initial)], input=b"1\n2\n7\n16\n",
                       env=dict(os.environ, ROOT=str(servers.root)),
                       stdout=servers.log, stderr=servers.log, check=True)
    client = servers.start()
    for name in ("alpha", "beta"):
        reply = command(client, f"{MKIDX}|7|{name}_idx|{servers.root}|1|{name}|").split(b"|", 9)
        index, workfile = int(reply[1]), int(reply[3])
        data = (servers.root / "files" / name).read_bytes()
        header = struct.unpack_from("!H", data)[0]
        record = struct.unpack_from("!q", data, header + 2)[0]
        while record:
            key = data[record + 17:record + 24].decode()
            check(command(client, f"{SORT}|{index}|0|{record}|{key}|") == b"1|", "sort failed")
            record = struct.unpack_from("!q", data, record + 9)[0]
        check(command(client, f"{ICLOSE}|{index}|") == b"1|", "index build close failed")
        check(command(client, f"{ICLOSE}|-{workfile}|") == b"1|", "workfile close failed")
    return client, expected


def run(servers, mkdf):
    client, original = fixtures(servers, mkdf)
    indexes = open_indexes(client, servers.root)
    verify(client, indexes, original)
    before = snapshot(servers.root)
    changed = dict(original)
    del changed["KEEP001"], changed["DROP001"]
    changed.update(REN0001=payload("REN0001", "changed"), NEW0001=payload("NEW0001", "inserted"))

    mutate(client, indexes)
    verify(client, indexes, changed)
    journal = servers.journal / ".dataman-undo"
    check(journal.stat().st_size > 0, "active transaction has no journal")
    during = snapshot(servers.root)
    for name in before:
        check(during[name] != before[name], f"transaction did not change {name}")
    servers.stop()
    check(journal.exists(), "crash did not leave an undo journal")
    check(snapshot(servers.root) == during, "crash unexpectedly performed rollback")

    client = servers.start()
    check(not journal.exists(), "startup did not retire recovered journal")
    check(snapshot(servers.root) == before, "recovery did not restore every file byte-for-byte")
    indexes = open_indexes(client, servers.root)
    verify(client, indexes, original)
    print("multi-file crash recovery: exact data/index restoration and lookups PASS", flush=True)

    mutate(client, indexes)
    check(command(client, f"{ROLLBACK}|") == b"1|", "explicit rollback failed")
    check(snapshot(servers.root) == before, "explicit rollback did not restore all files")
    verify(client, indexes, original)
    check(not journal.exists(), "explicit rollback left journal")
    print("multi-file explicit rollback: restored files and open-index caches PASS", flush=True)

    mutate(client, indexes)
    check(command(client, f"{COMMIT}|") == b"1|", "commit failed")
    check(not journal.exists(), "commit left journal")
    committed = snapshot(servers.root)
    verify(client, indexes, changed)
    servers.stop()
    client = servers.start()
    check(snapshot(servers.root) == committed, "committed files changed on restart")
    verify(client, open_indexes(client, servers.root), changed)
    print("multi-file commit: data and index changes survive restart PASS", flush=True)


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit("usage: transaction_recovery_test.py recovery_srv recovery_con mkdf")
    with tempfile.TemporaryDirectory(prefix="dm-recovery-") as directory:
        servers = Servers(Path(directory), *(str(Path(arg).resolve()) for arg in sys.argv[1:3]))
        try:
            run(servers, str(Path(sys.argv[3]).resolve()))
        except BaseException:
            servers.log.flush()
            print((Path(directory) / "servers.log").read_text(errors="replace"), file=sys.stderr)
            raise
        finally:
            servers.close()
