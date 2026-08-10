# Utilities

## `mkdf`

Creates a data file from an input description:

```sh
mkdf new-file input-file.i
```

The output is placed under `$ROOT/files`. Keep the input description in source
control; it is part of the application's persistent data contract.

## `dumpdf`

Prints the contents of a data file:

```sh
dumpdf [-r root] file-name
```

## `dumpix`

Prints the logical entries in a V2 index:

```sh
dumpix [-r root] index-name
```

It rejects legacy and incomplete-build indexes.

## `dnodes`

Prints the physical V2 tree, including root slots and node relationships:

```sh
dnodes [-r database-root] index-name
```

This is a diagnostic companion to `dumpix`, not normal application machinery.

## `dbclean`

Performs configured database cleanup and integrity work. With no filename it
reads `Cleanfile` from the current directory; command-line switches also permit
an explicit file, root, minimum value, informational mode, and verbose output.
Always inspect the configured root and rules before running it against valuable
data.

## `dfedit`

Provides low-level data-file editing:

```sh
dfedit data-file
```

Use it only with a backup and while normal database access is stopped. Logical
client operations are safer for routine changes.
