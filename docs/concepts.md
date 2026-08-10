# Concepts

## Data files and record formats

A Dataman data file contains records described by one or more record formats.
A format defines the fields present in that kind of record. Fixed fields live
in the data file; blob data is stored separately under the database root.

Applications commonly place related formats in the same data file. Physical
record order can represent relationships: for example, a parent record followed
by its detail records. `forward` and `back` walk that physical order, while an
index lookup follows key order.

## Indexes

An index is an ordered mapping from a composite key to a data-file record.
One index may refer to records from multiple data files, and a record may be
included in multiple indexes. Duplicate user-visible key text is possible
because the stored composite entry also identifies the target record.

Normal navigation consists of:

- `get` for a key lookup;
- `get_first`, `get_last`, `get_next`, and `get_prior` for ordered traversal;
- `get_current` to re-read the current entry;
- `save` and `restore` to preserve and return to an index position.

The client retains a node and entry hint after a successful lookup. Subsequent
navigation can probe that location first, but correctness never depends on the
hint: a changed generation or moved key causes a normal search.

## Master and work records

Client libraries expose two record buffers:

- The **master record** is the record obtained through an index or physical-file
  navigation operation.
- The **work record** is application-owned working storage used to construct or
  modify data before an insert or save.

Their exact names differ by language. In PHP they are always the module-created
`$masterRecord` and `$workRecord`; applications cannot construct replacements.

## Common write verbs

- `insert` creates a data record in a database file.
- `include` adds another index entry for an existing record.
- `remove` removes an index entry without deleting the record.
- `delete` deletes the current record and removes its associated index entry.
- `protect` and `clear` provide cooperative record protection around updates.

These verbs intentionally resemble EDITOR database operations. Language
bindings may express failure as a return value, an exception, or a PHP warning;
consult the binding-specific guide.
