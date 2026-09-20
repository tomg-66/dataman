"""Wire command names shared by the Python protocol tests.

Keep in sync with server/dbfunc.h and clientlib/java/DatamanFunc.java.
protocol_mapping_test.py independently pins the expected wire numbers.
"""

ROLLBACK = -3
COMMIT = -2
START_XACT = -1
GET = 0
GET_FIRST = 1
GET_LAST = 2
GET_NEXT = 3
GET_PRIOR = 4
GET_CURRENT = 5
FORWARD = 6
BACK = 7
PROTECT = 8
GET_DESC = 9
INIT_DAT = 10
RELEASE = 11
MKIDX = 12
RESTORE = 13
DELETE = 14
INSERT = 15
INCLUDE = 16
REMOVE = 17
CLEAR = 18
IOPEN = 19
ICLOSE = 20
SORT = 21
DEF_ROOT = 22
FLUSH = 23
DISCON = 24

# Versioned connection greeting; independent of the software release number.
PROTOCOL_VERSION = 1
PROTOCOL_HELLO = b"DMAN0001\n"
