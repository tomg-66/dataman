/* Check on-disk links through production record mutations, without IPC. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "srv_index.h"
#include "misc.h"
#include "storage_io.h"

int idx_cnt = 1;
int dbgsw;
INDEX *_indices;
static int locked;
extern int insert(char *, int, char **);
extern int delete(char *, int, char **);
extern int undelete(char *, int, char **);
extern void put_ll(void *, int64_t);
extern int64_t get_ll(void *);

int fl_lock(P_LOCK *lock, int type)
{
	(void)lock;
	if (type == LOCK_EX) { assert(!locked); locked = 1; }
	else { assert(type == LOCK_UN && locked); locked = 0; }
	return(0);
}

void blob_ctl(char *root, char *name, int fmt, int64_t recno, int mode)
{
	(void)root; (void)name; (void)fmt; (void)recno; (void)mode;
}

int get_blobs(FILES *file, int fmt, int64_t recno, char **data, int *len)
{
	(void)file; (void)fmt; (void)recno; (void)data; (void)len;
	abort();
}

static void check_record(int fd, int64_t offset, int flag, int64_t prev, int64_t next)
{
	char record[DATARECORD_HEADER_LENGTH+4];
	assert(dm_storage_read_at(fd, record, sizeof(record), offset) == 0);
	assert((unsigned char)record[0] == flag);
	assert(get_ll(record+OFFSET_TO_PREV) == prev);
	assert(get_ll(record+OFFSET_TO_NEXT) == next);
	assert(memcmp(record+DATARECORD_HEADER_LENGTH, "    ", 4) == 0);
}

int main(void)
{
	char path[] = "/tmp/dataman-record-XXXXXX";
	char initial[64+DATARECORD_HEADER_LENGTH+4] = {0};
	char cmd[128], ptr[PTR_LENGTH];
	char *data = NULL;
	int fd = mkstemp(path);
	int64_t added = sizeof(initial);
	int16_t formats = 1;
	RFDESC record = {0};
	FILEDESC desc = {0};
	FILES file = {0};
	FILES *files[] = {&file};
	INDEX index = {0};

	assert(fd >= 0);
	assert(unlink(path) == 0);
	record.rf_len = 4;
	desc.n_rformats = 1;
	desc.record_desc = &record;
	file._chan = fd;
	file._filedesc = &desc;
	file._desc = &formats;
	file._hlen = 16;
	index._refcnt = 1;
	index._f_cnt = 1;
	index._files = files;
	_indices = &index;
	put_ll(initial+18, 64);
	initial[64] = 1;
	memset(initial+64+DATARECORD_HEADER_LENGTH, ' ', 4);
	assert(dm_storage_write_at(fd, initial, sizeof(initial), 0) == 0);

	/* Insert before the only record; the file's first pointer must move. */
	strcpy(cmd, "1|0|0|0|64|");
	assert(insert(cmd, 0, &data) > 0 && !locked && !data);
	check_record(fd, added, 1, 0, 64);
	check_record(fd, 64, 1, added, 0);
	assert(dm_storage_read_at(fd, ptr, sizeof(ptr), 18) == 0);
	assert(get_ll(ptr) == added);

	sprintf(cmd, "0|0|%lld|%d|", (long long)added, INCOMMIT);
	assert(delete(cmd, 0, &data) == 0 && !locked && !data);
	check_record(fd, added, 0201, 0, 64);
	check_record(fd, 64, 1, 0, 0);
	assert(dm_storage_read_at(fd, ptr, sizeof(ptr), 18) == 0);
	assert(get_ll(ptr) == 64);

	sprintf(cmd, "0|0|%lld|", (long long)added);
	assert(undelete(cmd, 0, &data) > 0 && !locked && !data);
	assert(strcmp(cmd, "0|") == 0);
	check_record(fd, added, 1, 0, 64);
	check_record(fd, 64, 1, added, 0);
	assert(dm_storage_read_at(fd, ptr, sizeof(ptr), 18) == 0);
	assert(get_ll(ptr) == added);
	/* Insert into the middle to exercise both neighboring pointer writes. */
	sprintf(cmd, "1|1|0|0|%lld|", (long long)added);
	assert(insert(cmd, 0, &data) > 0 && !locked && !data);
	int64_t middle = added + DATARECORD_HEADER_LENGTH + 4;
	check_record(fd, added, 1, 0, middle);
	check_record(fd, middle, 1, added, 64);
	check_record(fd, 64, 1, middle, 0);
	sprintf(cmd, "0|0|%lld|%d|", (long long)middle, INCOMMIT);
	assert(delete(cmd, 0, &data) == 0 && !locked && !data);
	check_record(fd, added, 1, 0, 64);
	check_record(fd, 64, 1, added, 0);
	sprintf(cmd, "0|0|%lld|", (long long)middle);
	assert(undelete(cmd, 0, &data) > 0 && !locked && !data);
	assert(strcmp(cmd, "0|") == 0);
	check_record(fd, added, 1, 0, middle);
	check_record(fd, middle, 1, added, 64);
	check_record(fd, 64, 1, middle, 0);
	assert(close(fd) == 0);
	return(0);
}
