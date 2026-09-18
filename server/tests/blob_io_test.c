/* Real blob files plus deterministic close/write/namespace failures. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "storage_io.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); exit(1); } } while (0)
static int fail_close, fail_write, reject_namespace, fail_rename, fail_unlink;
static int checked_close(int fd)
{
	int result = close(fd);
	if (fail_close) { errno = EIO; return -1; }
	return result;
}
static int checked_rename(const char *from, const char *to)
{
	if (fail_rename) { errno = EIO; return -1; }
	return rename(from, to);
}
static int checked_unlink(const char *path)
{
	if (fail_unlink) { errno = EIO; return -1; }
	return unlink(path);
}
#define close checked_close
#include "../put_blobs.c"
#undef close
#define rename checked_rename
#define unlink checked_unlink
#include "../blob_ctl.c"
#undef rename
#undef unlink

static int route(int fd, const void *data, size_t size, int64_t offset)
{
	if (fail_write) { errno = ENOSPC; return -1; }
	return dm_storage_write_at(fd, data, size, offset);
}
static int guard(void) { return reject_namespace ? -1 : 0; }
static void expect(const char *path, const char *value)
{
	char bytes[32]; struct stat st;
	int fd = open(path, O_RDONLY); CHECK(fd >= 0);
	CHECK(fstat(fd, &st) == 0 && st.st_size == (off_t)strlen(value));
	CHECK(read(fd, bytes, sizeof(bytes)) == st.st_size);
	CHECK(!memcmp(bytes, value, st.st_size) && close(fd) == 0);
}
int main(void)
{
	char root[] = "/tmp/dataman-blob-XXXXXX";
	char directory[256], source[256], hidden[256], filename[256];
	char data[7]; uint32_t length = htonl(3);
	int16_t sizes[] = {0};
	RFDESC record = {0}; FILEDESC desc = {0}; FILES file = {0};
	CHECK(mkdtemp(root));
	snprintf(directory, sizeof(directory), "%s/blobs", root);
	CHECK(mkdir(directory, 0700) == 0);
	snprintf(filename, sizeof(filename), "%s/files/data", root);
	snprintf(source, sizeof(source), "%s/blobs/data.1.30.0", root);
	snprintf(hidden, sizeof(hidden), "%s/blobs/.data.1.30.0", root);
	record.n_fields = record.has_blob = 1; record.field_sizes = sizes;
	desc.n_rformats = 1; desc.record_desc = &record;
	file._fname = filename; file._filedesc = &desc;
	memcpy(data, &length, 4); memcpy(data + 4, "old", 3);
	dm_storage_set_router(route, guard);
	CHECK(put_blobs(&file, 1, 30, data) == 0); expect(source, "old");
	reject_namespace = 1; memcpy(data + 4, "new", 3);
	CHECK(put_blobs(&file, 1, 30, data) == EBLOBWRT); expect(source, "old");
	CHECK(blob_ctl(root, filename, 1, 30, HIDE) == EBLOBWRT); expect(source, "old");
	reject_namespace = 0;
	CHECK(put_blobs(&file, 1, 30, data) == 0); expect(source, "new");
	fail_close = 1; CHECK(put_blobs(&file, 1, 30, data) == EBLOBWRT); fail_close = 0;
	fail_write = 1; CHECK(put_blobs(&file, 1, 30, data) == EBLOBWRT); fail_write = 0;
	CHECK(put_blobs(&file, 1, 30, data) == 0);
	fail_rename = 1; CHECK(blob_ctl(root, filename, 1, 30, HIDE) == EBLOBWRT); fail_rename = 0;
	expect(source, "new");
	CHECK(blob_ctl(root, filename, 1, 30, HIDE) == 0); expect(hidden, "new");
	CHECK(blob_ctl(root, filename, 1, 30, UNHIDE) == 0); expect(source, "new");
	CHECK(blob_ctl(root, filename, 1, 30, HIDE) == 0);
	fail_unlink = 1; CHECK(blob_ctl(root, filename, 1, 30, CLEANUP) == EBLOBWRT); fail_unlink = 0;
	expect(hidden, "new");
	CHECK(blob_ctl(root, filename, 1, 30, CLEANUP) == 0);
	CHECK(access(hidden, F_OK) == -1 && errno == ENOENT);
	CHECK(put_blobs(&file, 1, 30, data) == 0);
	CHECK(blob_ctl(root, filename, 1, 30, UNLINK) == 0);
	CHECK(rmdir(directory) == 0);
	CHECK(blob_ctl(root, filename, 1, 30, HIDE) == ENOBLOB);
	CHECK(put_blobs(&file, 1, 30, data) == ENOBLOB);
	CHECK(rmdir(root) == 0);
	return 0;
}
