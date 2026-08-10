# PHP extension

The PHP extension is in `clientlib/php` and targets current PHP releases. It is
built out-of-tree with PHP's extension toolchain; rebuilding PHP itself is not
required.

## Build

Install Dataman's C headers/library first, then run:

```sh
cd clientlib/php
phpize
./configure --with-dataman=/usr/local
make
make test
sudo make install
```

The prefix passed to `--with-dataman` must contain
`include/dataman/dataman.h` and the Dataman client library. Enable the installed
module in the appropriate CLI/FPM/Apache `php.ini`:

```ini
extension=dataman
```

For an uninstalled smoke test:

```sh
php -d extension="$PWD/modules/dataman.so" \
    -r 'var_dump(extension_loaded("dataman"));'
```

## Records

At request startup the extension publishes exactly two record objects:
`$masterRecord` and `$workRecord`. They are internal, final objects; user code
must not instantiate, clone, serialize, or replace their role. They provide
array-style access to fields. The extension refreshes their content after
successful database operations.

## Basic use

```php
<?php
dataman_connect($argc, $argv);
dataman_iopen('customers', UPDATE);

if (dataman_get_first('customers')) {
    echo dataman_key_str(), PHP_EOL;
}

dataman_iclose('customers');
```

The extension supplies `BEFORE`, `AFTER`, `RDONLY`, and `UPDATE`. Navigation,
insert/include/remove/delete, protect/clear, save/restore, transaction, and
record-state functions are declared in
[`clientlib/php/dataman.stub.php`](../clientlib/php/dataman.stub.php).

## Errors and cleanup

Functions return `bool` where the stub declares it; check every such result.
Explicit `dataman_iclose()` removes an index from request-shutdown cleanup,
while remaining open indexes are closed automatically.  The socket connection
to the server is shut down in an orderly manner when the script terminates.

Run the PHPT suite after extension or protocol changes. Tests `001` through
`011` currently exercise loading, validation, connection, navigation,
save/restore, mutation, and automatic removal of keys pointing to deleted
records.
