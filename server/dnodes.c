/* Display the physical page and tree structure of a v2 index. */
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "index_v2.h"

#define REACH_SLOT0 0x01
#define REACH_SLOT1 0x02
#define REACH_BUILD 0x04
#define INDEX_V2_MIN_KEYS (INDEX_V2_MAX_KEYS / 2)

typedef struct root_info {
	int valid;
	uint64_t generation;
	uint64_t offset;
	uint32_t root_crc;
} ROOT_INFO;

typedef struct dump_context {
	int fd;
	uint16_t keylen;
	uint16_t file_count;
	uint16_t page_size;
	uint64_t node_start;
	uint64_t file_size;
	size_t page_count;
	unsigned char *reachable;
	ROOT_INFO roots[INDEX_V2_ROOT_SLOTS];
	uint64_t build_root;
	int building;
	int errors;
} DUMP_CONTEXT;

static uint16_t get_u16(const unsigned char *ptr)
{
	return((uint16_t)((ptr[0] << 8) | ptr[1]));
}

static uint32_t get_u32(const unsigned char *ptr)
{
	return(((uint32_t)ptr[0] << 24) | ((uint32_t)ptr[1] << 16) |
		((uint32_t)ptr[2] << 8) | ptr[3]);
}

static uint64_t get_u64(const unsigned char *ptr)
{
	uint64_t value = 0;
	int i;

	for (i = 0; i < 8; i++)
		value = (value << 8) | ptr[i];
	return(value);
}

static void print_key(const unsigned char *key, uint16_t keylen)
{
	uint16_t i;

	putchar('"');
	for (i = 0; i < keylen && key[i] != '\0'; i++) {
		if (isprint(key[i]) && key[i] != '"' && key[i] != '\\')
			putchar(key[i]);
		else
			printf("\\x%02x", key[i]);
	}
	putchar('"');
}

static int page_number(DUMP_CONTEXT *context, uint64_t offset, size_t *page)
{
	if (offset < context->node_start || offset >= context->file_size ||
			(offset - context->node_start) % context->page_size != 0)
		return(0);
	*page = (size_t)((offset - context->node_start) / context->page_size);
	return(*page < context->page_count);
}

static int mark_tree(DUMP_CONTEXT *context, uint64_t offset, unsigned char bit)
{
	INDEX_V2_NODE node;
	size_t page;
	uint32_t expected;
	uint8_t i;

	if (!page_number(context, offset, &page)) {
		fprintf(stderr, "Invalid child page offset %" PRIu64 "\n", offset);
		context->errors++;
		return(0);
	}
	if (context->reachable[page] & bit) {
		fprintf(stderr, "Cycle or duplicate reference to page %" PRIu64 "\n",
			offset);
		context->errors++;
		return(0);
	}
	context->reachable[page] |= bit;
	if (!index_v2_read_node(context->fd, offset, &node)) {
		fprintf(stderr, "Invalid node or CRC at page %" PRIu64 "\n", offset);
		context->errors++;
		return(0);
	}
	expected = (node.flags & INDEX_V2_NODE_LEAF) ?
		(uint32_t)node.key_count * (context->keylen + 10) :
		(uint32_t)(node.key_count + 1) * 8 +
		(uint32_t)node.key_count * (context->keylen + 10);
	if (node.key_count > INDEX_V2_MAX_KEYS || node.payload_length != expected ||
			(!(node.flags & INDEX_V2_NODE_LEAF) && node.key_count == 0)) {
		fprintf(stderr, "Malformed node at page %" PRIu64 "\n", offset);
		context->errors++;
		index_v2_free_node(&node);
		return(0);
	}
	if (!(node.flags & INDEX_V2_NODE_LEAF))
		for (i = 0; i <= node.key_count; i++)
			mark_tree(context, get_u64(node.payload + (size_t)i * 8), bit);
	index_v2_free_node(&node);
	return(1);
}

static int read_root_slot(const unsigned char *slot, ROOT_INFO *root)
{
	uint32_t slot_crc = get_u32(slot + INDEX_V2_SLOT_CRC_OFFSET);

	memset(root, 0, sizeof(*root));
	if (slot_crc == 0 || slot_crc !=
			index_v2_crc32(slot, INDEX_V2_SLOT_CRC_OFFSET))
		return(0);
	root->generation = get_u64(slot + INDEX_V2_SLOT_GENERATION_OFFSET);
	root->offset = get_u64(slot + INDEX_V2_SLOT_ROOT_OFFSET);
	root->root_crc = get_u32(slot + INDEX_V2_SLOT_ROOT_CRC_OFFSET);
	root->valid = root->generation != 0 && root->offset >= INDEX_V2_HEADER_SIZE;
	return(root->valid);
}

static void print_entry(DUMP_CONTEXT *context, const unsigned char *entry)
{
	print_key(entry, context->keylen);
	printf(" file=%u record=%" PRIu64, get_u16(entry + context->keylen),
		get_u64(entry + context->keylen + INDEX_V2_FILE_ID_SIZE));
}

static void print_page(DUMP_CONTEXT *context, size_t page)
{
	INDEX_V2_NODE node;
	uint64_t offset = context->node_start + (uint64_t)page * context->page_size;
	uint32_t child_bytes;
	uint8_t count, i;
	unsigned char marks = context->reachable[page];
	const unsigned char *entries;
	int root_page = 0;

	printf("\npage %zu offset=%" PRIu64 " reach=", page, offset);
	if (marks == 0)
		printf("free/unreachable");
	else {
		if (marks & REACH_SLOT0) printf("slot0 ");
		if (marks & REACH_SLOT1) printf("slot1 ");
		if (marks & REACH_BUILD) printf("build ");
	}
	putchar('\n');
	if (!index_v2_read_node(context->fd, offset, &node)) {
		printf("  INVALID NODE OR CRC\n");
		context->errors++;
		return;
	}
	count = node.key_count;
	printf("  type=%s keys=%u payload=%u crc=%08" PRIx32 "\n",
		(node.flags & INDEX_V2_NODE_LEAF) ? "leaf" : "internal", count,
		node.payload_length, node.crc);
	if ((context->roots[0].valid && context->roots[0].offset == offset) ||
			(context->roots[1].valid && context->roots[1].offset == offset) ||
			(context->building && context->build_root == offset))
		root_page = 1;
	if (!root_page && marks != 0 && count < INDEX_V2_MIN_KEYS)
		printf("  WARNING: non-root occupancy below %u\n", INDEX_V2_MIN_KEYS);
	if (count > INDEX_V2_MAX_KEYS) {
		printf("  MALFORMED: too many keys\n");
		context->errors++;
		index_v2_free_node(&node);
		return;
	}
	child_bytes = (node.flags & INDEX_V2_NODE_LEAF) ? 0 :
		(uint32_t)(count + 1) * 8;
	if (node.payload_length != child_bytes + (uint32_t)count *
			(context->keylen + 10)) {
		printf("  MALFORMED: invalid payload length\n");
		context->errors++;
		index_v2_free_node(&node);
		return;
	}
	entries = node.payload + child_bytes;
	if (!(node.flags & INDEX_V2_NODE_LEAF))
		for (i = 0; i <= count; i++)
			printf("  child[%u]=%" PRIu64 "\n", i,
				get_u64(node.payload + (size_t)i * 8));
	for (i = 0; i < count; i++) {
		printf("  %s[%u]=", (node.flags & INDEX_V2_NODE_LEAF) ? "entry" :
			"separator", i);
		print_entry(context, entries + (size_t)i * (context->keylen + 10));
		putchar('\n');
	}
	index_v2_free_node(&node);
}

static void usage(void)
{
	fprintf(stderr, "Usage: dnodes [-r database_root] index_name\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	DUMP_CONTEXT context;
	unsigned char header[INDEX_V2_HEADER_SIZE];
	const char *root;
	const char *name;
	char path[1024];
	struct stat status;
	uint32_t names_length;
	uint16_t flags;
	size_t i;

	memset(&context, 0, sizeof(context));
	if (argc == 4 && strcmp(argv[1], "-r") == 0) {
		root = argv[2];
		name = argv[3];
	} else if (argc == 2) {
		root = getenv("ROOT");
		name = argv[1];
	} else
		usage();
	if (root != NULL)
		snprintf(path, sizeof(path), "%s/index/%s", root, name);
	else
		snprintf(path, sizeof(path), "%s", name);
	context.fd = open(path, O_RDONLY);
	if (context.fd < 0) {
		fprintf(stderr, "Cannot open %s: %s\n", path, strerror(errno));
		return(EXIT_FAILURE);
	}
	if (pread(context.fd, header, sizeof(header), 0) != (ssize_t)sizeof(header) ||
			memcmp(header + INDEX_V2_MAGIC_OFFSET, INDEX_V2_MAGIC,
				strlen(INDEX_V2_MAGIC)) != 0 ||
			get_u16(header + INDEX_V2_VERSION_OFFSET) != INDEX_V2_VERSION ||
			get_u16(header + INDEX_V2_HEADER_SIZE_OFFSET) != INDEX_V2_HEADER_SIZE ||
			fstat(context.fd, &status) != 0 || status.st_size < 0) {
		fprintf(stderr, "%s is not a valid v2 index\n", path);
		close(context.fd);
		return(EXIT_FAILURE);
	}
	context.keylen = get_u16(header + INDEX_V2_KEYLEN_OFFSET);
	context.file_count = get_u16(header + INDEX_V2_FILE_COUNT_OFFSET);
	names_length = get_u32(header + INDEX_V2_NAMES_LENGTH_OFFSET);
	context.page_size = get_u16(header + INDEX_V2_PAGE_SIZE_OFFSET);
	flags = get_u16(header + INDEX_V2_FLAGS_OFFSET);
	context.building = (flags & INDEX_V2_FLAG_BUILDING) != 0;
	context.build_root = get_u64(header + INDEX_V2_BUILD_ROOT_OFFSET);
	if (context.keylen == 0 || context.keylen > 32 || context.file_count == 0 ||
			context.page_size < index_v2_page_size(context.keylen) ||
			(context.page_size & 077) != 0) {
		fprintf(stderr, "Invalid v2 header values\n");
		close(context.fd);
		return(EXIT_FAILURE);
	}
	context.node_start = ((INDEX_V2_HEADER_SIZE + (uint64_t)names_length +
		context.page_size - 1) / context.page_size) * context.page_size;
	context.file_size = (uint64_t)status.st_size;
	if (context.file_size < context.node_start ||
			(context.file_size - context.node_start) % context.page_size != 0) {
		fprintf(stderr, "Index page area is misaligned\n");
		close(context.fd);
		return(EXIT_FAILURE);
	}
	context.page_count = (size_t)((context.file_size - context.node_start) /
		context.page_size);
	context.reachable = calloc(context.page_count, 1);
	if (context.reachable == NULL && context.page_count != 0) {
		fprintf(stderr, "Cannot allocate page map\n");
		close(context.fd);
		return(EXIT_FAILURE);
	}
	read_root_slot(header + INDEX_V2_SLOT0_OFFSET, &context.roots[0]);
	read_root_slot(header + INDEX_V2_SLOT1_OFFSET, &context.roots[1]);
	printf("Index: %s\nkey length=%u files=%u page size=%u pages=%zu\n",
		path, context.keylen, context.file_count, context.page_size,
		context.page_count);
	for (i = 0; i < INDEX_V2_ROOT_SLOTS; i++) {
		ROOT_INFO *slot = &context.roots[i];
		if (slot->valid) {
			INDEX_V2_NODE root_node;
			int crc_ok = index_v2_read_node(context.fd, slot->offset, &root_node) &&
				root_node.crc == slot->root_crc;
			printf("root slot %zu: generation=%" PRIu64 " root=%" PRIu64
				" root-crc=%08" PRIx32 " %s\n", i, slot->generation,
				slot->offset, slot->root_crc, crc_ok ? "valid" : "INVALID");
			index_v2_free_node(&root_node);
			if (!crc_ok)
				context.errors++;
			mark_tree(&context, slot->offset,
				i == 0 ? REACH_SLOT0 : REACH_SLOT1);
		} else
			printf("root slot %zu: invalid/empty\n", i);
	}
	if (context.building) {
		printf("BUILD IN PROGRESS: working root=%" PRIu64 "\n",
			context.build_root);
		mark_tree(&context, context.build_root, REACH_BUILD);
	} else if (flags != 0) {
		printf("WARNING: unknown header flags 0x%04x\n", flags);
		context.errors++;
	}
	for (i = 0; i < context.page_count; i++)
		print_page(&context, i);
	printf("\nSummary: pages=%zu errors=%d\n", context.page_count,
		context.errors);
	free(context.reachable);
	close(context.fd);
	return(context.errors == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* vim: set noet sw=4 sts=4 ts=4 fdm=marker: */
