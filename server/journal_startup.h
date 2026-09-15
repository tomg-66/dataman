/* Persistent journal ownership and startup recovery. GPL-2.0-or-later. */
#ifndef DATAMAN_JOURNAL_STARTUP_H
#define DATAMAN_JOURNAL_STARTUP_H

#define DATAMAN_DEFAULT_JOURNAL_DIR "/var/lib/dataman/journal"

typedef struct dm_journal_guard dm_journal_guard;
/*
 * Absolute path; parent directory must exist. Create the leaf with mode 0700.
 * Require service ownership/private permissions; retain the lock until exit.
 * The caller must fork/daemonize BEFORE acquiring the POSIX record lock.
 */
int dm_journal_open(const char *directory, dm_journal_guard **out);
int dm_journal_recover(dm_journal_guard *guard);
const char *dm_journal_directory(const dm_journal_guard *guard);
void dm_journal_close(dm_journal_guard *guard);
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
