# C client API

Include `dataman.h` and link with the installed Dataman C client library. The
public header exposes the EDITOR-style verbs as macros over `db_*` functions,
plus the current index and record state.

## Typical lifecycle

1. Initialize the client connection.
2. Open an index with `iopen(..., RDONLY)` or `iopen(..., UPDATE)`.
3. Navigate with `get`, `get_first`, `get_next`, and related verbs.
4. Read or modify the master/work fields appropriate to the current format.
5. Use `protect`/`clear` around coordinated updates where required.
6. Close indexes and terminate the connection on every exit path.

`BEFORE` and `AFTER` select insertion placement. `MFMT`, `WFMT`, `KEY`, and
`FILE` expose current operation state. The `when_mfmt`, `when_wfmt`, and
`when_file` helpers make format-dependent dispatch concise.

## Errors and ownership

Current C routines report failure after calling the library error mechanism;
applications must check the documented return from every operation. A failed
operation must not be assumed to have replaced the current key or record.

Buffers returned by the transport are library-owned until parsed or released.
Do not retain pointers into a response buffer. Record-field allocations and
index descriptions must be released by their matching library cleanup path;
avoid freeing individual internals directly.

When adding a new error path, preserve the previous valid state until the new
response is fully parsed, free each temporary exactly once, and unwind partially
opened indexes before returning.

The authoritative declarations are in [`clientlib/c/dataman.h`](../clientlib/c/dataman.h)
and [`clientlib/c/proto.h`](../clientlib/c/proto.h).
