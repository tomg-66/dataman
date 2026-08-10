# Getting started

## Prerequisites

The core build uses Autoconf and Automake. Install a C and C++ compiler,
Autoconf, Automake, Libtool, Make, and the normal POSIX development headers.
Java support is optional. The PHP extension has a separate build and requires
the PHP development package (`phpize`, `php-config`, and headers).

## Build and install

From a release archive:

```sh
./configure
make
make check
sudo make install
```

From a source checkout that does not yet contain generated build files:

```sh
autoreconf -fi
./configure
make
make check
sudo make install
```

Enable the Java subdirectory with `./configure --enable-java`. See the
[PHP extension guide](php-extension.md) for its separate build.

## Create a database root

Dataman expects a root directory containing these subdirectories:

```text
database-root/
├── blobs/
├── files/
└── index/
```

Set `ROOT` for server utilities and local clients:

```sh
export ROOT=/srv/dataman/example
mkdir -p "$ROOT/files" "$ROOT/index" "$ROOT/blobs"
```

Create data files with `mkdf`, then build their indexes with the client-side
`mkidx`/`sort` workflow. Input definitions and key construction are application
specific; begin from a known working definition in this repository or from an
existing application.

## Start and stop the service

Start the supervisor in the foreground while configuring a new installation:

```sh
dataman -D
```

The supervisor starts and watches the connection and database services. Clients
connect to the connection service (historically TCP port 8758). Use the normal
termination signal for an orderly shutdown:

```sh
kill -TERM "$(cat /path/to/dataman.pid)"
```

The precise PID-file location and service wrapper are installation choices;
verify them before using the example command.

## Verify the installation

Inspect a data file and a V2 index without opening them through a client:

```sh
dumpdf -r "$ROOT" data-file-name
dumpix -r "$ROOT" index-name
dnodes -r "$ROOT" index-name
```

`dumpix` prints logical entries. `dnodes` prints the physical tree and is more
useful when diagnosing index structure.
