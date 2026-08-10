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
it. A CMake migration would be a separate compatibility project, not a required
step for current development.

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

## Documentation policy

Update this directory when behavior changes. Preserve `old_docs/` as historical
material until each useful detail has either been incorporated here or declared
obsolete. Prefer a focused guide and links to authoritative headers over a
separate page that merely repeats every function prototype.
