#!/usr/bin/env python3
"""Exercise contention, disconnect undo, and commit through the real TCP protocol.

Run against a disposable database containing files/one_rec and index/one_rec_idx
(the Java fixture), after the Java mutation tests. No server is started/stopped.
"""
import pathlib
import socket
import struct
import sys
import time


class Client:
    def __init__(self, host, port, root):
        self.socket = socket.create_connection((host, port), timeout=10)
        self.socket.sendall(b"9-30-1966")
        assert self.read(2) == b"ok"
        assert self.command(f"24|{root}|") == b"1|"

    def read(self, size):
        data = bytearray()
        while len(data) < size:
            part = self.socket.recv(size - len(data))
            if not part:
                raise RuntimeError("server closed the connection")
            data.extend(part)
        return bytes(data)

    def command(self, data):
        if isinstance(data, str):
            data = data.encode()
        self.socket.sendall(struct.pack("!i", len(data)) + data)
        return self.read(struct.unpack("!i", self.read(4))[0])

    def close(self):
        self.socket.close()


def run(root, host, port):
    root = pathlib.Path(root).resolve()
    owner = Client(host, port, root)
    other = Client(host, port, root)
    try:
        opened = owner.command(f"21|one_rec_idx|{root}|").split(b"|")
        assert int(opened[0]) > 0, opened
        index = int(opened[1])
        assert int(owner.command(f"1|{index}|").split(b"|")[0]) > 0
        original = (root / "files/one_rec").read_bytes()
        header = struct.unpack_from("!H", original)[0]
        record = struct.unpack_from("!q", original, header + 2)[0]
        # Format 1 of the standard fixture has 55 fixed bytes, no blobs.
        data = original[record + 17:record + 17 + 55]
        assert len(data) == 55
        assert owner.command("-1|") == b"1|"
        assert int(other.command("-1|").split(b"|")[0]) < 0
        update = f"25|{index}|0|{record}|1|55|".encode() + b"undo me" + data[7:]
        assert owner.command(update) == b"1|"
        assert (root / "files/one_rec").read_bytes() != original
        owner.close()
        # Disconnect cleanup runs asynchronously in the connection process.
        for _ in range(100):
            response = other.command("-1|")
            if response == b"1|":
                break
            time.sleep(.02)
        else:
            raise AssertionError("disconnected transaction kept admission closed")
        assert (root / "files/one_rec").read_bytes() == original
        assert other.command("-3|") == b"1|"
        print("protocol contention/disconnect undo: PASS")
    finally:
        owner.close()
        other.close()


if __name__ == "__main__":
    run(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else "localhost",
        int(sys.argv[3]) if len(sys.argv) > 3 else 8758)
