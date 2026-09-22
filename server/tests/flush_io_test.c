/* Exercise production flush error propagation without a running server.
 * Licensed under GPL-2.0-or-later. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "srv_index.h"
#include "errors.h"
#include "misc.h"
#include "storage_io.h"

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
	exit(1); } } while (0)

int idx_cnt;
INDEX *_indices;
FILES *_wfiles[MAX_CONNS];
static int locked, writes, blobs, write_result, blob_result;

int fl_lock(P_LOCK *lock, int type)
{
	(void)lock;
	if (type == LOCK_EX) { CHECK(!locked); locked = 1; }
	else { CHECK(type == LOCK_UN && locked); locked = 0; }
	return 0;
}

int dm_storage_mutate_at(int fd, const void *buffer, size_t length, int64_t offset)
{
	CHECK(locked && fd == 42 && length == 4);
	CHECK(offset == 30 + DATARECORD_HEADER_LENGTH);
	CHECK(memcmp(buffer, "data", 4) == 0);
	++writes;
	return write_result;
}

int put_blobs(FILES *file, int fmt, int64_t recno, char *data)
{
	CHECK(locked && file == _wfiles[0] && fmt == 1 && recno == 30);
	CHECK(memcmp(data, "data", 4) == 0);
	++blobs;
	return blob_result;
}

extern int flush(char *, int, char **);

static void run(const char *request, int expected, int expected_writes, int expected_blobs)
{
	char cmd[128];
	char *data = strdup("data");
	CHECK(data != NULL);
	strcpy(cmd, request);
	writes = blobs = 0;
	CHECK(flush(cmd, 0, &data) == expected);
	CHECK(data == NULL && !locked);
	CHECK(writes == expected_writes && blobs == expected_blobs);
	if (expected == 4) CHECK(strcmp(cmd, "0|1|") == 0);
}

int main(void)
{
	RFDESC record = {0};
	FILEDESC desc = {0};
	FILES file = {0};
	record.rf_len = 4;
	record.has_blob = 1;
	desc.n_rformats = 1;
	desc.record_desc = &record;
	file._filedesc = &desc;
	file._chan = 42;
	_wfiles[0] = &file;

	write_result = -1;
	run("-1|0|30|1|", ERECWRT, 1, 0);
	write_result = 0;
	blob_result = EBLOBWRT;
	run("-1|0|30|1|", EBLOBWRT, 1, 1);
	blob_result = 0;
	run("-1|0|30|1|", 4, 1, 1);
	record.has_blob = 0;
	run("-1|0|30|1|", 4, 1, 0);
	run("-1|0|-1|1|", ERECWRT, 0, 0);
	run("-1|0|9223372036854775807|1|", ERECWRT, 0, 0);
	return 0;
}
