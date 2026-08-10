# Server operation

## Process layout

Dataman uses three server programs:

- `dataman` is the supervisor. It starts the other services, handles shutdown,
  and restarts a child that exits unexpectedly.
- `dataman_con` accepts client connections and routes protocol messages.
- `dataman_srv` owns database operations and worker threads.

Normally operators start only `dataman`. Starting child services independently
is mainly useful for debugging.

## Configuration

`ROOT` selects the database root used by utilities and by clients that do not
pass an explicit root. Client programs also accept or derive a server host; the
legacy environment name is `DSRVHOST`.

The supervisor supports command-line switches for foreground/debug operation,
quiet or status actions, shared-memory size, and database worker count. Run the
installed binary with an invalid option or consult its build's usage output
before scripting these flags: some service-management details remain
installation-specific.

## Operational rules

- Keep `files/`, `index/`, and `blobs/` beneath one controlled database root.
- Do not copy live database files as an assumed consistent backup.
- Do not expose an index to clients while rebuilding it. V2 indexes carry a
  build-state marker, and open/dump operations reject an incomplete rebuild.
- Treat legacy-index errors as a request to rebuild, not as corruption to edit
  by hand.
- Shut down through the supervisor so both child processes exit together.

## Diagnostics

Use `dumpdf` for logical record inspection, `dumpix` for logical index entries,
and `dnodes` for page/tree diagnostics. Run the server in debug mode only while
actively observing it; normal deployments should capture stderr and supervise
the top-level process with the host's service manager.
