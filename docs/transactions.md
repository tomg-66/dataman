# Transactions and locking

## Current transaction model

`start_transaction` begins client-side command collection. `commit` sends or
executes the collected operations, and `rollback` discards or reverses work as
supported by the current implementation.

This is not a full ACID transaction manager. In particular, applications must
not assume durable write-ahead logging, crash recovery across every operation,
or general isolation from concurrent clients. Treat transaction grouping as a
useful application feature, not as the equivalent of an SQL database commit.

The work record is working storage, not independently protected transactional
state.

## Cooperative record protection

Use `protect` before changing a record that another client could update, and
`clear` when the protected operation is complete. Structure every error path so
that an acquired protection is cleared. Keep the protected interval short and
never wait for user interaction while holding it.

A robust update flow is:

1. Locate the record through an update-mode index.
2. Protect it and handle failure without modifying state.
3. Copy or edit the work/master data.
4. Save, insert, include, remove, or delete as required.
5. Clear protection in both success and failure cleanup.

## Application guidance

Make multi-step operations idempotent where possible. Record enough application
state to detect an interrupted operation, verify outcomes after reconnecting,
and keep independent backups. Work that requires strict atomicity, durability,
or isolation should wait for a future journaled transaction design or be
coordinated by a system that provides those guarantees.
