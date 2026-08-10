# C++ client API

The C++ binding lives in namespace `Dataman` and uses `.hpp` public headers.
Include the needed class headers (principally `dataman.hpp` and `index.hpp`) and
link with the C++ client library.

## Index objects

Constructing `Dataman::index(name, mode)` opens the index; destroying it closes
the index. Opening can fail, so construct inside a `try` block and catch
`Dataman::datamanError` at an application boundary:

```cpp
try {
    Dataman::index customers(const_cast<char *>("customers"), UPDATE);
    if (customers.get_first()) {
        // inspect Dataman's current master record
    }
} catch (const Dataman::datamanError& error) {
    // log error.what() and choose whether to retry or abort the operation
}
```

Prefer automatic storage or `std::unique_ptr` so constructor failure and later
exceptions cannot leak an index object. `iclose()` is available for explicit
early close; the destructor must not let an exception escape during stack
unwinding.

## Operations

`index` provides exact lookup and ordered navigation, physical `forward`/`back`,
record protection, delete/remove, position save/restore, insert, include, and
explicit close. `get_key()` returns the current key.

The binding preserves the previous current key until a new server reply has
been completely validated and constructed. Communication failure is expressed
as an exception; callers should not expect a usable response buffer on failure.

See [`clientlib/c++/index.hpp`](../clientlib/c++/index.hpp) and
[`clientlib/c++/datamanError.hpp`](../clientlib/c++/datamanError.hpp) for the
current declarations.
