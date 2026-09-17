# Transactions and locking

## Current transaction model

`start_transaction` begins client-side command collection. `commit` sends or
executes the collected operations, and `rollback` discards or reverses work as
supported by the current implementation.

This is not a full ACID transaction manager. In particular, applications must
not assume durable write-ahead logging, crash recovery across every operation,
or general isolation from concurrent clients. Treat transaction grouping as a
useful application feature, not as the equivalent of an SQL database commit.

The work record is working storage, not independently protected transactional
state.

## Cooperative record protection

Use `protect` before changing a record that another client could update, and
`clear` when the protected operation is complete. Structure every error path so
that an acquired protection is cleared. Keep the protected interval short and
never wait for user interaction while holding it.

A robust update flow is:

1. Locate the record through an update-mode index.
2. Protect it and handle failure without modifying state.
3. Copy or edit the work/master data.
4. Save, insert, include, remove, or delete as required.
5. Clear protection in both success and failure cleanup.

## Application guidance

Make multi-step operations idempotent where possible. Record enough application
state to detect an interrupted operation, verify outcomes after reconnecting,
and keep independent backups. Work that requires strict atomicity, durability,
or isolation should wait for a future journaled transaction design or be
coordinated by a system that provides those guarantees.

## Transaction manager development plan

This section describes proposed work on `add_transactions`, based on release
`v4.1.0` (`2b5aa88`). It does not change the guarantees described above.

The first implementation step added checked positional reads/writes in
`server/storage_io.c` and uses checked writes for record payloads in `flush`.
Interrupted and short transfers are handled, and a failed record write stops
before blob processing. Standalone fault-injection tests cover this behavior.
The existing-file undo journal below has recovery and fault-injection tests.
The storage server now runs recovery from its persistent journal directory before
starting workers. Server-owned transactions and live write integration remain
planned; ordinary client writes still bypass the journal.

### Ownership and transaction boundary

The transaction manager belongs in `dataman_srv`, alongside the storage
operations. No additional server process is needed. `dataman_con` will forward
begin, operation, commit, abort, and session-close requests instead of owning
the command list and performing compensating operations.

Server transaction state belongs to a session, not a worker thread. Use a
server-issued session generation as well as the connection identity so a reused
PID or IPC identifier cannot inherit an abandoned transaction. Serialize
requests within a session and associate reads with its active transaction.

The initial isolation target is serializable execution through a coarse gate:
an explicit transaction acquires exclusive access before its first database
access and retains it through commit or completed abort. Other sessions' reads
and writes must participate in the gate. Standalone mutations are implicit
transactions; standalone reads hold access for their operation. Work buffers
remain session-local working storage, outside durable transaction state.

Each connection declares one database root through `init_dataman`; transactions
on that connection belong to that root. Enforce this boundary during server
integration. One server may serve many such application roots. Start with a
server-wide gate and one journal for simplicity; concurrent transactions are a
later extension. Queue requests that cannot proceed; do not block every
dispatch worker waiting for the gate, preventing the owning session's commit
or abort from running. Existing cooperative protections must also be audited
for waits on another session while the transaction gate is held.

### Storage and recovery approach

The proposed first implementation uses a physical undo journal with forced
data at commit. This accommodates the existing in-place record writes without
requiring a private page cache and redo engine in the first implementation.

Before modifying an existing byte range, persist and synchronize its before
image. Before extending or creating a file, persist its original length or
absence. Blob replacement, renaming, and deletion require equivalent durable
undo information, including the previous contents where needed. Every mutation
must pass through this layer, including index page reuse and root publication.
Copy-on-write alone does not make several indexes commit atomically.

Commit ordering is:

1. Complete all mutations while retaining exclusive transaction access.
2. Synchronize all modified files and affected directories.
3. Append and synchronize a checksummed commit record in the journal.
4. Release access and acknowledge success to the client.
5. Reclaim recovery information only through a crash-safe journal retirement
   procedure. Cleanup failure must not reverse an already durable commit.

Recovery runs before accepting database requests. Undo transactions without a
valid durable commit record in reverse mutation order, synchronize restored
files and directories, and durably mark recovery complete before retiring the
journal. Undo must be idempotent so recovery can itself be interrupted. Reset
or reload cached file headers, index roots, and allocator state after abort.

Journal framing needs a version, bounded lengths, transaction identity,
sequence numbers, and checksums. A torn final record must be distinguishable
from corruption of an earlier record; ambiguous recovery must stop database
access. Stable file identities and validated paths are needed for replay.
The experimental format and retirement protocol are specified below. The journal
directory is persistent server-wide storage, independent of application roots.

An I/O failure makes the transaction abort-only. If durable abort cannot finish,
stop serving database operations and require recovery. A lost commit response
is an unknown outcome for the client, not proof of rollback. Automatic retry of
such a transaction is unsafe without a separate outcome/deduplication protocol.

### Persistent mutation inventory

This is the initial source audit; indirect calls and cached state must be
traced when each operation is integrated.

| Paths | State that must participate |
| --- | --- |
| `server/flush.c` | Record payload and associated blobs; check short writes and preserve the original write error across blob processing. |
| `server/insert.c` | New record, neighboring links, file endpoints, extension/allocation state. |
| `server/delete.c`, `server/undelete.c` | Record flags, links, endpoints, blob hide/restore/cleanup. |
| `server/include.c`, `server/remove.c`, `server/upd_idx.c`, `server/index_v2.c` | Index entries, pages, root slots, generations, reused storage, and cached roots. |
| `server/put_blobs.c`, `server/blob_ctl.c` | File creation, truncation, contents, rename, unlink, directory durability, and checked error propagation. |
| `server/mkidx.c`, `server/sort.c` | Index creation and in-place rebuild; require exclusive maintenance handling initially. |
| `server/mkdf.c`, `server/dfedit.c`, `server/rebuild.c` | Utility writes outside normal server dispatch; require enforced offline exclusion before ACID guarantees can cover a database. |

The record payload, insert/delete/undelete link and flag writes, and v2 index
page/header writes now use `dm_storage_mutate_at`. This includes legacy record
error-repair writes. Their offsets are explicit, and interrupted/short writes
are handled centrally. The server installs a transaction router at startup. With
no journal owner it uses direct checked I/O; with an owner it requires an
explicit worker scope and routes through descriptor-validated undo writes.
Dispatch scope construction, exclusion, and restoration of cached index state
remain integration work. Blob creation/truncation/rename/unlink still need journal
operations and durable directory updates before activation.

### API compatibility and lifecycle

Root registration is now handled by `server/session_root.c`, alongside the
existing connection-side transaction implementation. No journal is opened by
registration, and journaled transaction execution is not enabled yet.

Clients without a work file send `DEF_ROOT` as `24|<database-root>|` after
connection setup. Success is `0|1|`, with no work-file payload. Failures use
the normal negative first field: malformed requests or root changes return
`EINVMSG`; missing/non-directory roots return `ENOFILE`; missing connection IPC
returns `ENOSHM`. Existing short-command message-size limits still apply.
This branch assigns `FLUSH=25` and `DISCON=26`; clients and both server processes
must use matching protocol definitions. There is no version negotiation for
the previous command numbering.

Traditional `INIT_DAT` derives the root from the trailing `/files/<workfile>`
path and publishes it only after work-file initialization succeeds. `MKIDX`
also supports initial registration: its request carries the root after the key
length and index name (`12|<key-length>|<index-name>|<root>|<file-count>|...`).
It publishes the root only after index creation succeeds and rejects a different
registered root before invoking the index builder. These initialization paths
canonicalize the directory and store its device/inode identity. Repeating the
same root is allowed; changing it, including replacing the directory at the
same path, is rejected for that session. This registration does not yet enforce
root containment on other database operations such as index opening.

Metadata is keyed by the connection's shared-memory ID, including its kernel IPC
generation, rather than PID alone. Connection cleanup now sends an internal
`DISCON|<shmid>|` notification before clearing protections, closing indexes, and
removing IPC resources. The storage server validates that the supplied ID still
belongs to that connection's PID. No additional public command or client change
is required; client `DISCON` continues to terminate the connection normally.

The storage server also sweeps registered sessions approximately once per second.
A removed shared-memory segment, an orphaned segment with no attachments, or a
connection PID that no longer exists triggers journal abort before metadata is
forgotten. A still-attached worker does not prevent cleanup when the connection
PID is gone. This does not yet reclaim all legacy work-file/index/protection
resources left by a killed connection process, or make legacy commit replay
crash-atomic. Complete IPC/session invalidation on server restart remains work
for integration with transaction execution.

### Server-side journal owner manager

`transaction_session.c` supplies internal begin/write/commit/abort operations keyed
by the registered session ID. Startup configures it after successful journal
recovery while retaining the persistent journal lock. Only one owner is admitted;
a second owner's begin returns `ENOLOCK` immediately rather than occupying a
worker waiting for the journal. Duplicate begin returns `EINXACT`, and a write,
commit, or abort from a non-owner returns `ENOXACT`.

Beginning pins the registered root through journal creation, with a consistent
root-metadata-before-transaction lock order. Disconnect/reaping cannot remove
that session between root lookup and journal creation. Root identity is checked
again before use. Successful commit or abort releases ownership; disconnect of
an owner aborts, and disconnect of a non-owner is harmless.

A failed write makes the owner abort-only. Failed begin, commit, or abort blocks
new transaction ownership until restart recovery. A failed commit reports an
unknown outcome (`EMULTIPLE`), not guaranteed rollback. Failed abort retains the
session and journal; dispatch rejects further requests and the main server loop
exits for startup recovery. Already executing requests cannot be safely stopped
by this check alone; transaction-wide isolation must precede live activation.

The owner manager is tested with real journals, but the legacy wire-level
`START_XACT`, `COMMIT`, and `ROLLBACK` still execute in `serial_service`. They do
not call the new begin/write/commit APIs. Opening an empty journal around those
unmodified writes would give a false recovery guarantee. Live activation requires
all persistent writes to use the manager and all other database access to
participate in transaction isolation. Until then, disconnect/reaping hooks are
in place but ordinary client transactions do not own recovery journals.

### Remaining API compatibility and lifecycle work

Retain public `start_transaction`, `commit`, and `rollback` entry points where
possible. Audit all four client libraries before replacing the connection-side
queue: inserted records currently use temporary negative identifiers, and
clients may depend on deferred responses or local transaction bookkeeping.
Define read-your-writes and cursor behavior explicitly, including cursor
invalidation after abort. A record read before begin is outside the protected
transaction boundary.

Disconnect before durable commit aborts the transaction. Connection-process
death must trigger the same cleanup even when no close message arrives. Server
restart must invalidate old sessions and IPC requests. Resource limits and an
idle-transaction policy are required because a client can otherwise retain the
coarse gate indefinitely. Begin while active and commit/abort while inactive
must return defined errors.

### Implementation stages and acceptance tests

1. Specify session lifecycle, request scheduling, journal format, and client
   compatibility. Add a transaction integration harness with isolated database
   roots and deterministic failure injection.
2. Implement checked storage I/O and the undo journal with standalone recovery
   tests. Cover short writes, interrupted calls, disk-full errors, torn journal
   tails, and crashes during recovery itself.
3. Integrate server-owned sessions and the transaction gate with a complete
   journaled record-update path. Treat this as an internal milestone, not an
   ACID release while other mutation paths bypass journaling.
4. Integrate insertion, deletion, indexes, blobs, autocommit, and enforced
   maintenance exclusion. Replace connection-side transaction execution only
   after client compatibility tests pass.
5. Run crash and concurrency tests across complete multi-operation transactions.
   Optimize lock granularity and commit throughput only after correctness.

Acceptance requires that every acknowledged commit survives restart and every
aborted transaction restores its prior logical state. Kill the server before
and after journal sync, each storage mutation, data sync, commit sync, and
retirement. Verify record traversal, index-to-record mappings, blob contents,
and subsequent allocation after recovery. Concurrent clients must never see
partial transactions or lose updates within protected read-modify-write work.
Include reader visibility, worker-pool saturation, owner disconnect, and
server restart tests. Process-kill tests alone do not simulate power loss;
durability validation also needs controlled loss/reordering of unsynchronized
writes.

## Standalone existing-file journal prototype

`server/undo_journal.h` exposes begin, write, commit, abort, close, and recovery
operations. Storage-server startup invokes recovery under persistent journal
ownership; `undo_journal_test` exercises the write, commit, and abort paths.
It establishes ordering and recovery behavior before live transaction integration.

### Scope and caller contract

Begin takes a persistent journal directory and a separate database root:
`dm_undo_begin(journal_directory, database_root, &tx)`. Recovery takes only
`dm_undo_recover(journal_directory)`: it discovers the root from the header,
without a connected client or a dependency on the current working directory.
Provision the journal directory in persistent service-owned storage (for example
`/var/lib/dataman/journal`), not `/tmp`. The server now defaults to that path,
with `DATAMAN_JOURNAL_DIR` as an absolute-path override. Startup creates the leaf
directory if its parent exists, checks ownership/private permissions, holds
`.server.lock`, and performs recovery before workers start. See
[Server operation](server.md) for provisioning and failure behavior.

The caller must serialize journal use and exclude other access, including reads,
to the selected database root for the entire transaction and recovery. This
prototype does not acquire that exclusion itself. Targets are existing regular
files addressed relative to the root, including `files/`, `index/`, and `blobs/`
subdirectories. Every component is opened without following symlinks; absolute
target paths, empty components, `.` and `..` are rejected. Paths are limited to
4095 bytes and components to 255 bytes. Aliases of the journal itself are rejected.
Recorded device/inode identities prevent recovery from silently writing a
replacement file or database root at the same path. The journal directory's
identity is also recorded; moving a journal to another directory is unsupported.
A missing or replaced database root fails recovery and retains the journal.

An update may overwrite or extend a file, including writing beyond EOF. Each
undo record contains the overwritten bytes and the file length immediately
before that update. Reverse replay restores both. Updates to multiple files
and overlapping updates to the same file are supported. File creation,
replacement, deletion, index caching, and concurrent
access are outside this increment. The pre-transaction files must already
represent a durable baseline.

Limits are 1 MiB per write, 128 nonempty writes, and 64 MiB per journal. Recovery
validates and opens every referenced target before modifying any of them; the
limits bound memory and descriptor use. Resource exhaustion fails recovery and
must block database access. Zero-length writes create no journal records.

### Experimental on-disk format

The journal directory contains at most one `.dataman-undo` file, created exclusively
with mode 0600. This format is experimental, not a released compatibility
contract. Integers are unsigned little-endian; there are no native C structs
on disk. CRCs use IEEE CRC-32 (reflected polynomial `0xedb88320`, initial and
final XOR `0xffffffff`).

The header has a 56-byte fixed prefix followed by the canonical absolute
database-root path, without a terminating NUL:

| Byte offset | Width | Field |
| --- | --- | --- |
| 0 | 8 | `DMUNDO02` magic/version |
| 8 | 8 | Journal-directory device |
| 16 | 8 | Journal-directory inode |
| 24 | 8 | Database-root device |
| 32 | 8 | Database-root inode |
| 40 | 4 | Root-path byte length (1–4095) |
| 44 | 4 | CRC of root-path bytes |
| 48 | 4 | CRC of prefix bytes 0 through 47 |
| 52 | 4 | Reserved zero bytes |

Root aliases are resolved at begin; recovery opens the recorded canonical path
without following symlink components and verifies the root identity before
opening targets. The complete header and journal directory entry are synchronized
before any target can change. Incomplete, corrupt, or older `DMUNDO01` headers
fail closed and retain the journal for inspection. Resolve any old prototype
journals with the matching old code before using this format; automatic salvage
or migration is not implemented.

Each record has a 72-byte header followed by a filename and before-image:

| Byte offset | Width | Field |
| --- | --- | --- |
| 0 | 4 | `DMUR` magic |
| 4 | 4 | Type: 1 = undo, 2 = resolved |
| 8 | 8 | Sequence, starting at 1 |
| 16 | 4 | Total record length |
| 20 | 4 | Root-relative target-path byte length (no terminating NUL) |
| 24 | 8 | Target byte offset |
| 32 | 8 | Previous file length |
| 40 | 8 | Target device |
| 48 | 8 | Target inode |
| 56 | 4 | Before-image byte length |
| 60 | 4 | CRC of filename plus before-image |
| 64 | 4 | CRC of header bytes 0 through 63 |
| 68 | 4 | Reserved zero bytes |

A resolved record has no payload and zero target fields. It means no undo is
needed: either commit data or restored abort data was fully synchronized before
the marker was written. It must be the last record, without trailing bytes.
Exclusive journal creation and sequence numbers identify the one active log;
session/transaction identifiers will be added with server integration.

Recovery ignores only an incomplete final record header, or an incomplete final
payload whose full header has passed validation. Complete checksum failures,
invalid framing, and target identity mismatches fail closed without modifying
targets. Header checksums prevent a damaged length from being accepted as an
incomplete payload. These checks detect corruption; they are not an
authentication mechanism against a writer with filesystem access.

### Ordering and failure handling

Write appends the before-image, synchronizes the journal, then changes the
target. Any write error leaves the transaction abort-only. Commit validates the
journal, synchronizes all referenced files, and appends and synchronizes the
resolved marker before unlinking the journal and synchronizing the directory.

Abort/recovery validates the whole log and target identities first, then
restores before-images and previous lengths in reverse sequence and synchronizes
the files. It removes any validated incomplete tail, synchronizes the journal,
and uses the same resolved-marker and retirement procedure. A crash during
undo leaves the original log available for another full reverse replay. No
recovery progress is discarded until restored data is durable.

Close releases resources without committing, aborting, or deleting the journal.
An unresolved journal abandoned by a disconnected or failed application must be
undone, even if the application intentionally closed without committing. The
session manager now calls abort on disconnect and reaping; live activation still
requires transaction-wide exclusion until abort completes. The standalone
journal cannot detect a connection closing by itself.
Journal presence alone does not prove an unfinished transaction: a durable
resolved marker means only retirement remains, even if the connection then dies.
A failed commit or abort requires close followed by recovery before further
access. A missing journal during recovery still causes a directory sync, so a
previously failed unlink-directory sync is not silently bypassed. A complete
resolved marker may survive an error or lost acknowledgment; callers must not
interpret a failed commit return as proof of rollback.

The test suite covers actual child-process termination at write, sync, marker,
retirement, and recovery boundaries, plus injected partial writes and sync
failures. It checks overlapping updates, sparse extension, multiple files,
repeated recovery, corrupt framing, and wrong target identities. Separate journal
and database directories, missing/replaced roots, root-path corruption, nested
targets, traversal rejection, and abandoned handles are also tested. It also
constructs incomplete journal tails. These are process-crash and I/O-ordering
tests; they do not emulate a storage device losing or reordering cached writes.

Client journal activation remains deferred until inserts, deletes, index changes,
and blobs are all covered. There is no opt-in record-only transaction mode.

### Descriptor identity before transaction writes

`dm_tx_write_fd` and `dm_undo_write_fd` accept an existing descriptor alongside
its root-relative path. Before capturing undo or changing data, the journal
opens the path beneath its recorded root without symlink traversal and checks
that its device/inode match the descriptor. A mismatch rejects the write and
makes the transaction abort-only. The descriptor must be read/write without
append mode; its ownership and current position stay with the caller.

The record and index mutation paths now reach this API through the storage
router, but client transaction commands do not activate it yet. The caller must
still exclude concurrent namespace changes and descriptor close/reuse. File
identity checks do not supply transaction isolation. Dispatch routing, complete
blob mutation coverage, and cached-state restoration remain required.

### Scoped mutation routing

`dm_tx_configure` installs the process-wide storage mutation router before
workers start. Standalone tools that do not install a router retain direct I/O.
Journal append, data application, and recovery use the raw checked write helper
so they cannot recursively enter the router.

A transaction handler calls `dm_tx_enter` with its session and borrowed
`dm_tx_target` bindings (descriptor plus root-relative path), then calls
`dm_tx_leave` on every exit. The scope belongs to the current worker thread and
transaction generation. It cannot be nested or reused for a later transaction.
When a journal is active, unscoped writes are rejected; a missing descriptor
binding or failed journal write makes the owner's transaction abort-only.
Ordinary writes remain concurrent. Begin returns `ENOLOCK` if a previously
admitted ordinary write is still running.

These checks cover routed byte writes, not reads, blob namespace operations,
maintenance, or cached index state. They are not a substitute for dispatch
isolation. Client journal activation remains disabled until those requirements
are met. Handler failures must cause rollback even if all preceding byte writes
succeeded.

### Ordinary dispatch admission

Every ordinary `dbfunc` handler now enters the transaction manager's request
gate, including read and maintenance commands. Multiple ordinary handlers may
run concurrently. Beginning a journal returns `ENOLOCK` until all admitted
handlers and direct writes finish; while a journal owns the server, ordinary
handler admission returns `ENOLOCK`. Recovery-blocked admission returns
`EROLLBACK`. No transaction mutex is held across a handler, avoiding inversion
with file and session-root locks.

Dispatch admits a handler after receiving any request payload and releases
admission immediately after the handler returns, before sending its response.
Thus rejection does not strand the connection partway through sending a record,
and a slow response consumer does not hold database admission. Internal session
close notifications retain their separate cleanup path.

This gate deliberately also rejects the owner's ordinary commands while a
journal is active. Transaction-aware owner dispatch, descriptor bindings, blob
coverage, and cached-state restoration are still required before enabling
client journal execution. The gate covers this server's handlers, not external
utilities accessing the same files.
