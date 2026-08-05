/* ***************************************************************
 *
 * PROCEDURE:	index_v2.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 *				Mon Jul 20 04:11:25 PM MDT 2026
 *				tomg
 *				implementing new index V2 copy on write functionality,
 *				search, insert, and removal routines
 *
 ************************************************************* */

/*
 * 4.0.0 dataman file edit procedure header
 * Copyright (c) SuperUser Software 1988-2026.  All rights reserved.
 *
 *  Copy-on-write index v2 header and root-publication primitives.
 *
 *  as the storage capacity, price, and speed of storage has changed
 *  drastically in the last several years, many of the space optomizations
 *  and concurrency issues of the past has become much less important.
 *  this facilitates much more robust disk safety at the expense of
 *  disk space.  Indexes grow much faster as you make many insertions
 *  and deletions.  it has been somewhat mitigated by trying to re-use
 *  free space as it's possible.  It is still very much necessary to
 *  use the clean utility to optomize disk space as things grow though.
 *
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "index_v2.h"

static void put_u16(unsigned char *buf, uint16_t value)
{
	buf[0] = (unsigned char)(value >> 8);
	buf[1] = (unsigned char)value;
}

static void put_u32(unsigned char *buf, uint32_t value)
{
	buf[0] = (unsigned char)(value >> 24);
	buf[1] = (unsigned char)(value >> 16);
	buf[2] = (unsigned char)(value >> 8);
	buf[3] = (unsigned char)value;
}

static void put_u64(unsigned char *buf, uint64_t value)
{
	int i;

	for (i = 7; i >= 0; i--) {
		buf[i] = (unsigned char)value;
		value >>= 8;
	}
}

static uint16_t get_u16(const unsigned char *buf)
{
	return((uint16_t)((buf[0] << 8) | buf[1]));
}

static uint32_t get_u32(const unsigned char *buf)
{
	return(((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
			((uint32_t)buf[2] << 8) | buf[3]);
}

static uint64_t get_u64(const unsigned char *buf)
{
	uint64_t value = 0;
	int i;

	for (i = 0; i < 8; i++)
		value = (value << 8) | buf[i];
	return(value);
}

static bool read_full_at(int fd, void *buf, size_t len, off_t offset)
{
	unsigned char *ptr = buf;
	ssize_t ret;

	while (len) {
		ret = pread(fd, ptr, len, offset);
		if (ret < 0 && errno == EINTR)
			continue;
		if (ret <= 0)
			return(false);
		ptr += ret;
		offset += ret;
		len -= (size_t)ret;
	}
	return(true);
}

static bool write_full_at(int fd, const void *buf, size_t len, off_t offset)
{
	const unsigned char *ptr = buf;
	ssize_t ret;

	while (len) {
		ret = pwrite(fd, ptr, len, offset);
		if (ret < 0 && errno == EINTR)
			continue;
		if (ret <= 0)
			return(false);
		ptr += ret;
		offset += ret;
		len -= (size_t)ret;
	}
	return(true);
}

static bool write_header(int fd, uint16_t keylen, uint16_t file_count,
					 uint32_t names_length, uint16_t page_size, uint16_t flags)
{
	unsigned char header[INDEX_V2_HEADER_SIZE];

	memset(header, 0, sizeof(header));
	memcpy(header + INDEX_V2_MAGIC_OFFSET, INDEX_V2_MAGIC, strlen(INDEX_V2_MAGIC));
	put_u16(header + INDEX_V2_VERSION_OFFSET, INDEX_V2_VERSION);
	put_u16(header + INDEX_V2_HEADER_SIZE_OFFSET, INDEX_V2_HEADER_SIZE);
	put_u16(header + INDEX_V2_KEYLEN_OFFSET, keylen);
	put_u16(header + INDEX_V2_FILE_COUNT_OFFSET, file_count);
	put_u32(header + INDEX_V2_NAMES_LENGTH_OFFSET, names_length);
	put_u16(header + INDEX_V2_PAGE_SIZE_OFFSET, page_size);
	put_u16(header + INDEX_V2_FLAGS_OFFSET, flags);

	return(write_full_at(fd, header, sizeof(header), 0));
}

uint32_t index_v2_crc32(const void *buf, uint32_t len)
{
	const unsigned char *ptr = buf;
	uint32_t crc = 0xffffffffU;
	uint32_t bit;
	int i;

	while (len--) {
		crc ^= *ptr++;
		for (i = 0; i < 8; i++) {
			bit = crc & 1U;
			crc >>= 1;
			if (bit)
				crc ^= 0xedb88320U;
		}
	}
	return(~crc);
}

uint16_t index_v2_page_size(uint16_t keylen)
{
	uint32_t size = INDEX_V2_NODE_HEADER_SIZE + (INDEX_V2_MAX_KEYS + 1) * 8 +
		INDEX_V2_MAX_KEYS * (keylen + INDEX_V2_FILE_ID_SIZE +
		INDEX_V2_RECORD_OFFSET_SIZE);

	return((uint16_t)((size + 63) & ~63U));
}

bool index_v2_init_header(int fd, uint16_t keylen, uint16_t file_count)
{
	return(write_header(fd, keylen, file_count, 0, index_v2_page_size(keylen), 0));
}

static bool v2_create_empty(int fd, uint16_t keylen, uint16_t file_count,
		const char *const *file_names, bool building, uint64_t *root_offset)
{
	unsigned char node[INDEX_V2_NODE_HEADER_SIZE];
	uint32_t names_length = 0;
	uint32_t crc;
	uint16_t i;
	off_t offset;
	unsigned char *page;
	uint16_t page_size;

	if (keylen == 0 || keylen > 32 || file_count == 0 || file_names == NULL)
		return(false);
	for (i = 0; i < file_count; i++) {
		if (file_names[i] == NULL || strlen(file_names[i]) + 1 >
				UINT32_MAX - names_length)
			return(false);
		names_length += (uint32_t)strlen(file_names[i]) + 1;
	}
	page_size = index_v2_page_size(keylen);
	if (!write_header(fd, keylen, file_count, names_length, page_size,
			building ? INDEX_V2_FLAG_BUILDING : 0))
		return(false);
	offset = INDEX_V2_HEADER_SIZE;
	for (i = 0; i < file_count; i++) {
		size_t length = strlen(file_names[i]) + 1;
		if (!write_full_at(fd, file_names[i], length, offset))
			return(false);
		offset += (off_t)length;
	}
	offset = ((offset + page_size - 1) / page_size) * page_size;
	page = calloc(1, page_size);
	if (page == NULL)
		return(false);
	memset(node, 0, sizeof(node));
	memcpy(node, INDEX_V2_NODE_MAGIC, 4);
	node[INDEX_V2_NODE_FLAGS_OFFSET] = INDEX_V2_NODE_LEAF;
	crc = index_v2_crc32(node, INDEX_V2_NODE_CRC_OFFSET);
	put_u32(node + INDEX_V2_NODE_CRC_OFFSET, crc);
	memcpy(page, node, sizeof(node));
	if (!write_full_at(fd, page, page_size, offset) || fdatasync(fd) != 0) {
		free(page);
		return(false);
	}
	free(page);
	if (building) {
		unsigned char encoded[8];

		put_u64(encoded, (uint64_t)offset);
		if (!write_full_at(fd, encoded, sizeof(encoded), INDEX_V2_BUILD_ROOT_OFFSET) ||
				fdatasync(fd) != 0)
			return(false);
		if (root_offset != NULL)
			*root_offset = (uint64_t)offset;
		return(true);
	}
	return(index_v2_publish_root(fd, (uint64_t)offset, crc, 1));
}

bool index_v2_create_empty(int fd, uint16_t keylen, uint16_t file_count,
					const char *const *file_names)
{
	return(v2_create_empty(fd, keylen, file_count, file_names, false, NULL));
}

bool index_v2_build_begin(int fd, uint16_t keylen, uint16_t file_count,
					const char *const *file_names, uint64_t *root_offset)
{
	if (root_offset == NULL)
		return(false);
	return(v2_create_empty(fd, keylen, file_count, file_names, true,
		root_offset));
}

static bool read_slot(const unsigned char *slot, uint64_t *root_offset,
					uint32_t *root_crc, uint64_t *generation)
{
	uint32_t stored_crc;

	stored_crc = get_u32(slot + INDEX_V2_SLOT_CRC_OFFSET);

	if (stored_crc == 0 || stored_crc != index_v2_crc32(slot, INDEX_V2_SLOT_CRC_OFFSET))
		return(false);

	*generation = get_u64(slot + INDEX_V2_SLOT_GENERATION_OFFSET);
	*root_offset = get_u64(slot + INDEX_V2_SLOT_ROOT_OFFSET);
	*root_crc = get_u32(slot + INDEX_V2_SLOT_ROOT_CRC_OFFSET);

	return(*generation != 0 && *root_offset >= INDEX_V2_HEADER_SIZE);
}

bool index_v2_read_header(int fd, uint16_t *keylen, uint16_t *file_count,
						uint64_t *root_offset, uint32_t *root_crc,
						uint64_t *generation)
{
	unsigned char header[INDEX_V2_HEADER_SIZE];
	uint64_t root[INDEX_V2_ROOT_SLOTS];
	uint64_t gen[INDEX_V2_ROOT_SLOTS];
	uint32_t crc[INDEX_V2_ROOT_SLOTS];
	bool valid[INDEX_V2_ROOT_SLOTS];

	if (!read_full_at(fd, header, sizeof(header), 0) ||
			memcmp(header + INDEX_V2_MAGIC_OFFSET, INDEX_V2_MAGIC,
				strlen(INDEX_V2_MAGIC)) != 0 ||
			get_u16(header + INDEX_V2_VERSION_OFFSET) != INDEX_V2_VERSION ||
			get_u16(header + INDEX_V2_HEADER_SIZE_OFFSET) != INDEX_V2_HEADER_SIZE ||
			get_u16(header + INDEX_V2_FLAGS_OFFSET) != 0)
		return(false);

	valid[0] = read_slot(header + INDEX_V2_SLOT0_OFFSET, &root[0], &crc[0], &gen[0]);
	valid[1] = read_slot(header + INDEX_V2_SLOT1_OFFSET, &root[1], &crc[1], &gen[1]);

	if (!valid[0] && !valid[1])
		return(false);

	if (!valid[1] || (valid[0] && gen[0] > gen[1])) {
		*root_offset = root[0];
		*root_crc = crc[0];
		*generation = gen[0];
	} else {
		*root_offset = root[1];
		*root_crc = crc[1];
		*generation = gen[1];
	}

	*keylen = get_u16(header + INDEX_V2_KEYLEN_OFFSET);
	*file_count = get_u16(header + INDEX_V2_FILE_COUNT_OFFSET);
	return(*keylen > 0 && *file_count > 0);
}

static bool v2_read_build_header(int fd, uint16_t *keylen,
		uint16_t *file_count, uint64_t *root_offset)
{
	unsigned char header[INDEX_V2_HEADER_SIZE];

	if (!read_full_at(fd, header, sizeof(header), 0) ||
			memcmp(header + INDEX_V2_MAGIC_OFFSET, INDEX_V2_MAGIC,
				strlen(INDEX_V2_MAGIC)) != 0 ||
			get_u16(header + INDEX_V2_VERSION_OFFSET) != INDEX_V2_VERSION ||
			get_u16(header + INDEX_V2_HEADER_SIZE_OFFSET) != INDEX_V2_HEADER_SIZE ||
			get_u16(header + INDEX_V2_FLAGS_OFFSET) != INDEX_V2_FLAG_BUILDING)
		return(false);
	*keylen = get_u16(header + INDEX_V2_KEYLEN_OFFSET);
	*file_count = get_u16(header + INDEX_V2_FILE_COUNT_OFFSET);
	*root_offset = get_u64(header + INDEX_V2_BUILD_ROOT_OFFSET);
	return(*keylen > 0 && *keylen <= 32 && *file_count > 0 &&
		*root_offset >= INDEX_V2_HEADER_SIZE);
}

bool index_v2_read_file_names(int fd, uint16_t file_count, char ***file_names)
{
	unsigned char header[INDEX_V2_HEADER_SIZE];
	unsigned char *names;
	char **result;
	uint32_t names_length;
	uint32_t position;
	uint16_t i;

	if (file_names == NULL || !read_full_at(fd, header, sizeof(header), 0))
		return(false);
	names_length = get_u32(header + INDEX_V2_NAMES_LENGTH_OFFSET);
	if (names_length < file_count || names_length > 1024U * 1024U)
		return(false);
	names = malloc(names_length);
	result = calloc(file_count, sizeof(*result));
	if (names == NULL || result == NULL) {
		free(names);
		free(result);
		return(false);
	}
	if (!read_full_at(fd, names, names_length, INDEX_V2_HEADER_SIZE)) {
		free(names);
		free(result);
		return(false);
	}
	position = 0;
	for (i = 0; i < file_count; i++) {
		uint32_t start = position;
		while (position < names_length && names[position] != '\0')
			position++;
		if (position == names_length) {
			while (i--)
				free(result[i]);
			free(names);
			free(result);
			return(false);
		}
		result[i] = strdup((char *)names + start);
		if (result[i] == NULL) {
			while (i--)
				free(result[i]);
			free(names);
			free(result);
			return(false);
		}
		position++;
	}
	free(names);
	if (position != names_length) {
		for (i = 0; i < file_count; i++)
			free(result[i]);
		free(result);
		return(false);
	}
	*file_names = result;
	return(true);
}

bool index_v2_read_page_size(int fd, uint16_t keylen, uint16_t *page_size)
{
	unsigned char header[INDEX_V2_HEADER_SIZE];
	uint16_t stored;

	if (!read_full_at(fd, header, sizeof(header), 0))
		return(false);
	stored = get_u16(header + INDEX_V2_PAGE_SIZE_OFFSET);
	if (stored == 0)
		stored = INDEX_V2_LEGACY_PAGE_SIZE;
	if (stored < index_v2_page_size(keylen) || (stored & 077) != 0)
		return(false);
	*page_size = stored;
	return(true);
}

bool index_v2_publish_root(int fd, uint64_t root_offset, uint32_t root_crc,
						uint64_t generation)
{
	unsigned char slot[INDEX_V2_ROOT_SLOT_SIZE];
	off_t offset;

	if (root_offset < INDEX_V2_HEADER_SIZE || generation == 0)
		return(false);
	memset(slot, 0, sizeof(slot));
	put_u64(slot + INDEX_V2_SLOT_GENERATION_OFFSET, generation);
	put_u64(slot + INDEX_V2_SLOT_ROOT_OFFSET, root_offset);
	put_u32(slot + INDEX_V2_SLOT_ROOT_CRC_OFFSET, root_crc);
	put_u32(slot + INDEX_V2_SLOT_CRC_OFFSET,
			index_v2_crc32(slot, INDEX_V2_SLOT_CRC_OFFSET));
	offset = (generation & 1U) ? INDEX_V2_SLOT0_OFFSET : INDEX_V2_SLOT1_OFFSET;
	return(write_full_at(fd, slot, sizeof(slot), offset) && fdatasync(fd) == 0);
}

bool index_v2_read_node(int fd, uint64_t offset, INDEX_V2_NODE *node)
{
	unsigned char header[INDEX_V2_NODE_HEADER_SIZE];
	unsigned char *crc_buf;
	uint32_t crc;

	if (node == NULL)
		return(false);

	memset(node, 0, sizeof(*node));

	if (offset > (uint64_t)INT64_MAX || !read_full_at(fd, header, sizeof(header), (off_t)offset) ||
			memcmp(header + INDEX_V2_NODE_MAGIC_OFFSET, INDEX_V2_NODE_MAGIC, 4) != 0)
		return(false);

	node->flags = header[INDEX_V2_NODE_FLAGS_OFFSET];
	node->key_count = header[INDEX_V2_NODE_KEY_COUNT_OFFSET];
	node->payload_length = get_u32(header + INDEX_V2_NODE_PAYLOAD_OFFSET);
	node->crc = get_u32(header + INDEX_V2_NODE_CRC_OFFSET);
	node->payload = NULL;

	if ((node->flags & ~INDEX_V2_NODE_LEAF) != 0 ||
			node->payload_length > 1024U * 1024U)
		return(false);
	crc_buf = malloc(INDEX_V2_NODE_CRC_OFFSET + node->payload_length);
	if (crc_buf == NULL)
		return(false);
	memcpy(crc_buf, header, INDEX_V2_NODE_CRC_OFFSET);
	if (node->payload_length != 0) {
		node->payload = malloc(node->payload_length);
		if (node->payload == NULL || !read_full_at(fd, node->payload,
				node->payload_length, (off_t)offset + INDEX_V2_NODE_HEADER_SIZE)) {
			free(crc_buf);
			free(node->payload);
			node->payload = NULL;
			return(false);
		}
		memcpy(crc_buf + INDEX_V2_NODE_CRC_OFFSET, node->payload,
				node->payload_length);
	}
	crc = index_v2_crc32(crc_buf, INDEX_V2_NODE_CRC_OFFSET +
			node->payload_length);
	free(crc_buf);
	if (crc != node->crc) {
		free(node->payload);
		node->payload = NULL;
		return(false);
	}
	return(true);
}

void index_v2_free_node(INDEX_V2_NODE *node)
{
	if (node != NULL) {
		free(node->payload);
		memset(node, 0, sizeof(*node));
	}
}

typedef struct v2_insert_context {
	int fd;
	uint16_t keylen;
	uint16_t file_count;
	size_t entry_size;
	uint16_t page_size;
	uint64_t next_offset;
	uint64_t *free_pages;
	size_t free_count;
	bool in_place;
} V2_INSERT_CONTEXT;

typedef struct v2_insert_result {
	uint64_t offset;
	uint32_t crc;
	bool split;
	uint64_t right_offset;
	unsigned char *separator;
	uint64_t leaf_offset;
	uint8_t leaf_index;
} V2_INSERT_RESULT;

static bool v2_write_node(V2_INSERT_CONTEXT *context, bool leaf,
		uint8_t key_count, const unsigned char *entries, const uint64_t *children,
		uint64_t *offset, uint32_t *crc)
{
	uint32_t payload_length;
	unsigned char *buffer;
	unsigned char *crc_buffer;
	size_t total;
	uint8_t i;

	if (key_count > INDEX_V2_MAX_KEYS)
		return(false);
	payload_length = leaf ? (uint32_t)key_count * context->entry_size :
		(uint32_t)(key_count + 1) * 8 + (uint32_t)key_count * context->entry_size;
	total = INDEX_V2_NODE_HEADER_SIZE + payload_length;
	if (total > context->page_size)
		return(false);
	buffer = calloc(1, context->page_size);
	if (buffer == NULL)
		return(false);
	memcpy(buffer + INDEX_V2_NODE_MAGIC_OFFSET, INDEX_V2_NODE_MAGIC, 4);
	buffer[INDEX_V2_NODE_FLAGS_OFFSET] = leaf ? INDEX_V2_NODE_LEAF : 0;
	buffer[INDEX_V2_NODE_KEY_COUNT_OFFSET] = key_count;
	put_u32(buffer + INDEX_V2_NODE_PAYLOAD_OFFSET, payload_length);
	if (leaf) {
		if (payload_length)
			memcpy(buffer + INDEX_V2_NODE_HEADER_SIZE, entries, payload_length);
	} else {
		for (i = 0; i <= key_count; i++)
			put_u64(buffer + INDEX_V2_NODE_HEADER_SIZE + (size_t)i * 8, children[i]);
		if (key_count)
			memcpy(buffer + INDEX_V2_NODE_HEADER_SIZE + (size_t)(key_count + 1) * 8,
				entries, (size_t)key_count * context->entry_size);
	}
	crc_buffer = malloc(INDEX_V2_NODE_CRC_OFFSET + payload_length);
	if (crc_buffer == NULL) {
		free(buffer);
		return(false);
	}
	memcpy(crc_buffer, buffer, INDEX_V2_NODE_CRC_OFFSET);
	if (payload_length)
		memcpy(crc_buffer + INDEX_V2_NODE_CRC_OFFSET,
			buffer + INDEX_V2_NODE_HEADER_SIZE, payload_length);
	*crc = index_v2_crc32(crc_buffer, INDEX_V2_NODE_CRC_OFFSET + payload_length);
	free(crc_buffer);
	put_u32(buffer + INDEX_V2_NODE_CRC_OFFSET, *crc);
	if (context->in_place && *offset != 0) {
		/* Rebuild pages are private and may be safely rewritten in place. */
	} else if (context->free_count)
		*offset = context->free_pages[--context->free_count];
	else {
		*offset = context->next_offset;
		context->next_offset += context->page_size;
	}
	if (!write_full_at(context->fd, buffer, context->page_size, (off_t)*offset)) {
		free(buffer);
		return(false);
	}
	free(buffer);
	return(true);
}

static bool v2_load_node(V2_INSERT_CONTEXT *context, uint64_t offset,
		bool *leaf, uint8_t *key_count, unsigned char **entries,
		uint64_t **children)
{
	INDEX_V2_NODE disk;
	uint32_t expected;
	uint32_t child_bytes;
	uint8_t i;

	*entries = NULL;
	*children = NULL;

	if (!index_v2_read_node(context->fd, offset, &disk) || disk.key_count > INDEX_V2_MAX_KEYS)
		return(false);

	*leaf = (disk.flags & INDEX_V2_NODE_LEAF) != 0;
	*key_count = disk.key_count;
	expected = *leaf ? (uint32_t)*key_count * context->entry_size : (uint32_t)(*key_count + 1) * 8 + (uint32_t)*key_count * context->entry_size;

	if ((!*leaf && *key_count == 0) || disk.payload_length != expected) {
		index_v2_free_node(&disk);
		return(false);
	}

	if (*key_count) {
		*entries = malloc((size_t)*key_count * context->entry_size);
		if (*entries == NULL) {
			index_v2_free_node(&disk);
			return(false);
		}
	}
	if (*leaf) {
		if (*key_count)
			memcpy(*entries, disk.payload, (size_t)*key_count * context->entry_size);
	} else {
		child_bytes = (uint32_t)(*key_count + 1) * 8;
		*children = malloc((size_t)(*key_count + 1) * sizeof(**children));
		if (*children == NULL) {
			free(*entries);
			*entries = NULL;
			index_v2_free_node(&disk);
			return(false);
		}
		for (i = 0; i <= *key_count; i++)
			(*children)[i] = get_u64(disk.payload + (size_t)i * 8);
		if (*key_count)
			memcpy(*entries, disk.payload + child_bytes,
				(size_t)*key_count * context->entry_size);
	}
	index_v2_free_node(&disk);
	return(true);
}

static bool v2_insert_node(V2_INSERT_CONTEXT *context, uint64_t offset,
		const unsigned char *entry, V2_INSERT_RESULT *result)
{
	bool leaf;
	uint8_t count;
	unsigned char *entries;
	uint64_t *children;
	unsigned char *new_entries;
	uint64_t *new_children;
	V2_INSERT_RESULT child;
	uint8_t position;
	uint8_t new_count;
	uint8_t i;

	memset(result, 0, sizeof(*result));
	if (context->in_place)
		result->offset = offset;
	if (!v2_load_node(context, offset, &leaf, &count, &entries, &children))
		return(false);
	if (leaf) {
		for (position = 0; position < count; position++) {
			int compare = memcmp(entry, entries + (size_t)position * context->entry_size,
				context->entry_size);
			if (compare == 0) {
				free(entries);
				return(false);
			}
			if (compare < 0)
				break;
		}
		new_count = count + 1;
		new_entries = malloc((size_t)new_count * context->entry_size);
		if (new_entries == NULL)
			goto fail;
		if (position)
			memcpy(new_entries, entries, (size_t)position * context->entry_size);
		memcpy(new_entries + (size_t)position * context->entry_size, entry,
			context->entry_size);
		if (count != position)
			memcpy(new_entries + (size_t)(position + 1) * context->entry_size,
				entries + (size_t)position * context->entry_size,
				(size_t)(count - position) * context->entry_size);
		free(entries);
		if (new_count <= INDEX_V2_MAX_KEYS) {
			if (!v2_write_node(context, true, new_count, new_entries, NULL,
					&result->offset, &result->crc)) {
				free(new_entries);
				return(false);
			}
			free(new_entries);
			result->leaf_offset = result->offset;
			result->leaf_index = position;
			return(true);
		}
		if (!v2_write_node(context, true, INDEX_V2_MAX_KEYS / 2, new_entries, NULL,
				&result->offset, &result->crc) ||
			!v2_write_node(context, true, new_count - INDEX_V2_MAX_KEYS / 2,
				new_entries + (size_t)(INDEX_V2_MAX_KEYS / 2) * context->entry_size,
				NULL, &result->right_offset, &child.crc)) {
			free(new_entries);
			return(false);
		}

		uint8_t split_position = INDEX_V2_MAX_KEYS / 2;

		if (position < split_position) {
			result->leaf_offset = result->offset;
			result->leaf_index = position;
		} else {
			result->leaf_offset = result->right_offset;
			result->leaf_index = position - split_position;
		}

		result->separator = malloc(context->entry_size);
		if (result->separator == NULL) {
			free(new_entries);
			return(false);
		}
		memcpy(result->separator, new_entries + (size_t)(INDEX_V2_MAX_KEYS / 2) *
			context->entry_size, context->entry_size);
		free(new_entries);
		result->split = true;
		return(true);
	}
	for (position = 0; position < count; position++)
		if (memcmp(entry, entries + (size_t)position * context->entry_size,
				context->entry_size) < 0)
			break;
	if (!v2_insert_node(context, children[position], entry, &child))
		goto fail;

	result->leaf_offset = child.leaf_offset;
	result->leaf_index = child.leaf_index;

	new_count = count + (child.split ? 1 : 0);
	new_entries = malloc((size_t)new_count * context->entry_size);
	new_children = malloc((size_t)(new_count + 1) * sizeof(*new_children));
	if (new_entries == NULL || new_children == NULL) {
		free(child.separator);
		free(new_entries);
		free(new_children);
		goto fail;
	}
	memcpy(new_entries, entries, (size_t)position * context->entry_size);
	if (child.split)
		memcpy(new_entries + (size_t)position * context->entry_size,
			child.separator, context->entry_size);
	memcpy(new_entries + (size_t)(position + child.split) * context->entry_size,
		entries + (size_t)position * context->entry_size,
		(size_t)(count - position) * context->entry_size);
	for (i = 0; i < position; i++)
		new_children[i] = children[i];
	new_children[position] = child.offset;
	if (child.split)
		new_children[position + 1] = child.right_offset;
	for (i = position + 1; i <= count; i++)
		new_children[i + child.split] = children[i];
	free(child.separator);
	free(entries);
	free(children);
	if (new_count <= INDEX_V2_MAX_KEYS) {
		if (!v2_write_node(context, false, new_count, new_entries, new_children,
				&result->offset, &result->crc)) {
			free(new_entries);
			free(new_children);
			return(false);
		}
		free(new_entries);
		free(new_children);
		return(true);
	}
	position = INDEX_V2_MAX_KEYS / 2;
	result->separator = malloc(context->entry_size);
	if (result->separator == NULL)
		goto split_fail;
	memcpy(result->separator, new_entries + (size_t)position * context->entry_size,
		context->entry_size);
	if (!v2_write_node(context, false, position, new_entries, new_children,
			&result->offset, &result->crc) ||
			!v2_write_node(context, false, new_count - position - 1,
				new_entries + (size_t)(position + 1) * context->entry_size,
				new_children + position + 1, &result->right_offset, &child.crc)) {
		free(result->separator);
		result->separator = NULL;
		goto split_fail;
	}
	free(new_entries);
	free(new_children);
	result->split = true;
	return(true);

split_fail:
	free(new_entries);
	free(new_children);
	return(false);
fail:
	free(entries);
	free(children);
	return(false);
}

static bool v2_mark_live_pages(int fd, uint64_t offset, uint16_t keylen,
		uint16_t page_size, uint64_t first_page, size_t page_count,
		unsigned char *live)
{
	INDEX_V2_NODE node;
	size_t page;
	uint32_t expected;
	uint8_t i;

	if (offset < first_page || (offset - first_page) % page_size != 0)
		return(false);
	page = (size_t)((offset - first_page) / page_size);
	if (page >= page_count || live[page])
		return(page < page_count);
	live[page] = 1;
	if (!index_v2_read_node(fd, offset, &node))
		return(false);
	expected = (node.flags & INDEX_V2_NODE_LEAF) ?
		(uint32_t)node.key_count * (keylen + INDEX_V2_FILE_ID_SIZE +
			INDEX_V2_RECORD_OFFSET_SIZE) : (uint32_t)(node.key_count + 1) * 8 +
		(uint32_t)node.key_count * (keylen + INDEX_V2_FILE_ID_SIZE +
			INDEX_V2_RECORD_OFFSET_SIZE);
	if (node.key_count > INDEX_V2_MAX_KEYS || node.payload_length != expected ||
		(!(node.flags & INDEX_V2_NODE_LEAF) && node.key_count == 0)) {
		index_v2_free_node(&node);
		return(false);
	}
	if (!(node.flags & INDEX_V2_NODE_LEAF))
		for (i = 0; i <= node.key_count; i++)
			if (!v2_mark_live_pages(fd, get_u64(node.payload + (size_t)i * 8),
					keylen, page_size, first_page, page_count, live)) {
				index_v2_free_node(&node);
				return(false);
			}
	index_v2_free_node(&node);
	return(true);
}

static bool v2_collect_free_pages(V2_INSERT_CONTEXT *context)
{
	unsigned char header[INDEX_V2_HEADER_SIZE];
	unsigned char *live;
	uint64_t roots[INDEX_V2_ROOT_SLOTS], generation, ignored_root;
	uint32_t crcs[INDEX_V2_ROOT_SLOTS], ignored_crc;
	bool valid[INDEX_V2_ROOT_SLOTS];
	uint32_t names_length;
	uint64_t first_page;
	struct stat status;
	size_t page_count, i;

	if (!read_full_at(context->fd, header, sizeof(header), 0) ||
			fstat(context->fd, &status) != 0 || status.st_size < 0)
		return(false);
	names_length = get_u32(header + INDEX_V2_NAMES_LENGTH_OFFSET);
	if (!index_v2_read_page_size(context->fd, context->keylen, &context->page_size))
		return(false);
	first_page = ((INDEX_V2_HEADER_SIZE + (uint64_t)names_length +
		context->page_size - 1) / context->page_size) * context->page_size;
	if ((uint64_t)status.st_size < first_page ||
		((uint64_t)status.st_size - first_page) % context->page_size != 0)
		return(false);
	page_count = (size_t)(((uint64_t)status.st_size - first_page) /
		context->page_size);
	live = calloc(page_count, 1);
	context->free_pages = malloc(page_count * sizeof(*context->free_pages));
	if (live == NULL || context->free_pages == NULL) {
		free(live);
		free(context->free_pages);
		context->free_pages = NULL;
		return(false);
	}
	valid[0] = read_slot(header + INDEX_V2_SLOT0_OFFSET, &roots[0], &crcs[0],
		&generation);
	valid[1] = read_slot(header + INDEX_V2_SLOT1_OFFSET, &roots[1], &crcs[1],
		&generation);
	for (i = 0; i < INDEX_V2_ROOT_SLOTS; i++)
		if (valid[i] && !v2_mark_live_pages(context->fd, roots[i], context->keylen,
				context->page_size, first_page, page_count, live)) {
			free(live);
			free(context->free_pages);
			context->free_pages = NULL;
			return(false);
		}
	context->free_count = 0;
	for (i = 0; i < page_count; i++)
		if (!live[i])
			context->free_pages[context->free_count++] = first_page +
				(uint64_t)i * context->page_size;
	free(live);
	(void)ignored_root;
	(void)ignored_crc;
	return(true);
}

bool index_v2_insert(int fd, const void *key, uint16_t file_id,
					uint64_t record_offset, INDEX_V2_CURSOR *cursor,
					uint64_t *root_offset)
{
	V2_INSERT_CONTEXT context = { 0 };
	V2_INSERT_RESULT result;

	struct stat status;
	uint64_t root;
	uint64_t generation;
	uint32_t root_crc;
	unsigned char entry[INDEX_V2_MAX_KEYS + 32];
	uint64_t children[2];

	if (key == NULL || !index_v2_read_header(fd, &context.keylen,
			&context.file_count, &root, &root_crc, &generation) ||
			context.keylen > 32 || file_id >= context.file_count ||
			fstat(fd, &status) != 0 || status.st_size < 0)
		return(false);

	context.fd = fd;
	context.entry_size = context.keylen + INDEX_V2_FILE_ID_SIZE + INDEX_V2_RECORD_OFFSET_SIZE;

	if (!index_v2_read_page_size(fd, context.keylen, &context.page_size))
		return(false);

	context.next_offset = (((uint64_t)status.st_size + context.page_size - 1) / context.page_size) * context.page_size;

	if (!v2_collect_free_pages(&context))
		return(false);

	memcpy(entry, key, context.keylen);
	put_u16(entry + context.keylen, file_id);
	put_u64(entry + context.keylen + INDEX_V2_FILE_ID_SIZE, record_offset);

	if (!v2_insert_node(&context, root, entry, &result)) {
		free(context.free_pages);
		return(false);
	}

	if (result.split) {
		children[0] = result.offset;
		children[1] = result.right_offset;
		if (!v2_write_node(&context, false, 1, result.separator, children,
				&result.offset, &result.crc)) {
			free(result.separator);
			free(context.free_pages);
			return(false);
		}
		free(result.separator);
	}

	if (fdatasync(fd) != 0) {
		free(context.free_pages);
		return(false);
	}

	free(context.free_pages);
	if (!index_v2_publish_root(fd, result.offset, result.crc, generation + 1))
		return(false);

	*root_offset = result.offset;
	cursor->generation = generation+1;
	cursor->node_offset = result.leaf_offset;
	cursor->entry_index = result.leaf_index;
	return true;
}

bool index_v2_build_insert(int fd, const void *key, uint16_t file_id,
					uint64_t record_offset, uint64_t *root_offset)
{
	V2_INSERT_CONTEXT context = { 0 };
	V2_INSERT_RESULT result;
	struct stat status;
	unsigned char entry[INDEX_V2_MAX_KEYS + 32];
	unsigned char encoded_root[8];
	uint64_t root;
	uint64_t children[2];
	uint64_t new_root;

	if (key == NULL || root_offset == NULL ||
			!v2_read_build_header(fd, &context.keylen, &context.file_count, &root) ||
			file_id >= context.file_count || fstat(fd, &status) != 0 ||
			status.st_size < 0)
		return(false);
	context.fd = fd;
	context.entry_size = context.keylen + INDEX_V2_FILE_ID_SIZE +
		INDEX_V2_RECORD_OFFSET_SIZE;
	if (!index_v2_read_page_size(fd, context.keylen, &context.page_size))
		return(false);
	context.next_offset = (((uint64_t)status.st_size + context.page_size - 1) /
		context.page_size) * context.page_size;
	context.in_place = true;
	memcpy(entry, key, context.keylen);
	put_u16(entry + context.keylen, file_id);
	put_u64(entry + context.keylen + INDEX_V2_FILE_ID_SIZE, record_offset);
	if (!v2_insert_node(&context, root, entry, &result))
		return(false);
	if (result.split) {
		children[0] = result.offset;
		children[1] = result.right_offset;
		new_root = 0;
		if (!v2_write_node(&context, false, 1, result.separator, children,
				&new_root, &result.crc)) {
			free(result.separator);
			return(false);
		}
		free(result.separator);
		root = new_root;
		put_u64(encoded_root, root);
		if (!write_full_at(fd, encoded_root, sizeof(encoded_root),
				INDEX_V2_BUILD_ROOT_OFFSET))
			return(false);
	}
	*root_offset = root;
	return(true);
}

bool index_v2_build_finish(int fd, uint64_t *root_offset)
{
	INDEX_V2_NODE root_node;
	unsigned char cleared[10] = { 0 };
	uint16_t keylen, file_count;
	uint64_t root;

	if (!v2_read_build_header(fd, &keylen, &file_count, &root) ||
			!index_v2_read_node(fd, root, &root_node))
		return(false);
	if (fdatasync(fd) != 0 ||
			!index_v2_publish_root(fd, root, root_node.crc, 1)) {
		index_v2_free_node(&root_node);
		return(false);
	}
	index_v2_free_node(&root_node);
	if (!write_full_at(fd, cleared, sizeof(cleared), INDEX_V2_FLAGS_OFFSET) ||
			fdatasync(fd) != 0)
		return(false);
	if (root_offset != NULL)
		*root_offset = root;
	(void)keylen;
	(void)file_count;
	return(true);
}

#define INDEX_V2_MIN_KEYS (INDEX_V2_MAX_KEYS / 2)

typedef struct v2_remove_result {
	uint64_t offset;
	uint32_t crc;
	uint8_t key_count;
	bool leaf;
	bool found;
} V2_REMOVE_RESULT;

/* Separators are derived data: entry i is the first entry below child i+1. */
static bool v2_first_entry(V2_INSERT_CONTEXT *context, uint64_t offset,
		unsigned char *entry)
{
	bool leaf;
	uint8_t count;
	unsigned char *entries;
	uint64_t *children;
	uint64_t child;

	if (!v2_load_node(context, offset, &leaf, &count, &entries, &children))
		return(false);
	if (leaf) {
		if (count == 0) {
			free(entries);
			return(false);
		}
		memcpy(entry, entries, context->entry_size);
		free(entries);
		return(true);
	}
	child = children[0];
	free(entries);
	free(children);
	return(v2_first_entry(context, child, entry));
}

static bool v2_write_internal(V2_INSERT_CONTEXT *context, uint8_t count,
		const uint64_t *children, uint64_t *offset, uint32_t *crc)
{
	unsigned char *entries;
	uint8_t i;
	bool ok = false;

	if (count == 0 || count > INDEX_V2_MAX_KEYS)
		return(false);
	entries = malloc((size_t)count * context->entry_size);
	if (entries == NULL)
		return(false);
	for (i = 0; i < count; i++)
		if (!v2_first_entry(context, children[i + 1],
				entries + (size_t)i * context->entry_size))
			goto done;
	ok = v2_write_node(context, false, count, entries, children, offset, crc);
done:
	free(entries);
	return(ok);
}

static bool v2_remove_node(V2_INSERT_CONTEXT *context, uint64_t offset,
		const unsigned char *target, bool root, V2_REMOVE_RESULT *result)
{
	bool leaf, sibling_leaf;
	uint8_t count, sibling_count, position, i;
	unsigned char *entries = NULL, *sibling_entries = NULL, *combined = NULL;
	uint64_t *children = NULL, *sibling_children = NULL, *new_children = NULL;
	uint64_t sibling_offset, rewritten_sibling;
	uint32_t sibling_crc;
	V2_REMOVE_RESULT child;

	bool returnValue = false;

	memset(result, 0, sizeof(*result));
	if (!v2_load_node(context, offset, &leaf, &count, &entries, &children))
		return(false);
	result->leaf = leaf;
	if (leaf) {
		for (position = 0; position < count; position++)
			if (memcmp(target, entries + (size_t)position * context->entry_size,
					context->entry_size) == 0)
				break;
		if (position == count) {
			free(entries);
			return(true);
		}
		memmove(entries + (size_t)position * context->entry_size,
			entries + (size_t)(position + 1) * context->entry_size,
			(size_t)(count - position - 1) * context->entry_size);
		count--;
		if (!v2_write_node(context, true, count, entries, NULL,
				&result->offset, &result->crc)) {
			free(entries);
			return(false);
		}
		free(entries);
		result->key_count = count;
		result->found = true;
		return(true);
	}

	for (position = 0; position < count; position++)
		if (memcmp(target, entries + (size_t)position * context->entry_size,
				context->entry_size) < 0)
			break;
	if (!v2_remove_node(context, children[position], target, false, &child))
		goto done;
	if (!child.found) {
		free(entries);
		free(children);
		return(true);
	}
	children[position] = child.offset;
	free(entries);
	entries = NULL;

	if (child.key_count < INDEX_V2_MIN_KEYS) {
		/* Prefer borrowing from the left sibling. */
		if (position > 0) {
			sibling_offset = children[position - 1];
			if (!v2_load_node(context, sibling_offset, &sibling_leaf,
					&sibling_count, &sibling_entries, &sibling_children) ||
					sibling_leaf != child.leaf)
				goto done;
			if (sibling_count > INDEX_V2_MIN_KEYS) {
				if (child.leaf) {
					combined = malloc((size_t)(child.key_count + 1) * context->entry_size);
					if (combined == NULL)
						goto done;
					memcpy(combined, sibling_entries + (size_t)(sibling_count - 1) *
						context->entry_size, context->entry_size);
					if (!v2_load_node(context, child.offset, &leaf, &i, &entries,
							&new_children))
						goto done;
					memcpy(combined + context->entry_size, entries,
						(size_t)i * context->entry_size);
					if (!v2_write_node(context, true, sibling_count - 1, sibling_entries,
							NULL, &rewritten_sibling, &sibling_crc) ||
							!v2_write_node(context, true, i + 1, combined, NULL,
							&child.offset, &child.crc))
						goto done;
					child.key_count = i + 1;
				} else {
					if (!v2_load_node(context, child.offset, &leaf, &i, &entries,
							&new_children))
						goto done;
					new_children = realloc(new_children,
						(size_t)(i + 2) * sizeof(*new_children));
					if (new_children == NULL)
						goto done;
					memmove(new_children + 1, new_children,
						(size_t)(i + 1) * sizeof(*new_children));
					new_children[0] = sibling_children[sibling_count];
					if (!v2_write_internal(context, sibling_count - 1, sibling_children,
							&rewritten_sibling, &sibling_crc) ||
							!v2_write_internal(context, i + 1, new_children,
							&child.offset, &child.crc))
						goto done;
					child.key_count = i + 1;
				}
				children[position - 1] = rewritten_sibling;
				children[position] = child.offset;
				goto balanced;
			}
			free(sibling_entries); sibling_entries = NULL;
			free(sibling_children); sibling_children = NULL;
		}

		/* Then borrow from the right sibling. */
		if (position < count) {
			sibling_offset = children[position + 1];
			if (!v2_load_node(context, sibling_offset, &sibling_leaf,
					&sibling_count, &sibling_entries, &sibling_children) ||
					sibling_leaf != child.leaf)
				goto done;
			if (sibling_count > INDEX_V2_MIN_KEYS) {
				if (child.leaf) {
					if (!v2_load_node(context, child.offset, &leaf, &i, &entries,
							&new_children))
						goto done;
					combined = realloc(entries, (size_t)(i + 1) * context->entry_size);
					if (combined == NULL)
						goto done;
					entries = NULL;
					memcpy(combined + (size_t)i * context->entry_size, sibling_entries,
						context->entry_size);
					memmove(sibling_entries, sibling_entries + context->entry_size,
						(size_t)(sibling_count - 1) * context->entry_size);
					if (!v2_write_node(context, true, i + 1, combined, NULL,
							&child.offset, &child.crc) ||
							!v2_write_node(context, true, sibling_count - 1,
							sibling_entries, NULL, &rewritten_sibling, &sibling_crc))
						goto done;
					child.key_count = i + 1;
				} else {
					if (!v2_load_node(context, child.offset, &leaf, &i, &entries,
							&new_children))
						goto done;
					new_children = realloc(new_children,
						(size_t)(i + 2) * sizeof(*new_children));
					if (new_children == NULL)
						goto done;
					new_children[i + 1] = sibling_children[0];
					memmove(sibling_children, sibling_children + 1,
						(size_t)sibling_count * sizeof(*sibling_children));
					if (!v2_write_internal(context, i + 1, new_children,
							&child.offset, &child.crc) ||
							!v2_write_internal(context, sibling_count - 1,
							sibling_children, &rewritten_sibling, &sibling_crc))
						goto done;
					child.key_count = i + 1;
				}
				children[position] = child.offset;
				children[position + 1] = rewritten_sibling;
				goto balanced;
			}
			free(sibling_entries); sibling_entries = NULL;
			free(sibling_children); sibling_children = NULL;
		}

		/* Neither sibling can lend, so merge left when one exists. */
		if (position > 0) {
			sibling_offset = children[position - 1];
			if (!v2_load_node(context, sibling_offset, &sibling_leaf,
					&sibling_count, &sibling_entries, &sibling_children) ||
					!v2_load_node(context, child.offset, &leaf, &i, &entries,
						&new_children) || sibling_leaf != leaf)
				goto done;
			if (child.leaf) {
				combined = realloc(sibling_entries,
					(size_t)(sibling_count + i) * context->entry_size);
				if (combined == NULL)
					goto done;
				sibling_entries = NULL;
				memcpy(combined + (size_t)sibling_count * context->entry_size,
					entries, (size_t)i * context->entry_size);
				if (!v2_write_node(context, true, sibling_count + i, combined, NULL,
						&child.offset, &child.crc))
					goto done;
			} else {
				sibling_children = realloc(sibling_children,
					(size_t)(sibling_count + i + 2) * sizeof(*sibling_children));
				if (sibling_children == NULL)
					goto done;
				memcpy(sibling_children + sibling_count + 1, new_children,
					(size_t)(i + 1) * sizeof(*new_children));
				if (!v2_write_internal(context, sibling_count + i + 1,
						sibling_children, &child.offset, &child.crc))
					goto done;
			}
			children[position - 1] = child.offset;
			memmove(children + position, children + position + 1,
				(size_t)(count - position) * sizeof(*children));
			count--;
		} else {
			sibling_offset = children[1];
			if (!v2_load_node(context, sibling_offset, &sibling_leaf,
					&sibling_count, &sibling_entries, &sibling_children) ||
					!v2_load_node(context, child.offset, &leaf, &i, &entries,
						&new_children) || sibling_leaf != leaf)
				goto done;
			if (child.leaf) {
				combined = realloc(entries,
					(size_t)(i + sibling_count) * context->entry_size);
				if (combined == NULL)
					goto done;
				entries = NULL;
				memcpy(combined + (size_t)i * context->entry_size, sibling_entries,
					(size_t)sibling_count * context->entry_size);
				if (!v2_write_node(context, true, i + sibling_count, combined, NULL,
						&child.offset, &child.crc))
					goto done;
			} else {
				new_children = realloc(new_children,
					(size_t)(i + sibling_count + 2) * sizeof(*new_children));
				if (new_children == NULL)
					goto done;
				memcpy(new_children + i + 1, sibling_children,
					(size_t)(sibling_count + 1) * sizeof(*sibling_children));
				if (!v2_write_internal(context, i + sibling_count + 1,
						new_children, &child.offset, &child.crc))
					goto done;
			}
			children[0] = child.offset;
			memmove(children + 1, children + 2,
				(size_t)(count - 1) * sizeof(*children));
			count--;
		}
	}

balanced:
	/* A zero-key internal root is represented by its only child. */
	if (root && count == 0) {
		*result = child;
		result->found = true;
		returnValue = true;
	} else if (!v2_write_internal(context, count, children, &result->offset, &result->crc)) {
		;						// this is actually a fail condition.  we don't want to change returnValue
	} else {
		result->key_count = count;
		result->leaf = false;
		result->found = true;
		returnValue = true;
	}

done:
	free(entries);
	free(children);
	free(sibling_entries);
	free(sibling_children);
	free(combined);
	free(new_children);
	return(returnValue);
}

bool index_v2_remove(int fd, const void *key, uint16_t file_id,
					uint64_t record_offset)
{
	V2_INSERT_CONTEXT context = { 0 };
	V2_REMOVE_RESULT result;
	struct stat status;
	uint64_t root, generation;
	uint32_t root_crc;
	unsigned char target[INDEX_V2_MAX_KEYS + 32];

	if (key == NULL || !index_v2_read_header(fd, &context.keylen,
			&context.file_count, &root, &root_crc, &generation) ||
			context.keylen > 32 || file_id >= context.file_count ||
			fstat(fd, &status) != 0 || status.st_size < 0)
		return(false);
	context.fd = fd;
	context.entry_size = context.keylen + INDEX_V2_FILE_ID_SIZE +
		INDEX_V2_RECORD_OFFSET_SIZE;
	if (!index_v2_read_page_size(fd, context.keylen, &context.page_size))
		return(false);
	context.next_offset = (((uint64_t)status.st_size + context.page_size - 1) /
		context.page_size) * context.page_size;
	if (!v2_collect_free_pages(&context)) {
		free(context.free_pages);
		return(false);
	}
	memcpy(target, key, context.keylen);
	put_u16(target + context.keylen, file_id);
	put_u64(target + context.keylen + INDEX_V2_FILE_ID_SIZE, record_offset);
	if (!v2_remove_node(&context, root, target, true, &result) || !result.found)
		goto fail;
	free(context.free_pages);
	if (fdatasync(fd) != 0)
		return(false);
	return(index_v2_publish_root(fd, result.offset, result.crc, generation + 1));
fail:
	free(context.free_pages);
	return(false);
}

static bool v2_find_entry(V2_INSERT_CONTEXT *context, uint64_t offset,
		const unsigned char *target, bool exact, size_t key_length,
		uint16_t *file_id, uint64_t *record_offset, unsigned char *matched_key,
		INDEX_V2_CURSOR *cursor)
{
	bool leaf;
	uint8_t count, position;
	unsigned char *entries;
	unsigned char *entry;
	uint64_t *children;

	for (;;) {
		if (!v2_load_node(context, offset, &leaf, &count, &entries, &children))
			return(false);

		for (position = 0; position < count; position++) {
			int compare;
			entry = entries + (size_t)position * context->entry_size;
			compare = memcmp(target, entry, context->entry_size);
			if (compare < 0 || (leaf && compare == 0))
				break;
		}

		if (!leaf) {
			offset = children[position];
			free(entries);
			free(children);
			continue;
		}
		free(children);

		if (position == count) {
			free(entries);
			return(false);
		}
		entry = entries + (size_t)position * context->entry_size;

		if ((exact && memcmp(target, entry, context->entry_size) != 0) ||
				(!exact && memcmp(target, entry, key_length) != 0)) {
			free(entries);
			return(false);
		}

		*file_id = get_u16(entry + context->keylen);
		*record_offset = get_u64(entry + context->keylen +
			INDEX_V2_FILE_ID_SIZE);
		if (matched_key != NULL)
			memcpy(matched_key, entry, context->keylen);
		if (cursor != NULL) {
			cursor->node_offset = offset;
			cursor->entry_index = position;
		}
		free(entries);
		return(true);
	}
}

static bool v2_pattern_matches(const unsigned char *entry,
		const unsigned char *pattern, size_t length)
{
	size_t i;

	for (i = 0; i < length; i++) {
		if (pattern[i] == '*') {
			if (entry[i] == '\0')
				return(false);
		} else if (entry[i] != pattern[i])
			return(false);
	}
	return(true);
}

typedef enum {
	V2_FIND_ERROR = -1,
	V2_FIND_NOT_FOUND,
	V2_FIND_FOUND
} V2_FIND_RESULT;

/*
 * args
 *     context - the search context
 *     offset - offset in the index to the node we're reading
 *     pattern - the template key we are searching for
 *     pattern_length - length of the template
 *     file_id - receives the file offset in the index file descriptions
 *     record_offset - receives the offset to the record in the data file
 *     matched_key - receives the logical key that was found
 *     cursor - receives the leaf page and entry index
 */
static V2_FIND_RESULT v2_find_pattern_node(V2_INSERT_CONTEXT *context,
		uint64_t offset,
		const unsigned char *pattern, size_t pattern_length, uint16_t *file_id,
		uint64_t *record_offset, unsigned char *matched_key,
		INDEX_V2_CURSOR *cursor)
{
	bool leaf;
	uint8_t count, i;
	unsigned char *entries;
	unsigned char *entry;
	uint64_t *children;
	V2_FIND_RESULT result;

	if (!v2_load_node(context, offset, &leaf, &count, &entries, &children))
		return(V2_FIND_ERROR);

	if (leaf) {
		for (i = 0; i < count; i++) {
			entry = entries + (size_t)i * context->entry_size;
			if (!v2_pattern_matches(entry, pattern, pattern_length))
				continue;
			*file_id = get_u16(entry + context->keylen);
			*record_offset = get_u64(entry + context->keylen +
				INDEX_V2_FILE_ID_SIZE);
			if (matched_key != NULL)
				memcpy(matched_key, entry, context->keylen);
			if (cursor != NULL) {
				cursor->node_offset = offset;
				cursor->entry_index = i;
			}
			free(entries);
			return(V2_FIND_FOUND);
		}
		free(entries);
		return(V2_FIND_NOT_FOUND);
	}
	free(entries);

	for (i = 0; i <= count; i++) {
		result = v2_find_pattern_node(context, children[i], pattern,
			pattern_length, file_id, record_offset, matched_key, cursor);
		if (result != V2_FIND_NOT_FOUND) {
			free(children);
			return(result);
		}
	}
	free(children);
	return(V2_FIND_NOT_FOUND);
}

/*
 * args
 *     fd - index file descriptor
 *     key - the key we're searching for
 *     exact - are we looking for an exact key? (key+fno+recno)
 *     file_id - receives the datafile number from the index
 *     record_offset receives the record number
 *     matched_key receives the found logical key (set to null if you don't care about it)
 *     cursor receives the generation-qualified leaf location
 */
bool index_v2_find(int fd, const void *key, bool exact,
		uint16_t *file_id, uint64_t *record_offset, void *matched_key,
		INDEX_V2_CURSOR *cursor)
{
	V2_INSERT_CONTEXT context = { 0 };
	uint64_t root, generation;
	uint32_t root_crc;
	unsigned char target[INDEX_V2_MAX_KEYS + 32];
	size_t key_length;
	V2_FIND_RESULT result;

	if (key == NULL || file_id == NULL || record_offset == NULL ||
			!index_v2_read_header(fd, &context.keylen, &context.file_count,
				&root, &root_crc, &generation) || context.keylen > 32)
		return(false);
	if (cursor != NULL)
		memset(cursor, 0, sizeof(*cursor));

	context.fd = fd;
	context.entry_size = context.keylen + INDEX_V2_FILE_ID_SIZE + INDEX_V2_RECORD_OFFSET_SIZE;
	memcpy(target, key, context.keylen);
	key_length = strnlen((const char *)target, context.keylen);
	put_u16(target + context.keylen, exact ? *file_id : 0);
	put_u64(target + context.keylen + INDEX_V2_FILE_ID_SIZE, exact ? *record_offset : 0);

	if (exact && *file_id >= context.file_count)
		return(false);

	if (!exact && memchr(target, '*', key_length) != NULL) {
		result = v2_find_pattern_node(&context, root, target, key_length, file_id,
			record_offset, matched_key, cursor);
		if (result == V2_FIND_FOUND && cursor != NULL)
			cursor->generation = generation;
		return(result == V2_FIND_FOUND);
	}

	if (!v2_find_entry(&context, root, target, exact, key_length, file_id,
			record_offset, matched_key, cursor))
		return(false);
	if (cursor != NULL)
		cursor->generation = generation;
	return(true);
}

static bool v2_descend_tree(bool direction, int fd, uint16_t *file_id,
		uint64_t *record_offset, void *matched_key, INDEX_V2_CURSOR *cursor)
{

	V2_INSERT_CONTEXT context = { 0 };

	uint64_t root, generation;
	uint64_t *children;
	uint32_t root_crc;
	uint8_t key_count;
	int entry_index;

	bool leaf;
	bool return_value = false;

	unsigned char *entries;
	unsigned char *kptr;

	if (!file_id || !record_offset || !matched_key || !cursor) {
		return false;
	}
	if (!index_v2_read_header(fd, &context.keylen, &context.file_count, &root, &root_crc, &generation)) {
		return false;
	}

	uint64_t offset = root;
	context.fd = fd;
	context.entry_size = context.keylen + INDEX_V2_FILE_ID_SIZE + INDEX_V2_RECORD_OFFSET_SIZE;

	while(1) {
		if (!v2_load_node(&context, offset, &leaf, &key_count, &entries, &children)) {
			return false;
		}
		if (leaf) {
			break;
		}
		offset = direction ? children[key_count] : children[0];
		free(entries);
		free(children);
	}

	if (key_count == 0) {
		free(entries);
		goto done;
	}

	entry_index = direction ? key_count - 1 : 0;
	kptr = entries + entry_index * context.entry_size;
	memcpy(matched_key, kptr, context.keylen);
	*file_id = get_u16(kptr+context.keylen);
	*record_offset = get_u64(kptr + context.keylen + INDEX_V2_FILE_ID_SIZE);
	cursor->generation = generation;
	cursor->node_offset = offset;
	cursor->entry_index = entry_index;
	return_value = true;

done:
	free(entries);
	return return_value;
}

bool index_v2_first(int fd, uint16_t *file_id, uint64_t *record_offset,
		void *matched_key, INDEX_V2_CURSOR *cursor)
{
	
	return v2_descend_tree(false, fd, file_id, record_offset, matched_key, cursor);
}

bool index_v2_last(int fd, uint16_t *file_id, uint64_t *record_offset,
		void *matched_key, INDEX_V2_CURSOR *cursor)
{
	
	return v2_descend_tree(true, fd, file_id, record_offset, matched_key, cursor);
}

typedef struct v2_path_entry {
	uint64_t offset;
	uint8_t child_index;
} V2_PATH_ENTRY;

static bool v2_set_cursor_entry(V2_INSERT_CONTEXT *context, uint64_t generation,
		uint64_t node_offset, uint8_t entry_index, const unsigned char *entry,
		uint16_t *file_id, uint64_t *record_offset, void *matched_key,
		INDEX_V2_CURSOR *cursor)
{
	*file_id = get_u16(entry + context->keylen);
	if (*file_id >= context->file_count)
		return(false);
	*record_offset = get_u64(entry + context->keylen + INDEX_V2_FILE_ID_SIZE);
	memcpy(matched_key, entry, context->keylen);
	cursor->generation = generation;
	cursor->node_offset = node_offset;
	cursor->entry_index = entry_index;
	return(true);
}

static bool v2_find_adjacent(int fd, const void *key, uint16_t *file_id,
		uint64_t *record_offset, const INDEX_V2_CURSOR *hint, void *matched_key,
		INDEX_V2_CURSOR *cursor, bool forward)
{
	V2_INSERT_CONTEXT context = { 0 };
	V2_PATH_ENTRY *path = NULL;
	size_t path_capacity = 0, path_count = 0;
	unsigned char target[INDEX_V2_MAX_KEYS + 32];
	unsigned char *entries, *entry;
	uint64_t *children;
	uint64_t root, generation, offset, child;
	uint32_t root_crc;
	uint8_t count, position, selected;
	bool leaf, found = false;

	if (key == NULL || file_id == NULL || record_offset == NULL || hint == NULL ||
			matched_key == NULL || cursor == NULL ||
			!index_v2_read_header(fd, &context.keylen, &context.file_count, &root,
				&root_crc, &generation) || context.keylen > 32 ||
			*file_id >= context.file_count)
		return(false);
	context.fd = fd;
	context.entry_size = context.keylen + INDEX_V2_FILE_ID_SIZE +
		INDEX_V2_RECORD_OFFSET_SIZE;
	memcpy(target, key, context.keylen);
	put_u16(target + context.keylen, *file_id);
	put_u64(target + context.keylen + INDEX_V2_FILE_ID_SIZE, *record_offset);

	if (hint->generation == generation &&
			v2_load_node(&context, hint->node_offset, &leaf, &count, &entries,
				&children)) {
		if (leaf && hint->entry_index < count &&
				memcmp(entries + (size_t)hint->entry_index * context.entry_size,
					target, context.entry_size) == 0 &&
				((forward && hint->entry_index + 1 < count) ||
				(!forward && hint->entry_index > 0))) {
			selected = forward ? hint->entry_index + 1 : hint->entry_index - 1;
			entry = entries + (size_t)selected * context.entry_size;
			found = v2_set_cursor_entry(&context, generation, hint->node_offset,
				selected, entry, file_id, record_offset, matched_key, cursor);
		}
		free(entries);
		free(children);
		if (found)
			return(true);
	}

	offset = root;
	for (;;) {
		if (!v2_load_node(&context, offset, &leaf, &count, &entries, &children))
			goto done;
		for (position = 0; position < count; position++) {
			int compare = memcmp(target,
				entries + (size_t)position * context.entry_size,
				context.entry_size);

			if (compare < 0 || (leaf && compare == 0))
				break;
		}
		if (leaf)
			break;
		if (path_count == path_capacity) {
			V2_PATH_ENTRY *new_path;
			size_t new_capacity = path_capacity == 0 ? 16 : path_capacity * 2;

			new_path = realloc(path, new_capacity * sizeof(*path));
			if (new_path == NULL) {
				free(entries);
				free(children);
				goto done;
			}
			path = new_path;
			path_capacity = new_capacity;
		}
		path[path_count].offset = offset;
		path[path_count++].child_index = position;
		offset = children[position];
		free(entries);
		free(children);
	}

	if (position == count || memcmp(target,
			entries + (size_t)position * context.entry_size,
			context.entry_size) != 0) {
		free(entries);
		free(children);
		goto done;
	}
	if ((forward && position + 1 < count) || (!forward && position > 0)) {
		selected = forward ? position + 1 : position - 1;
		entry = entries + (size_t)selected * context.entry_size;
		found = v2_set_cursor_entry(&context, generation, offset, selected, entry,
			file_id, record_offset, matched_key, cursor);
		free(entries);
		free(children);
		goto done;
	}
	free(entries);
	free(children);

	while (path_count != 0) {
		V2_PATH_ENTRY parent = path[--path_count];

		if (!v2_load_node(&context, parent.offset, &leaf, &count, &entries,
				&children))
			goto done;
		if ((forward && parent.child_index < count) ||
				(!forward && parent.child_index > 0)) {
			child = children[forward ? parent.child_index + 1 :
				parent.child_index - 1];
			free(entries);
			free(children);
			offset = child;
			for (;;) {
				if (!v2_load_node(&context, offset, &leaf, &count, &entries,
						&children))
					goto done;
				if (leaf)
					break;
				offset = children[forward ? 0 : count];
				free(entries);
				free(children);
			}
			if (count != 0) {
				selected = forward ? 0 : count - 1;
				entry = entries + (size_t)selected * context.entry_size;
				found = v2_set_cursor_entry(&context, generation, offset, selected,
					entry, file_id, record_offset, matched_key, cursor);
			}
			free(entries);
			free(children);
			goto done;
		}
		free(entries);
		free(children);
	}

done:
	free(path);
	return(found);
}

bool index_v2_next(int fd, const void *key, uint16_t *file_id,
		uint64_t *record_offset, const INDEX_V2_CURSOR *hint, void *matched_key,
		INDEX_V2_CURSOR *cursor)
{
	return(v2_find_adjacent(fd, key, file_id, record_offset, hint, matched_key,
		cursor, true));
}

bool index_v2_prior(int fd, const void *key, uint16_t *file_id,
		uint64_t *record_offset, const INDEX_V2_CURSOR *hint, void *matched_key,
		INDEX_V2_CURSOR *cursor)
{
	return(v2_find_adjacent(fd, key, file_id, record_offset, hint, matched_key,
		cursor, false));
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
