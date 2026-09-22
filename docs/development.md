# Development

## Repository layout

```text
clientlib/c/      C client library and utilities
clientlib/c++/    C++ client library
clientlib/java/   Java client classes
clientlib/php/    PHP extension and PHPT tests
server/           server, V2 index implementation, and admin tools
old_docs/         historical HTML documentation
docs/             current Markdown documentation
```

## Build system

The core project remains on Autoconf/Automake. This is appropriate while the
existing install rules, optional Java build, and downstream packaging depend on
it. CMake is also supported as an optional, out-of-tree build path; see
[Building with CMake](cmake.md).

```sh
autoreconf -fi
./configure --enable-java
make -j
make check
```

Build the PHP extension separately as described in
[php-extension.md](php-extension.md).

## Change checklist

Protocol changes usually touch more than the server. Check:

- the server request parser and response builder;
- C request construction, response parsing, ownership, and errors;
- C++ parsing and exception safety;
- Java byte-order/width-safe parsing;
- PHP arginfo, wrappers, request globals, cleanup, and PHPT tests;
- diagnostic tools when an on-disk format changes.

For incompatible wire changes, update the protocol version and greeting in
`server/protocol.h`, Java's `DatamanComms`, and the Python protocol constants.
Update the independent mapping test deliberately; run the handshake tests as
well as the mutation tests. Release versions and wire-protocol versions are
separate. See [protocol compatibility](transactions.md#protocol-compatibility-after-legacy-handler-removal).

For `get*` replies, keep the key last in the message. Validate delimiters and
numeric fields before replacing client state. The response originates at the
server, but validation remains worthwhile because version skew, truncation, and
transport faults otherwise become memory-safety bugs.

## Index changes

Test exact lookup, wildcard lookup, first/last, next/prior across leaf
boundaries, current-position recovery, duplicates, insertion splits, removal
borrow/merge/root collapse, interrupted rebuilds, and rejection of legacy
headers. Compare `dumpix` logical output with `dnodes` structure after large
randomized builds and deletions.

## Transaction release validation

Before packaging, run the standalone tests and the isolated live recovery test
using the commands in [Tests](../tests/README.md). The recovery test is opt-in
and is not included in a default `make check`. Confirm that all referenced test
sources are tracked and included in the release archive.

Build the release tarfile on a separate development machine, then verify:

- Clean compilation, incremental Java compilation, and the packaged tests.
- Installation provisions the `dataman` user/group, TCP service entry, and private
  persistent journal directory as described in [Server operation](server.md).
- The selected init script or systemd unit starts the service as `dataman`, and
  that account can access the application's database files.
- Updated clients connect successfully; an older protocol client is rejected.
- Using a disposable database, stop the server during an uncommitted transaction
  and verify that restart restores the saved baseline and removes `.dataman-undo`.
- A committed transaction survives restart, and ordinary stop/start works.

Use `ls -la` when inspecting the journal directory: both `.dataman-undo` and
the persistent `.server.lock` are hidden names. Retain the documented limits
around exclusive server access, storage synchronization, maintenance operations,
and unknown outcomes after a lost commit reply in the release notes.

## Documentation policy

Update this directory when behavior changes. Preserve `old_docs/` as historical
material until each useful detail has either been incorporated here or declared
obsolete. Prefer a focused guide and links to authoritative headers over a
separate page that merely repeats every function prototype.
