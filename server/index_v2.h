/* ***************************************************************
 *
 * PROCEDURE:	srv_index.h
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
 *				Mon Jul 20 08:40:14 PM MDT 2026
 *				tomg
 *				Added this file for V2 indexing.  This implements
 *				the same basic ideas of the V1 index, but does it
 *				as a copy on write.
 *
 ************************************************************* */

/*
 * On-disk format for copy-on-write dataman indexes.
 *
 * All multi-byte fields are big-endian.  Every node occupies one fixed-size
 * page; unused bytes are zero.  Nodes are append-only until reclaimed. Readers
 * select the highest-generation valid root slot and never follow parent
 * pointers; a writer retains the descent path in memory while publishing a
 * replacement root.
 */
#ifndef DATAMAN_INDEX_V2_H
#define DATAMAN_INDEX_V2_H

#include <stdint.h>
#include <stdbool.h>

#define INDEX_V2_MAGIC          "DMIDXV2"
#define INDEX_V2_VERSION        2
#define INDEX_V2_HEADER_SIZE    128
#define INDEX_V2_ROOT_SLOTS     2
#define INDEX_V2_ROOT_SLOT_SIZE 32

/* Index file header byte offsets. */
#define INDEX_V2_MAGIC_OFFSET        0
#define INDEX_V2_VERSION_OFFSET      8
#define INDEX_V2_HEADER_SIZE_OFFSET  10
#define INDEX_V2_KEYLEN_OFFSET       12
#define INDEX_V2_FILE_COUNT_OFFSET   14
#define INDEX_V2_NAMES_LENGTH_OFFSET 16
#define INDEX_V2_PAGE_SIZE_OFFSET    20
#define INDEX_V2_FLAGS_OFFSET        22
#define INDEX_V2_BUILD_ROOT_OFFSET   24
#define INDEX_V2_SLOT0_OFFSET        32
#define INDEX_V2_SLOT1_OFFSET        (INDEX_V2_SLOT0_OFFSET + INDEX_V2_ROOT_SLOT_SIZE)

#define INDEX_V2_FLAG_BUILDING       0x0001

/* Root-slot byte offsets. */
#define INDEX_V2_SLOT_GENERATION_OFFSET 0
#define INDEX_V2_SLOT_ROOT_OFFSET       8
#define INDEX_V2_SLOT_ROOT_CRC_OFFSET   16
#define INDEX_V2_SLOT_CRC_OFFSET        20

/* Node header byte offsets. */
#define INDEX_V2_NODE_MAGIC             "DMN2"
#define INDEX_V2_NODE_HEADER_SIZE       16
#define INDEX_V2_LEGACY_PAGE_SIZE       1024
#define INDEX_V2_NODE_MAGIC_OFFSET      0
#define INDEX_V2_NODE_FLAGS_OFFSET      4
#define INDEX_V2_NODE_KEY_COUNT_OFFSET  5
#define INDEX_V2_NODE_PAYLOAD_OFFSET    8
#define INDEX_V2_NODE_CRC_OFFSET        12

#define INDEX_V2_NODE_LEAF              0x01

#define INDEX_V2_FILE_ID_SIZE           2
#define INDEX_V2_RECORD_OFFSET_SIZE     8
#define INDEX_V2_MAX_KEYS               12

/*
 * Leaf payload: key_count repetitions of
 *     key[keylen], file_id (uint16_t), record_offset (uint64_t).
 * Internal payload: child_offset[0..key_count], followed by key_count
 * separators.  A separator has the same full composite representation as a
 * leaf entry and is the first entry in its right child; equality descends to
 * that right child.
 */
typedef struct index_v2_node {
	uint8_t flags;
	uint8_t key_count;
	uint32_t payload_length;
	uint32_t crc;
	unsigned char *payload;
} INDEX_V2_NODE;

typedef struct index_v2_cursor {
	uint64_t generation;
	uint64_t node_offset;
	uint8_t entry_index;
} INDEX_V2_CURSOR;

uint32_t index_v2_crc32(const void *buf, uint32_t len);
uint16_t index_v2_page_size(uint16_t keylen);
bool index_v2_read_page_size(int fd, uint16_t keylen, uint16_t *page_size);
bool index_v2_init_header(int fd, uint16_t keylen, uint16_t file_count);
bool index_v2_create_empty(int fd, uint16_t keylen, uint16_t file_count,
					const char *const *file_names);
bool index_v2_build_begin(int fd, uint16_t keylen, uint16_t file_count,
					const char *const *file_names, uint64_t *root_offset);
bool index_v2_build_insert(int fd, const void *key, uint16_t file_id,
					uint64_t record_offset, uint64_t *root_offset);
bool index_v2_build_finish(int fd, uint64_t *root_offset);
bool index_v2_read_header(int fd, uint16_t *keylen, uint16_t *file_count,
						uint64_t *root_offset, uint32_t *root_crc,
						uint64_t *generation);
bool index_v2_read_file_names(int fd, uint16_t file_count, char ***file_names);
bool index_v2_publish_root(int fd, uint64_t root_offset, uint32_t root_crc,
						uint64_t generation);
bool index_v2_insert(int fd, const void *key, uint16_t file_id,
					uint64_t record_offset, INDEX_V2_CURSOR *cursor, uint64_t *root_offset);
bool index_v2_remove(int fd, const void *key, uint16_t file_id,
					uint64_t record_offset);
bool index_v2_find(int fd, const void *key, bool exact,
					uint16_t *file_id, uint64_t *record_offset, void *matched_key,
					INDEX_V2_CURSOR *cursor);
bool index_v2_read_node(int fd, uint64_t offset, INDEX_V2_NODE *node);
void index_v2_free_node(INDEX_V2_NODE *node);
bool index_v2_first(int fd, uint16_t *file_id, uint64_t *record_offset, void *matched_key, INDEX_V2_CURSOR *cursor);
bool index_v2_last(int fd, uint16_t *file_id, uint64_t *record_offset, void *matched_key, INDEX_V2_CURSOR *cursor);
bool index_v2_next(int fd, const void *key, uint16_t *file_id,
					uint64_t *record_offset, const INDEX_V2_CURSOR *hint,
					void *matched_key, INDEX_V2_CURSOR *cursor);
bool index_v2_prior(int fd, const void *key, uint16_t *file_id,
					uint64_t *record_offset, const INDEX_V2_CURSOR *hint,
					void *matched_key, INDEX_V2_CURSOR *cursor);

# endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
