# Tests

## Standalone storage tests

The Python protocol tests share named commands from `protocol_commands.py`.
Run `python3 tests/protocol_mapping_test.py` to check those constants, the shared
C/C++ server header, and Java constants and labels against an independent,
explicit wire-number mapping. CMake registers this as `protocol-mapping` when
Python is available. The C framing tests retain literal wire messages to check
the actual framing contract.

`protocol_handshake_test` checks the native client/server greeting over real
socket pairs: matching and mismatched versions, legacy greetings/replies,
fragmented I/O, truncated greetings, and read deadlines. It also checks that
bytes following a greeting are preserved. The isolated recovery test checks
these greetings against the running connection server, including a coalesced
greeting and initialization command. Socket tests need an environment that
allows socket I/O.

The Java client handshake has a separate loopback test:

```sh
javac -d /tmp/dataman-java-handshake clientlib/java/*.java tests/java/ProtocolHandshakeTest.java
java -cp /tmp/dataman-java-handshake Dataman.ProtocolHandshakeTest
```

It checks fragmented success replies, version mismatch, an unversioned `ok`
reply, rejection, and EOF. `run_java_tests.sh` also runs it.

The server storage tests run without a database service or fixtures:

```sh
cmake -S . -B /tmp/dataman-build -DBUILD_TESTING=ON
cmake --build /tmp/dataman-build --target storage_io_test flush_io_test undo_journal_test session_root_test journal_startup_test verify_pid_test transaction_session_test
ctest --test-dir /tmp/dataman-build -R '^(storage-io|flush-io|undo-journal|session-root|journal-startup|pid-ownership|transaction-session)$' --output-on-failure
```

They also run through Automake's `make check` after building the project.
`index_find_test` (CTest: `index-find`) checks prefix and full-key searches
across leaf and internal-subtree boundaries, including `#9999` finding
`#99999`, duplicate keys, exact record searches, and absent keys. It creates
temporary indexes using both normal insertion and the bulk-build API.

`storage_io_test` exercises real temporary files with injected interrupted,
short, zero-progress, and failed system calls. It checks EOF handling, range
validation, and preservation of the descriptor position. `flush_io_test`
verifies that record-write failures prevent blob writes and cannot become a
successful response, and that cleanup releases the lock and payload.

`undo_journal_test` exercises the standalone existing-file journal with private
temporary directories and child processes terminated at deterministic crash
boundaries. It also injects partial journal/data writes and failed syncs,
verifies repeated/interrupted recovery, and checks corrupt logs and replaced
target files. No running server or existing database is touched.
The journal and database use separate temporary directories. Root-discovery
tests recover abandoned work without client state, reject missing/replaced roots
and corrupt root paths, and exercise nested targets without following symlinks.

Live transaction writes now use the journal. Standalone tests do not emulate
power loss in a storage device; Java/protocol tests exercise the live server.

`session_root_test` checks `DEF_ROOT`, `INIT_DAT`, and `MKIDX` root registration
with substituted IPC, work-file initialization, and index creation. It covers
repeated registration, root-change rejection before index creation, failed
initialization, malformed requests, and reused PIDs
with new shared-memory identifiers. It does not exercise a network connection.

`journal_startup_test` runs the actual `dbserve` initialization in child
processes with substituted PID checks, message queues, and worker creation.
Journal I/O, recovery, directory permissions, and persistent locking are real.
It checks fresh startup, unfinished/resolved journals, unavailable database
roots, duplicate ownership, and rejection before workers start. Tests use
private temporary directories and do not touch production IPC or `/var/lib`.

`verify_pid_test` uses a unique temporary PID filename and a separate executable
process to check that a failed contender does not unlink the active owner's PID
file. It also checks close-on-exec on the held lock descriptor.

`transaction_session_test` combines the real root registry, journal owner manager,
and undo I/O with substituted IPC identity/liveness. It checks duplicate begin,
contention, non-owner rejection, commit, abort-only writes, explicit disconnect,
orphan reaping, process death, and failed abort that blocks further admission.
These are internal lifecycle tests, not end-to-end client transactions.

## Integration tests

### Isolated multi-file crash recovery (Linux)

This test starts and stops its own test servers; no installed server or existing
database is used. It requires Python 3, System V IPC, and permission to open a
loopback TCP socket. Build and run it with:

```sh
cmake -S . -B /tmp/dataman-recovery-build -DBUILD_TESTING=ON -DDATAMAN_ENABLE_RECOVERY_TESTS=ON
cmake --build /tmp/dataman-recovery-build --target recovery_srv recovery_con mkdf -j4
ctest --test-dir /tmp/dataman-recovery-build -R '^transaction-recovery$' --output-on-failure
```

`transaction_recovery_test.py` creates three disposable datafiles and three indexes,
including a blob-bearing file and indexes with multiple leaves. In one transaction
it updates records, changes keys, inserts records/keys, and deletes records/keys.
Blob operations include repeated replacement, truncation to zero, creation on an
existing record, insertion of a record with a blob, and deletion of a blob-bearing
record. Binary payloads exceed 1 MiB and cross journal/shared-memory chunk boundaries.
It kills the storage server before disconnecting the client, confirms that the
changed files and hidden `.dataman-undo` journal remain, and restarts the servers.
Recovery must restore every datafile, index, and original blob byte-for-byte,
remove newly created blobs, preserve originally absent blobs, and retire the
journal. Lookups and forward/reverse index traversal must match the baseline.
The test also verifies explicit rollback with open indexes and checks that a
successful commit survives a forced stop and restart.

The test also checks these failures through a Python TCP client:

- Competing begin/read/write requests return `ENOLOCK` without changing owner data.
- An injected `ENOSPC` data write makes the transaction abort-only; commit fails,
  ownership remains held, and explicit rollback restores records, indexes, and blobs.
- Client disconnect undoes the entire transaction before admitting another owner.
- An injected commit `fsync` error reports failure and blocks further work;
  restart recovers the uncommitted changes.
- A failed startup undo write refuses service and retains its journal;
  a subsequent restart completes recovery, including already partially undone blobs.

The test binaries use the production server sources with linker wrappers
for resource isolation and one-shot I/O failure injection: a reserved message
queue, unique PID files, an ephemeral loopback port, and disabled signaling of
the installed server.
Journaling, protocol handling, mutation handlers, and startup recovery are real.
The test removes its own processes, IPC resources, and temporary files afterward.
These test binaries are not installed. This is a process-crash test, not a
simulation of hardware power loss. The failure cases exercise client-visible wire
responses; they do not replace separate C/C++/Java client-library tests.

### Existing-server client tests

The PHP and Java integration tests share the data-file definitions in
`fixtures/`. Each language gets a separate database root so destructive tests
do not interfere with another binding.

The tests require:

- `mkdf` on `PATH`;
- a Dataman server listening on the test host;
- permission for that server to access this checkout's `tests` directory;
- PHP or a JDK, depending on the binding being tested.

## Java

Run the Java sequence with:

```sh
./tests/run_java_tests.sh
```

Set a non-default server host with:

```sh
DATAMAN_TEST_HOST=database-host ./tests/run_java_tests.sh
```

The harness performs these steps:

1. Rebuild `tests/java/files/one_rec` and remove the previous test index.
2. Compile the Java client and test programs into a temporary directory.
3. Run `BuildOneRecordIndex` in a separate JVM, allowing shutdown cleanup to
   publish and close the completed V2 index.
4. Run `OneRecordIntegrationTest`, which exercises insert, include, delete,
   navigation, and automatic removal of a key targeting a deleted record.

The generated database remains under `tests/java` for inspection. Compiled
classes are removed automatically.

## PHP

PHP test 010 rebuilds the PHP fixture and constructs `one_rec_idx`. Tests 011
and 012 then perform the equivalent mutation and automatic-removal sequence.
Those three tests are intentionally ordered and are not yet independently
isolated. Test 013 rebuilds the fixtures and constructs the separate
`blob_rec_idx`; tests 014 and 015 then exercise master-record fields, metadata,
blob replacement, field boundaries, and repeated index open/close cycles.

`record_mutation_test` uses a temporary data file and production insert and delete
handlers to check record flags, both neighboring links, and the
first-record pointer at the beginning and middle of a record chain. It needs no
running server. This checks direct-write behavior, not transaction atomicity.

The undo-journal tests also cover descriptor/path identity, replaced paths,
invalid descriptor modes, preservation of descriptor position, and recovery
of descriptor-bound writes. Transaction-session tests check ownership and
abort-only behavior through the descriptor-aware API.

Transaction-session integration coverage now runs the production flush, insert,
delete, and v2 index insert/remove paths through the mutation router.
It checks byte-for-byte rollback and original file lengths after record and
index growth, committed record contents, recovery after process exit, rejection
of writes from an unscoped worker, missing descriptor bindings, and stale scopes.
Production blob processing is included in the owner-dispatch and wire-dispatch
cases; separate live tests exercise the TCP connection path.

Transaction-session admission tests use two synchronized worker threads to
verify concurrent ordinary requests, exclusion of journal begin until both
requests finish, rejection during an active journal, safe duplicate leave,
and admission reopening after abort. Recovery-blocked admission is also checked.

Owner-dispatch tests derive bindings from absolute paths, reject paths outside
the registered root and traversal components, exercise production flush through
the wrapper, and check abort-only propagation from a handler failure. A second
worker verifies that owner scopes serialize and journal completion is blocked
until the active scope returns. Read-only scopes require no descriptor bindings.

`blob_io_test` uses temporary blob files and injected write, close, rename, and
unlink failures. It covers replacement, hide/unhide, hidden cleanup, deletion,
missing directories, and rejection before truncation by the namespace guard.
The transaction-session suite verifies that rejected owner namespace operations
make the transaction abort-only.

Blob journal tests cover replacement/truncation, creation, deletion, rename over
an existing destination, repeated changes to the same names, empty blobs, and
mode restoration. Forked tests interrupt journal append, blob mutation, commit,
and recovery itself. Failure tests cover partial writes, failed file/directory
sync, torn tails, corruption, hardlinks, extended metadata, and sizes beyond the former limits. A
version-2 byte journal recovery test covers the format transition. Production
flush and blob-control handlers are tested together through owner dispatch for
record-plus-blob abort, commit, and recovery after process exit.

Index cache tests cover rollback after page splits, duplicate/alias tracking,
commit refresh, and disconnect undo through the production remove-key handler.
A closed tracked descriptor causes completion to block admission without
publishing cache values. Tests compare the cache with the durable v2 header.

## Live transaction checks

`run_java_tests.sh` also runs `TransactionIntegrationTest` and
`transaction_protocol_test.py` against the disposable Java fixture. They check
read-your-writes, commit, rollback of inserts/deletes, client buffer discard,
contention between connections, and undo after disconnect. These tests modify the
fixture and require the development server; do not point them at production data.

The journal suite additionally restores a blob larger than the former 64 MiB
journal cap and interrupts streamed replay. Blob snapshot memory is bounded by
chunk size; the existing client transport still uses signed 32-bit lengths.

## Host provisioning

Run `python3 tests/system_setup_test.py` to check account creation, idempotent
service registration, conflict detection, and staged/non-root install behavior.
The test uses temporary files and mocked account commands; it does not modify
host users, `/etc/services`, or `/var/lib/dataman`.

`serial_prefix_test` exercises the production connection frame parser using pipes.
It accepts both legacy `26` and delimited `26|` disconnects, index/work-file close
commands, and transaction controls; malformed prefixes and mismatched FLUSH
payload lengths remain rejected. It runs under CTest and `make check`.

## Foreground supervisor

Run `python3 tests/supervisor_foreground_test.py /path/to/built/dataman`. It uses
a uniquely named supervisor and fake child processes, checking foreground PID
ownership, duplicate-start rejection, and child shutdown without a database.
The generated systemd unit can also be checked on a systemd host with
`systemd-analyze verify /path/to/dataman.service` after installing its binaries.
