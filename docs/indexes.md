# V2 indexes

## Format and compatibility

Current indexes use the `DMIDXV2` format. The header records format metadata,
two root publication slots, generation information, checksums, and rebuild
state. Nodes are fixed-size pages containing ordered composite entries and, for
internal nodes, child page offsets.

Only V2 indexes are supported. `iopen`, `dumpix`, and `dnodes` reject legacy
files. Rebuild an old index from its authoritative data file; there is no mixed
V1/V2 runtime mode.

## Normal mutation

Normal insertion and removal use copy-on-write (COW): changed pages are written
as a new generation, then a valid root slot publishes the new tree. Readers see
one published root, and old pages can be reclaimed later by the retained-root
allocator.

Removal rebalances only the affected path. A non-root node has a minimum
occupancy of six entries. Underflow borrows from the left sibling, then the
right, or merges when borrowing is impossible. Parent separators are the first
full composite entry of the right child. Merges can propagate upward, and an
empty internal root collapses to its sole child.

This keeps deletion writes bounded by tree height instead of rebuilding or
growing the whole index.

## Rebuilding

The sort/build path is deliberately in-place. Rebuilds have exclusive access,
so COW would create obsolete generations and extra allocator scans without
providing useful reader isolation. The builder produces dense pages, splitting
nodes as keys arrive.

The header's build marker prevents a partially rebuilt index from being opened
or dumped as a normal index. A successful finalization publishes the completed
root and clears that state.

## Lookup and patterns

Exact lookup descends by separator order. Pattern lookup uses `*` as a wildcard
for exactly one character. A candidate matches when it agrees with the template
through the template's length; trailing candidate characters do not invalidate
the match. This is prefix-template behavior, not shell globbing or a regular
expression.

## Inspection

```sh
dumpix -r /database/root index-name
dnodes -r /database/root index-name
```

Use `dumpix` to verify key/record mappings. Use `dnodes` when checking roots,
generations, page occupancy, separators, and child offsets.
