# Building with CMake

CMake is an additional build path; it does not replace Autoconf and Automake.
Both describe the same source targets, allowing existing packaging to continue
while new environments can use CMake.

## The basic workflow

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cmake --install build --prefix /usr/local
```

The important pieces are:

- `-S .` identifies the source directory containing the top-level
  `CMakeLists.txt`.
- `-B build` creates a separate build directory. Generated files and objects do
  not mix with source files.
- The configure step detects compilers and dependencies and writes a persistent
  cache in the build directory.
- `cmake --build` invokes the native build tool selected by CMake.
- `ctest` runs tests registered by the project.
- `cmake --install` copies completed artifacts to the selected prefix.

After editing a `CMakeLists.txt`, running `cmake --build build` normally causes
CMake to regenerate the build automatically. To start over completely, remove
the build directory and configure it again; no generated CMake files need to be
removed from the source tree.

## Project options

Options are selected during configuration with `-DNAME=ON` or `-DNAME=OFF`:

| Option | Default | Meaning |
| --- | --- | --- |
| `DATAMAN_BUILD_JAVA` | `OFF` | Build and install `dataman.jar`. |
| `DATAMAN_BUILD_DFEDIT_STATIC` | `OFF` | Request a statically linked `dfedit`; requires static system libraries. |
| `DATAMAN_ENABLE_INTEGRATION_TESTS` | `OFF` | Register tests requiring a running Dataman server. |
| `BUILD_TESTING` | `ON` | Enable ordinary CTest smoke tests. |

For example:

```sh
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DDATAMAN_BUILD_JAVA=ON
```

Configuration choices are cached in `build/CMakeCache.txt`. Repeating the
configure command with a different `-D` value updates that choice.

## Targets and output names

CMake target names identify nodes in the build graph and must be globally
unique. They do not have to equal installed filenames. For example, the C
library target is internally named `dataman_c` to distinguish it from the
`dataman` server executable, but its output remains `libdataman.so`.

The current build produces:

- `libdataman` and `libdataman++` shared client libraries;
- `dataman`, `dataman_con`, and `dataman_srv`;
- `mkdf`, `dumpdf`, `dumpix`, `dnodes`, `dbclean`, and `dfedit`;
- `dataman.jar` when Java is enabled.

The shared-library version and SONAME derive from the top-level project version.
For release 4.0.0, CMake installs `libdataman.so.4.0.0` with SONAME 4 and the
normal symbolic links.

## Dependencies

The core build requires C and C++ compilers, Curses, and POSIX threads. Enabling
Java also requires a JDK. CMake reports a clear configure-time error when a
required dependency is missing.

The PHP extension continues to use PHP's `phpize` build process. Install Dataman
to a staging prefix, then pass that prefix to the extension's
`--with-dataman=PREFIX` option.

## Staged installation

To inspect installation without modifying the system:

```sh
cmake --install build --prefix /tmp/dataman-stage
find /tmp/dataman-stage -maxdepth 4 -type f
```

Headers install beneath `include/dataman`, libraries beneath `lib` (or the
platform's configured library directory), executables beneath `bin`, and the
optional Java archive beneath `share/java`.
