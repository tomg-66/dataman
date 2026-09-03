<?php

/**
 * @generate-class-entries
 * @undocumentable
 */

/**
 * @param string[] $argv
 */
function dataman_connect(int $argc, array $argv): bool {}
function dataman_iopen(string $indexName, int $mode): bool {}
function dataman_get(string $indexName, string $key) : bool {}
function dataman_mkidx(string ...$args): bool {}
function dataman_sort(string $key): bool {}
function dataman_release(): bool {}
function dataman_iclose(string $indexName): bool {}
function dataman_get_next(string $indexName): bool {}
function dataman_get_prior(string $indexName): bool {}
function dataman_get_current(string $indexName): bool {}
function dataman_get_first(string $indexName) : bool {}
function dataman_get_last(string $indexName) : bool {}
function dataman_forward(?string $indexName = null): bool {}
function dataman_back(?string $indexName = null): bool {}
function dataman_insert(int $fmt, int $placement, string $indexname): bool {}
function dataman_include(string $sourceIndex, string $destIndex, string $key): bool {}
function dataman_remove(string $key, string $indexNmae): bool {}
function dataman_protect(string $indexName): bool {}
function dataman_clear(string $indexName): bool {}
function dataman_save(string $indexName): bool {}
function dataman_restore(string $indexName): bool {}
function dataman_delete(string $indexName): bool {}
function dataman_get_format(): int {}
function dataman_get_key(): string {}
function dataman_key_str(): string {}
function dataman_get_index(): string {}
function dataman_get_file(): string {}
function dataman_start_transaction(): bool {}
function dataman_commit(): bool {}
function dataman_rollback(): bool {}
function dataman_when_file(): bool {}
