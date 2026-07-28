/* ***************************************************************
 *
 * PROCEDURE:	dumpix.c
 *
 * PROJECT:		dataman system utilities
 * 
 * DATE:		legacy, written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 *				Tue Jul 21 12:20:23 PM MDT 2026
 *				tomg
 *				changing the structure of the index file so that it is
 *				more resilient.  using CopyOnWrite strategy, changing
 *				the node structure so that all of the final keys are
 *				contained in the leaf.  The original format was doing
 *				anything to optomize disk space.  That's not really an
 *				issue any more.
 *				this is dataman version 4.0.0 compliant
 *
 ************************************************************* */

/*
 * this routine does a dump of an index file to the standard output
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The GNU General Public License is contained in the file COPYING.
 */

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>

#include "misc.h"
#include "index_v2.h"

#define LEAF    0200                    /* the leaf bit mask */

char name[32];							/* name of index */
int pos[999];							/* the positions in the leve number */
int level;								/* our level in the index */
int chan;								/* channel of index */
int loop,count,tmp,acc,bytes,idx;		/* misc usage */
int64_t offs;							/* lseek offset */
int64_t cur_node;						/* posittion of current node */
int64_t prnt_node;						/* position of parent node */
short keylen;							/* internal length of key */
short fno;								/* number of files */
char **fnames;							/* files the index point to */
char file;

int cntr;

extern char *substr(char *, int ,int);	/* substring function */
extern short get_short(char *);
extern int64_t get_ll(void *);

static void usage(void);
static void all_done(void);
static int dump_v2(int);

typedef struct v2_dump_context {
	int fd;
	uint16_t keylen;
	uint16_t file_count;
	uint16_t page_size;
	char **file_names;
	uint64_t file_size;
	uint64_t node_start;
	uint64_t *seen;
	size_t seen_count;
	size_t seen_size;
	int leaf_keys;
} V2_DUMP_CONTEXT;

static uint16_t v2_get_u16(const unsigned char *);
static uint64_t v2_get_u64(const unsigned char *);
static int v2_dump_node(V2_DUMP_CONTEXT *, uint64_t, int,
					unsigned char **, unsigned char **);
static int v2_mark_seen(V2_DUMP_CONTEXT *, uint64_t);
static void v2_free_names(char **, uint16_t);

int main(int argc, char *argv[])

{

	int i;

	char path[512];
	char stuff[1024];						/* output buffer */
	char *root;
	char *key;								/* key to write out */

	root = NULL;
	if (argc != 2 && argc != 4)
		usage();

	if (argc == 4) {
	   	if (strcmp("-r", argv[1]))
			usage();
		root = strdup(argv[2]);
		strcpy(name, argv[3]);
	} else {
		strcpy(name, argv[1]);
		if (*name == '-')
			usage();
	}

	if (!root && (root =  getenv("ROOT")) == NULL) {		/* get root path name */
		fprintf(stderr, "ROOT not defined!\n");
		exit(-1);
	}
	strcpy(path, root);
	strcat(path,"/index/");		/* tack on index sub dir */
	strcat(path,name);			/* tack on the index name */

	if ((chan = open(path,O_RDONLY|O_LARGEFILE)) < 0) {
		fprintf(stderr, "Can't open index named %s ", path);
		perror("");
		exit(errno);
	}
	if (pread(chan, stuff, strlen(INDEX_V2_MAGIC), 0) ==
			(ssize_t)strlen(INDEX_V2_MAGIC) &&
			memcmp(stuff, INDEX_V2_MAGIC, strlen(INDEX_V2_MAGIC)) == 0)
		return(dump_v2(chan));
	fprintf(stderr, "Cannot dump legacy or invalid index %s; rebuild it with dbclean -i\n", path);
	close(chan);
	return(EXIT_FAILURE);
}

static uint16_t v2_get_u16(const unsigned char *ptr)
{
	return((uint16_t)((ptr[0] << 8) | ptr[1]));
}

static uint64_t v2_get_u64(const unsigned char *ptr)
{
	uint64_t value = 0;
	int i;

	for (i = 0; i < 8; i++)
		value = (value << 8) | ptr[i];
	return(value);
}

static int v2_mark_seen(V2_DUMP_CONTEXT *context, uint64_t offset)
{
	size_t i;
	uint64_t *new_seen;

	for (i = 0; i < context->seen_count; i++)
		if (context->seen[i] == offset) {
			fprintf(stderr, "v2 index is not a tree: node at %" PRIu64
					" is referenced more than once\n", offset);
			return(0);
		}
	if (context->seen_count == context->seen_size) {
		size_t new_size = context->seen_size ? context->seen_size * 2 : 64;
		new_seen = realloc(context->seen, new_size * sizeof(*new_seen));
		if (new_seen == NULL) {
			fprintf(stderr, "Can't allocate v2 node validation space\n");
			return(0);
		}
		context->seen = new_seen;
		context->seen_size = new_size;
	}
	context->seen[context->seen_count++] = offset;
	return(1);
}

static int v2_dump_node(V2_DUMP_CONTEXT *context, uint64_t offset, int level,
					unsigned char **first_key, unsigned char **last_key)
{
	INDEX_V2_NODE node;
	uint32_t expected;
	uint32_t child_bytes;
	unsigned char *first = NULL;
	unsigned char *last = NULL;
	uint16_t file_id;
	uint64_t record_offset;
	uint8_t i;

	*first_key = NULL;
	*last_key = NULL;
	if (offset < context->node_start || (offset % context->page_size) != 0 ||
			offset > context->file_size - context->page_size ||
			!v2_mark_seen(context, offset) ||
			!index_v2_read_node(context->fd, offset, &node)) {
		fprintf(stderr, "Invalid v2 node at %" PRIu64 "\n", offset);
		return(0);
	}
	if (node.key_count > INDEX_V2_MAX_KEYS) {
		fprintf(stderr, "v2 node at %" PRIu64 " has too many keys (%u)\n",
				offset, node.key_count);
		goto bad;
	}
	if (node.flags & INDEX_V2_NODE_LEAF) {
		expected = (uint32_t)node.key_count * (context->keylen + INDEX_V2_FILE_ID_SIZE + INDEX_V2_RECORD_OFFSET_SIZE);
		if (node.payload_length != expected)
			goto malformed;

		for (i = 1; i < node.key_count; i++) {
			if (memcmp(node.payload + (size_t)(i - 1) * (context->keylen + 10),
					node.payload + (size_t)i *(context->keylen + 10),
					context->keylen + 10) >= 0) {
				fprintf(stderr, "v2 leaf at %" PRIu64 " is not sorted\n", offset);
				goto bad;
			}
		}

		for (i = 0; i < node.key_count; i++) {
			unsigned char *entry = node.payload + (size_t)i * (context->keylen + 10);
			file_id = v2_get_u16(entry + context->keylen);
			record_offset = v2_get_u64(entry + context->keylen + 2);

			if (file_id >= context->file_count) {
				fprintf(stderr, "v2 leaf at %" PRIu64 " has invalid file id %u\n", offset, file_id);
				goto bad;
			}

			printf("key %.*s, file %s, pointer %" PRIu64 "\n",
				(int)context->keylen, entry, context->file_names[file_id], record_offset);
			context->leaf_keys++;
		}
		if (node.key_count != 0) {
			first = malloc(context->keylen + 10);
			last = malloc(context->keylen + 10);
			if (first == NULL || last == NULL)
				goto bad;
			memcpy(first, node.payload, context->keylen + 10);
			memcpy(last, node.payload + (size_t)(node.key_count - 1) *
					(context->keylen + 10), context->keylen + 10);
		}
	} else {
		child_bytes = (uint32_t)(node.key_count + 1) * 8;
		expected = child_bytes + (uint32_t)node.key_count *
			(context->keylen + INDEX_V2_FILE_ID_SIZE + INDEX_V2_RECORD_OFFSET_SIZE);
		if (node.key_count == 0 || node.payload_length != expected)
			goto malformed;

		for (i = 1; i < node.key_count; i++) {
			if (memcmp(node.payload + child_bytes + (size_t)(i - 1) *
					(context->keylen + 10), node.payload + child_bytes + (size_t)i *
					(context->keylen + 10), context->keylen + 10) >= 0) {
				fprintf(stderr, "v2 internal node at %" PRIu64 " is not sorted\n", offset);
				goto bad;
			}
		}

		for (i = 0; i <= node.key_count; i++) {
			unsigned char *child_first;
			unsigned char *child_last;
			uint64_t child = v2_get_u64(node.payload + (size_t)i * 8);
			if (!v2_dump_node(context, child, level + 1, &child_first, &child_last))
				goto bad;
			if (child_first == NULL || child_last == NULL) {
				free(child_first);
				free(child_last);
				fprintf(stderr, "v2 internal node at %" PRIu64 " has an empty child\n", offset);
				goto bad;
			}
			if (i != 0 && (memcmp(child_first, node.payload + child_bytes +
					(size_t)(i - 1) * (context->keylen + 10), context->keylen + 10) != 0 ||
					memcmp(last, child_first, context->keylen + 10) >= 0)) {
				free(child_first);
				free(child_last);
				fprintf(stderr, "v2 separator mismatch at node %" PRIu64 "\n", offset);
				goto bad;
			}
			if (i == 0)
				first = child_first;
			else
				free(child_first);
			free(last);
			last = child_last;
		}
	}
	index_v2_free_node(&node);
	*first_key = first;
	*last_key = last;
	return(1);

malformed:
	fprintf(stderr, "v2 node at %" PRIu64 " has an invalid payload length\n", offset);
bad:
	free(first);
	free(last);
	index_v2_free_node(&node);
	return(0);
}

static void v2_free_names(char **file_names, uint16_t file_count)
{
	uint16_t i;

	if (file_names != NULL) {
		for (i = 0; i < file_count; i++)
			free(file_names[i]);
		free(file_names);
	}
}

static int dump_v2(int fd)
{
	V2_DUMP_CONTEXT context;
	struct stat status;
	uint64_t root;
	uint32_t root_crc;
	uint64_t generation;
	INDEX_V2_NODE root_node;
	unsigned char *first;
	unsigned char *last;
	uint32_t names_length;
	unsigned char header[INDEX_V2_HEADER_SIZE];

	memset(&context, 0, sizeof(context));
	if (!index_v2_read_header(fd, &context.keylen, &context.file_count, &root, &root_crc, &generation) ||
			!index_v2_read_file_names(fd, context.file_count, &context.file_names) ||
			fstat(fd, &status) != 0 || pread(fd, header, sizeof(header), 0) != (ssize_t)sizeof(header)) {
		fprintf(stderr, "Invalid v2 index header\n");
		v2_free_names(context.file_names, context.file_count);
		return(1);
	}
	if (!index_v2_read_page_size(fd, context.keylen, &context.page_size)) {
		fprintf(stderr, "Invalid v2 page size\n");
		v2_free_names(context.file_names, context.file_count);
		return(1);
	}
	names_length = ((uint32_t)header[INDEX_V2_NAMES_LENGTH_OFFSET] << 24) |
		((uint32_t)header[INDEX_V2_NAMES_LENGTH_OFFSET + 1] << 16) |
		((uint32_t)header[INDEX_V2_NAMES_LENGTH_OFFSET + 2] << 8) |
		header[INDEX_V2_NAMES_LENGTH_OFFSET + 3];
	context.fd = fd;
	context.file_size = (uint64_t)status.st_size;
	context.node_start = ((INDEX_V2_HEADER_SIZE + names_length +
		context.page_size - 1) / context.page_size) * context.page_size;
	if (!index_v2_read_node(fd, root, &root_node) || root_node.crc != root_crc) {
		fprintf(stderr, "v2 root slot does not match its root node\n");
		index_v2_free_node(&root_node);
		v2_free_names(context.file_names, context.file_count);
		return(1);
	}
	index_v2_free_node(&root_node);
	printf("Index v2: %u dataset%s, key length %u, generation %" PRIu64 "\n",
		context.file_count, context.file_count == 1 ? "" : "s", context.keylen,
		generation);
	if (!v2_dump_node(&context, root, 0, &first, &last)) {
		v2_free_names(context.file_names, context.file_count);
		free(context.seen);
		return(1);
	}
	free(first);
	free(last);
	printf("Keys dumped: %d; nodes validated: %zu\n", context.leaf_keys, context.seen_count);
	v2_free_names(context.file_names, context.file_count);
	free(context.seen);
	return(0);
}

static void usage(void)
{
	fprintf(stderr, "Usage: dumpix [-r root] idx_name\n");
	fprintf(stderr, "    -r database_root_directory\n");
	exit(-1);
}

static void all_done(void)                      /* this is if finished with endex */
{
	fprintf(stderr,"Keys dumped: %d\nNormal termination\n",cntr);
	exit(0);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
