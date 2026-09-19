/* Prefix lookup across v2 tree boundaries. GPL-2.0-or-later. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "index_v2.h"

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

static void expect(int fd, const char *query, const char *expected,
		uint64_t expected_record)
{
	char key[6] = { 0 }, matched[6];
	uint16_t file = 0;
	uint64_t record = 0;
	INDEX_V2_CURSOR cursor;

	memcpy(key, query, strlen(query));
	CHECK(index_v2_find(fd, key, false, &file, &record, matched, &cursor));
	CHECK(file == 0 && record == expected_record);
	CHECK(!memcmp(matched, expected, sizeof(matched)));
	CHECK(cursor.generation != 0 && cursor.node_offset != 0);
	CHECK(index_v2_find(fd, matched, true, &file, &record, NULL, NULL));
}

static void exercise(bool build)
{
	char path[] = "/tmp/dataman-index-find-XXXXXX", key[7];
	const char *names[] = { "records" };
	uint64_t root = 0, record;
	INDEX_V2_CURSOR cursor;
	uint16_t file;
	int fd = mkstemp(path), i;

	CHECK(fd >= 0 && unlink(path) == 0);
	if (build)
		CHECK(index_v2_build_begin(fd, 6, 1, names, &root));
	else
		CHECK(index_v2_create_empty(fd, 6, 1, names));
	/* The seventh entry becomes the first key of the right leaf. */
	for (i = 0; i < 13; i++) {
		if (i < 6)
			snprintf(key, sizeof(key), "#%05d", i);
		else if (i == 6)
			strcpy(key, "#99999");
		else
			snprintf(key, sizeof(key), "A%05d", i);
		if (build)
			CHECK(index_v2_build_insert(fd, key, 0, 100 + i, &root));
		else
			CHECK(index_v2_insert(fd, key, 0, 100 + i, &cursor, &root));
	}
	if (build)
		CHECK(index_v2_build_finish(fd, &root));
	expect(fd, "#9999", "#99999", 106);
	expect(fd, "#99999", "#99999", 106);
	expect(fd, "#", "#00000", 100);
	expect(fd, "", "#00000", 100);
	expect(fd, "#9999*", "#99999", 106);
	file = 0; record = 0;
	CHECK(!index_v2_find(fd, "#99998", false, &file, &record, NULL, NULL));
	CHECK(!index_v2_find(fd, "Z00000", false, &file, &record, NULL, NULL));
	CHECK(!index_v2_find(fd, "#99999", true, &file, &record, NULL, NULL));

	/* Exercise leaf and internal-subtree boundaries in a deeper tree. */
	for (i = 0; i < 300; i++) {
		snprintf(key, sizeof(key), "B%05d", i * 10);
		CHECK(index_v2_insert(fd, key, 0, 1000 + i, &cursor, &root));
	}
	for (i = 0; i < 300; i++) {
		char prefix[6];
		snprintf(key, sizeof(key), "B%05d", i * 10);
		memcpy(prefix, key, 5); prefix[5] = '\0';
		expect(fd, prefix, key, 1000 + i);
		expect(fd, key, key, 1000 + i);
	}
	/* Duplicate logical keys must return the first composite entry. */
	CHECK(index_v2_insert(fd, "B00120", 0, 9000, &cursor, &root));
	expect(fd, "B0012", "B00120", 1012);
	CHECK(close(fd) == 0);
}

int main(void)
{
	exercise(false);
	exercise(true);
	return 0;
}
