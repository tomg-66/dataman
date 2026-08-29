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

When `dataman.hpp` is included, `KEY` names the current `key` object and
`KEY_STR` returns only its visible, null-terminated key text. The qualified
binary key used by navigation also contains the file number and record pointer;
access it explicitly with `KEY.get_data()` and use the index key length plus the
qualified-key header length rather than treating it as a C string.

The binding preserves the previous current key until a new server reply has
been completely validated and constructed. Communication failure is expressed
as an exception; callers should not expect a usable response buffer on failure.

## Data fields

`Dataman::datafield` represents either a fixed-width field belonging to a
database record or a standalone value produced by an expression. Assignment to
a record field preserves its defined width: short values are padded with spaces
and long values are truncated. Assignment to a standalone field resizes it to
hold the new value. Only assignment to a record-owned field marks that record
dirty; copies and arithmetic results are standalone values.

Non-blob record fields remain character fields even when assigned a numeric
value. This makes the behavior of `+` depend on the operands rather than the
assignment history:

```cpp
datafield ten("10");
datafield twenty("20");

datafield text = ten + twenty; // "1020": string plus string
datafield sum = ten + 20;      // 30: numeric string plus integer
datafield mixed = datafield("part") + 20; // "part20"
```

Two string fields always concatenate. When one operand is explicitly numeric,
a numeric string participates in addition; a nonnumeric string concatenates
with the formatted numeric value. Multiplication and division are always
numeric operations. They accept complete numeric strings with surrounding
whitespace, but reject partially numeric or nonnumeric strings by throwing
`datamanError`. Division by zero also throws `datamanError`.

An empty or whitespace-only field is numeric zero when an operation requires a
number. Thus a blank field multiplied or divided by a nonzero number produces
zero, while using a blank field as a divisor raises the normal division-by-zero
exception. Two character fields still concatenate with `+`; this rule does not
turn string-to-string addition into arithmetic.

Blob fields support binary data, including embedded null bytes and zero-length
values. Text and arithmetic operations on blobs are rejected. Use `put_blob()`
when the source is a pointer and explicit byte count; copying one blob
`datafield` to another preserves its length and bytes.

See [`clientlib/c++/index.hpp`](../clientlib/c++/index.hpp) and
[`clientlib/c++/datamanError.hpp`](../clientlib/c++/datamanError.hpp) for the
current declarations.
