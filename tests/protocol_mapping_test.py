#!/usr/bin/env python3
"""Pin the wire contract independently of the shared test command constants."""
from pathlib import Path
import re
import unittest

import protocol_commands

ROOT = Path(__file__).resolve().parents[1]

# Deliberately literal: changing all implementations together must still require
# an explicit update to this contract and a compatibility decision.
EXPECTED = {
    "ROLLBACK": -3, "COMMIT": -2, "START_XACT": -1,
    "GET": 0, "GET_FIRST": 1, "GET_LAST": 2, "GET_NEXT": 3,
    "GET_PRIOR": 4, "GET_CURRENT": 5, "FORWARD": 6, "BACK": 7,
    "PROTECT": 8, "GET_DESC": 9, "INIT_DAT": 10, "RELEASE": 11,
    "MKIDX": 12, "RESTORE": 13, "DELETE": 14, "INSERT": 15,
    "INCLUDE": 16, "REMOVE": 17, "CLEAR": 18, "IOPEN": 19,
    "ICLOSE": 20, "SORT": 21, "DEF_ROOT": 22, "FLUSH": 23, "DISCON": 24,
}


class ProtocolMappingTest(unittest.TestCase):
    def test_python_commands(self):
        actual = {name: value for name, value in vars(protocol_commands).items()
                  if name.isupper()}
        self.assertEqual(actual, EXPECTED)

    def test_server_and_native_clients(self):
        # C and C++ clients include this same header.
        header = (ROOT / "server/dbfunc.h").read_text()
        actual = {name: int(value) for name, value in
                  re.findall(r"^#define\s+(\w+)\s+(-?\d+)\b", header, re.M)}
        self.assertEqual(actual, EXPECTED)

    def test_java_commands(self):
        source = (ROOT / "clientlib/java/DatamanFunc.java").read_text()
        actual = {name: int(value) for name, value in
                  re.findall(r"protected static final int (\w+) = (-?\d+);", source)}
        self.assertEqual(actual, EXPECTED)
        labels = source.split("funcStrings", 1)[1].split("};", 1)[0]
        self.assertEqual(re.findall(r'"([A-Z_]+)"', labels),
                         sorted(EXPECTED, key=EXPECTED.get))


if __name__ == "__main__":
    unittest.main()
