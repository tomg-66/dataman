# Tests

## Standalone storage tests

The server storage tests run without a database service or fixtures:

```sh
cmake -S . -B /tmp/dataman-build -DBUILD_TESTING=ON
cmake --build /tmp/dataman-build --target storage_io_test flush_io_test undo_journal_test session_root_test journal_startup_test verify_pid_test transaction_session_test
ctest --test-dir /tmp/dataman-build -R '^(storage-io|flush-io|undo-journal|session-root|journal-startup|pid-ownership|transaction-session)$' --output-on-failure
```

They also run through Automake's `make check` after building the project.
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

`legacy_transaction_test` checks connection-side transaction cleanup across
repeated transactions, commit reply handling, and propagation of server errors
during rollback. Transport is stubbed; no running database or IPC is required.

`record_mutation_test` uses a temporary data file and production insert, delete,
and undelete handlers to check record flags, both neighboring links, and the
first-record pointer at the beginning and middle of a record chain. It needs no
running server. This checks direct-write behavior, not transaction atomicity.

The undo-journal tests also cover descriptor/path identity, replaced paths,
invalid descriptor modes, preservation of descriptor position, and recovery
of descriptor-bound writes. Transaction-session tests check ownership and
abort-only behavior through the descriptor-aware API.

Transaction-session integration coverage now runs the production flush, insert,
delete, undelete, and v2 index insert/remove paths through the mutation router.
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
