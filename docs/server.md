# Server operation

## Process layout

Dataman uses three server programs:

- `dataman` is the supervisor. It starts the other services, handles shutdown,
  and restarts a child that exits unexpectedly.
- `dataman_con` accepts client connections and routes protocol messages.
- `dataman_srv` owns database operations and worker threads.

Normally operators start only `dataman`. Starting child services independently
is mainly useful for debugging.

## systemd

Both build systems install `dataman.service` under the prefix's
`lib/systemd/system` directory. With the default `/usr/local` prefix, after a
root installation and service-account provisioning, run:

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now dataman.service
sudo systemctl status dataman.service
```

The unit runs the supervisor as `dataman` with `-f` (foreground), keeps its child
processes in the service cgroup, and restarts an exited supervisor. Installation
does not enable or start the service. `systemctl stop dataman` terminates active
connections; unfinished transactions are recovered on the next start. Use
`systemctl restart dataman` after installing updated server binaries.

Stop any manually started instance before switching to systemd. The `dataman`
account needs access to every application root. Existing journals and lock files
must belong to it; do not delete pending recovery files to change ownership.
Do not also enable the sample SysV rc script.

Use `journalctl -u dataman.service` for supervisor diagnostics. Child servers
retain their existing `/tmp/dbserv.log` and `/tmp/serial.log` logging behavior.
The unit's active state indicates process startup, not database readiness;
startup recovery must finish before requests can run. See the
[systemd service documentation](https://github.com/systemd/systemd/blob/main/man/systemd.service.xml)
for the `Type=simple` startup semantics.

Configure overrides with `systemctl edit dataman.service`, for example:

```ini
[Service]
Environment=DATAMAN_JOURNAL_DIR=/srv/dataman/journal
```

Provision an overridden journal directory with the same ownership and private
permissions. For a custom installation prefix outside systemd's search paths,
use `systemctl link /absolute/prefix/lib/systemd/system/dataman.service` before
enabling it. Configure the final prefix before building so `ExecStart` and `PATH`
point to the installed binaries. Packagers may override `systemdsystemunitdir`
with Make or `DATAMAN_SYSTEMD_UNIT_DIR` with CMake; `DESTDIR` staging is supported.

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
Direct Linux installs run `dataman-system-setup` when invoked as root. It creates
a system `dataman` group and non-login user if absent, provisions the journal
directories with mode 0700, and registers `dataman 8758/tcp` in `/etc/services`.
Existing account settings are retained; conflicting TCP registrations stop setup.
Staged (`DESTDIR`) and non-root installs skip host provisioning. On the target
host, run `sudo /usr/local/sbin/dataman-system-setup` (adjust the install prefix).

Start the supervisor as this account, for example
`sudo -u dataman /usr/local/bin/dataman`; the sample rc script uses `runuser`.
The account also needs access to the application database roots. Stop existing
servers before changing service ownership; existing journal files and lock files
are not recursively reassigned by setup.

For manual provisioning, create the service account first. The server creates the `journal` subdirectory with
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
tests. No UDP registration is required: the connection server uses TCP.

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

Live record/index/blob operations now use the server-owned undo journal.
Explicit transactions retain exclusive server admission until commit or rollback;
standalone operations use implicit transactions. Session-close notifications and
a periodic dead-session sweep abort abandoned transactions. Cleanup failure stops
service for restart recovery. See [Transactions](transactions.md) for supported
operations, maintenance restrictions, and unknown commit outcomes.

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
