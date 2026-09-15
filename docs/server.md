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

`dataman_srv` now uses `/var/lib/dataman/journal/` for persistent journal storage.
Provision `/var/lib/dataman` for the account that runs Dataman before starting
this development build. The server creates the `journal` subdirectory with
mode 0700 if needed; an existing journal directory must be owned by the service
account with no group or other permissions. For example, if that account is
named `dataman`, an administrator can provision it with:

```sh
install -d -o dataman -g dataman -m 0700 /var/lib/dataman /var/lib/dataman/journal
```

`DATAMAN_JOURNAL_DIR` overrides the default with an absolute directory path.
Set it in the supervisor's environment so initial starts and child restarts
inherit the same location. Its parent must already exist. Production journals
must remain on persistent storage; temporary directories are only for disposable
tests. The installation process does not create a service account or change
ownership automatically.

After daemonization, the storage server acquires its PID lock and the persistent
`.server.lock` in the journal directory. The latter remains locked until process
exit and is never removed during normal shutdown. Do not remove it to start
another server. Dataman still uses one server and fixed IPC keys; changing the
journal path does not enable multiple instances.

Only after acquiring ownership does the server clear stale queue messages and
recover the journal. Worker threads start after recovery succeeds. New messages
arriving during recovery remain queued. Missing database roots, corrupt journals,
ownership failures, or synchronization errors cause a nonzero exit before any
worker can execute a database request. The supervisor may restart the child;
resolve the reported problem rather than deleting the recovery journal.

Startup recovery is connected, but live transaction execution still uses the
existing connection-server implementation and does not produce these journals
yet. This step does not add ACID guarantees to client operations.

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
