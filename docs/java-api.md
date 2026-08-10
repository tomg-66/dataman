# Java client

Java support is optional in the top-level build:

```sh
./configure --enable-java
make
```

`Dataman.initDataman(...)` establishes global client state, including the
master and work records. `DatamanIndex` represents an open index and provides
lookup, ordered navigation, mutation, protection, and position operations.
`DatamanField` provides typed field access, including integer, long, short,
float, string, and blob methods.

## V2 protocol state

The current Java index client sends the V2 generation in navigation requests
and parses generation, node, and entry-offset information returned by `get*`.
These values are cursor hints; applications should not interpret or persist
them.

Use `DatamanField.getLong()` for a Java `long`. The implementation decodes the
wire representation explicitly rather than depending on native C layout, which
is required for portability.

## Error handling

Catch `DatamanRuntimeException` around connection, index construction/open, and
database operations. Close/release resources in `finally` or use a small
application wrapper implementing `AutoCloseable`; the current library classes
predate Java's try-with-resources pattern.

The Java binding is less extensively regression-tested than the C and PHP
paths. Run an application-level smoke test against the same server whenever the
wire protocol changes.
