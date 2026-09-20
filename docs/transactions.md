# Transactions and locking

## Client transactions

The `add_transactions` branch now executes transactions in the storage server.
The connection server forwards commands; it no longer collects mutations for
later execution. Open the indexes you need before starting a transaction.

1. Call `start_transaction` (Java: `startTransaction`).
2. Read and change records, index entries, and blobs. Changes execute immediately;
   the owning connection can read its own changes.
3. Commit to make the changes durable, or roll back to restore the original state.

One transaction owns the server at a time, across all database roots. Other
connections receive `ENOLOCK` while that transaction is active; workers do not
wait holding resources needed by its commit or rollback. Retry contention at an
application-appropriate boundary. Keep transactions short and avoid waiting for
user input while a transaction is open.

Standalone record/index operations use implicit transactions. This also covers
index reads that remove stale keys for deleted records. The conservative first
implementation creates a journal even for those requests that end up read-only;
optimizing this overhead is future work.

Every connection declares one root through `INIT_DAT`, `DEF_ROOT`, or `MKIDX`.
Descriptor bindings come from server metadata and must remain within that root.
An explicit transaction cannot open/close indexes, initialize/change its root,
build/sort an index, or release/advance a work-file session using `RELEASE`.
Those operations manage handle lifetimes or perform maintenance and must run
outside the transaction. Index construction retains its existing unavailable
marker until publication; maintenance is not part of the transactional guarantee.

Record insert/update/delete, index include/remove (including automatic
stale-key removal), and blob replacement/create/truncate/rename/unlink participate
in undo. Index root positions and generations, including cached aliases, are
reloaded before admission reopens after commit or rollback. In-memory cooperative
record protections are not transactional; clear protections explicitly.

A negative handler result makes an explicit transaction abort-only. Commit then
returns an error; call rollback. C, C++, and Java retain their transaction flag
when commit fails. On successful rollback they discard cached master/work record
fields so later navigation cannot flush abandoned edits. Reposition/reload records
before editing again; saved application values and cursors are not restored.

Disconnecting with an open journal aborts it, including an intentional application
exit without commit. A periodic sweep handles failed connections. Startup recovery
handles storage-server crashes before workers can access databases. If recovery or
completion cannot safely finish, admission stays blocked and service requires
restart/recovery. Never delete the journal to bypass an error.

A lost commit response means **unknown outcome**, not proof of rollback. The
commit marker may already be durable. Reconcile application state before retrying;
there is no transaction-outcome query or deduplication protocol yet.

These guarantees require exclusive database access through this server and a
filesystem/storage stack that honors synchronization. Stop the service before
using offline mutation utilities or replacing database files. The tests exercise
process crashes and injected I/O errors, not physical power-loss behavior.

## Journal location and ordering

The service owns `/var/lib/dataman/journal`, independent of application roots.
`DATAMAN_JOURNAL_DIR` can override it with an absolute persistent path. Provision
its parent directory with service ownership; startup creates/checks the private
leaf directory, takes the lifetime `.server.lock`, and recovers before workers
start. See [Server operation](server.md). Do not use `/tmp` for production journals.
Changing the journal path does not support multiple server instances.

Each journal header records the canonical database root and its device/inode.
Recovery discovers the root without a client. Missing/replaced roots, target
identity changes, invalid checksums, and unsupported metadata stop recovery.

Before each byte mutation the journal persists its before-image and original
file length. Blob names receive one durable snapshot of their original state
before their first mutation. Commit synchronizes affected files and directories,
then writes and synchronizes a resolved marker. Abort replays undo in reverse,
synchronizes restored files/directories, then writes the same marker. Only then
may the journal be retired and admission reopen.

An interrupted undo can run again. A valid resolved marker means only journal
retirement remains; recovery must not undo that transaction. Uncertain commit or
abort I/O blocks further operations until recovery resolves the on-disk state.

## Blob sizes and metadata

Version 4 removes the prototype's 1 MiB blob limit, 64 MiB journal limit, and
128-record limit. Blob capture, validation, and replay stream in 64 KiB chunks.
The journal does not allocate an entire old blob in memory. File-offset limits,
available disk space, and process resources still apply. Byte-write calls remain
limited to 1 MiB each; record and index writes fit within that bound.

The client wire format still has signed 32-bit frame/blob lengths. Consequently,
a transmitted record plus blobs must fit below the roughly 2 GiB protocol ceiling,
and the current request handler buffers its incoming payload. Removing the
journal cap does not remove these transport and memory constraints.

Snapshots restore contents, length, existence, mode, owner, and group. Rename
captures both names, including an overwritten destination. Restored blobs may
have new inode numbers. Only private regular files directly beneath `blobs/` are
supported: symlinks, hardlinks, and original files/parents with extended attributes
or ACLs are rejected before mutation. External namespace changes must remain
excluded through recovery because snapshots restore logical names.

## Experimental on-disk format

The journal is `.dataman-undo`, created exclusively with mode 0600. Integers are
little-endian; no native C structs are written. Checksums use IEEE CRC-32.
This development format is not a released compatibility contract. Version 2 byte
journals and version 3 bounded blob journals remain readable; version 1 is rejected.

The header is a 56-byte prefix followed by the canonical absolute root path:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 8 | `DMUNDO04` |
| 8 | 8 | Journal-directory device |
| 16 | 8 | Journal-directory inode |
| 24 | 8 | Database-root device |
| 32 | 8 | Database-root inode |
| 40 | 4 | Root-path length (1–4095) |
| 44 | 4 | Root-path CRC |
| 48 | 4 | CRC of prefix bytes 0–47 |
| 52 | 4 | Reserved zero |

Each record begins with 72 bytes, followed by a root-relative filename and payload:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | `DMUR` |
| 4 | 4 | Type: 1 byte undo, 2 resolved, 3 legacy blob, 4 streamed blob |
| 8 | 8 | Sequence, starting at 1 |
| 16 | 4 | Framed length, excluding a type-4 chunk stream |
| 20 | 4 | Filename length |
| 24 | 8 | Target byte offset, or original blob presence (0/1) |
| 32 | 8 | Original file length |
| 40 | 8 | Target device, or blob parent-directory device |
| 48 | 8 | Target inode, or blob parent-directory inode |
| 56 | 4 | Framed payload length |
| 60 | 4 | CRC of filename plus framed payload |
| 64 | 4 | CRC of header bytes 0–63 |
| 68 | 4 | Reserved zero |

Type 1 stores the overwritten bytes. Reverse replay writes them back and restores
that write's original file length. Byte undo cannot target `blobs/` in versions
3 and 4. Paths reject traversal and symlink components; descriptor-bound writes
verify that the named target still matches the caller's open descriptor.

For a present type-4 blob, the framed payload is 16 bytes: mode, uid, gid, reserved
zero (four uint32 values). It is followed by chunks, each containing uint32 length,
uint32 data CRC, and up to 65,536 data bytes. All chunks except the last are full;
their total data length equals the original file length. Absent blobs have no
metadata or chunks. Type 3 instead stores metadata and contents in one bounded
payload and is accepted only in version 3 journals.

Type 2 has zero target fields and no payload. It must be the last record. Recovery
validates the complete log and targets before changing data. An incomplete final
header or incomplete payload/chunk stream with a valid header is an unapplied tail;
complete checksum failures or ambiguous framing fail closed. These checks detect
corruption; they do not authenticate a journal against a filesystem writer.

## Implementation and validation

`transaction_wire.c` selects ordinary maintenance admission or journaled command
execution. `transaction_dispatch.c` and `transaction_session.c` bind the admitted
worker to its session/generation and target descriptors. The storage router rejects
unscoped writes while a journal is active. Journal I/O uses raw checked positional
writes to avoid recursively entering that router.

Standalone tests cover overlapping byte undo, file extension, record links,
index splits/root caches, blob namespace operations, large streamed snapshots,
corrupt framing, partial writes, sync failures, and interrupted recovery. Live
Java/protocol tests cover client transactions, read-your-writes, automatic key
removal, competing sessions, and disconnect undo. See [Tests](../tests/README.md).

### Protocol compatibility after legacy-handler removal

Removing `GET_REC` and `UNDELETE` renumbered commands from `DELETE` onward
by two (`DELETE` is now 14, `FLUSH` 23, and `DISCON` 24). This is incompatible
with the 4.1.0 wire protocol. Deploy rebuilt servers and C/C++ client libraries
together with the updated Java client; rebuild/relink statically linked clients
and update PHP deployments using the C library. Existing client binaries using
the old command numbers must not connect to the new server. The connection
handshake does not negotiate a protocol version or reject this mismatch.
