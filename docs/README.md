# Dataman

Dataman is a networked, record-oriented database derived from the database
verbs of the EDITOR language. It stores typed records in data files and uses
ordered indexes to find those records. The repository contains the server,
administrative tools, and C, C++, Java, and PHP clients.

This directory is the current documentation set. The HTML files in `old_docs/`
describe older releases and remain useful as historical reference, but may not
match the current protocol or V2 index format.

## Start here

- [Getting started](getting-started.md) — build, create a database root, and
  start the server.
- [Concepts](concepts.md) — records, files, indexes, and the master/work model.
- [Server operation](server.md) — processes, configuration, and shutdown.
- [V2 indexes](indexes.md) — current index behavior and legacy migration.
- [Utilities](utilities.md) — inspection, creation, and cleanup tools.
- [C client](c-api.md), [C++ client](cpp-api.md), [PHP extension](php-extension.md),
  and [Java client](java-api.md).
- [Operation reference](operations.md) — the shared database verbs at a glance.
- [Transactions and locking](transactions.md) — guarantees and limitations.
- [Development](development.md) — repository layout, builds, and tests.
- [Building with CMake](cmake.md) — the optional out-of-tree build path.

## Current compatibility boundary

The server operates only on V2 indexes. Opening or dumping an older index is an
error; rebuild it from its data file instead of attempting an in-place upgrade.
This deliberate boundary keeps all active server and client paths on one index
implementation.

Dataman is licensed under GPL version 2 or, at your option, a later version.
See [`COPYING`](../COPYING).
