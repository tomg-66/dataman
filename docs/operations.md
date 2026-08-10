# Operation reference

Names vary slightly between bindings—the C implementation often uses a `db_`
prefix and PHP uses `dataman_`—but the database operations share these meanings.
Consult the installed header or generated PHP arginfo for exact signatures.

## Connection and index lifecycle

| Operation | Purpose |
| --- | --- |
| `init_dataman` / `connect` | Establish client state and a server connection. |
| `iopen` | Open a named index read-only or for update. |
| `iclose` | Close an open index. |
| `mkidx` | Create a new index definition/build context. |
| `release` | Move the work record to the next in the work file. |
| `sort` | Submit a key while building an index. |

## Navigation

| Operation | Purpose |
| --- | --- |
| `get` | Find a key or supported key pattern. |
| `get_first`, `get_last` | Move to the lowest/highest composite entry. |
| `get_next`, `get_prior` | Move in index order. |
| `get_current` | Revalidate and read the current index entry. |
| `forward`, `back` | Move in physical data-file order. |
| `save`, `restore` | Save and restore the current index position. |

A successful navigation updates the current key, file, format, and master
record. End-of-index and failure are distinct conditions in the low-level
protocol; use the binding's documented return/exception behavior.

## Mutation and coordination

| Operation | Purpose |
| --- | --- |
| `insert` | Create a record before or after the current physical position and index it. |
| `include` | Add an index key referring to an existing record. |
| `remove` | Remove an index key without deleting its record. |
| `delete` / `delrec` | Delete the current record and its current index reference. |
| `protect`, `clear` | Acquire and release cooperative record protection. |
| `start_transaction` | Begin collecting a group of commands. |
| `commit`, `rollback` | Apply or abandon the collected group within current limitations. |

See [Transactions and locking](transactions.md) before relying on grouped
operations for correctness.

## Records and fields

`get_format`, `get_key`, `key_str`, `get_index`, and `get_file` expose current
state in bindings that provide procedural accessors. Blob and typed-field access
belong to the record/field classes or generated format headers.

The C library also contains a legacy terminal-window helper API (`accept`,
`show`, `mask`, window creation/stack operations, and related routines). It is
not part of the database wire protocol and is retained for older C applications;
new user interfaces should use their platform's normal UI facilities.
