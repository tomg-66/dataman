# Tests

## Standalone storage tests

The server storage tests run without a database service or fixtures:

```sh
cmake -S . -B /tmp/dataman-build -DBUILD_TESTING=ON
cmake --build /tmp/dataman-build --target storage_io_test flush_io_test undo_journal_test
ctest --test-dir /tmp/dataman-build -R '^(storage-io|flush-io|undo-journal)$' --output-on-failure
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

The journal is not integrated with the server yet. These tests do not establish
live-server ACID guarantees or emulate power loss in a storage device.

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
