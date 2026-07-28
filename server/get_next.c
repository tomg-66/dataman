/* Return the entry following the client's generation-qualified v2 cursor. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "index_v2.h"
#include "srv_index.h"
#include "lock.h"
#include "errors.h"
#include "misc.h"

extern int idx_cnt;
extern INDEX *_indices;

extern void rm_key(int, int, char *);
extern int64_t get_ll(void *);
extern void put_ll(void *, int64_t);
extern int upd_idx(INDEX *, const unsigned char *, uint16_t, uint64_t,
		const INDEX_V2_CURSOR *, char *, char **);

int get_next(char *cmd, int c_off, char **ret)
{
	INDEX *idx;
	INDEX_V2_CURSOR hint, cursor, ignored_cursor;
	unsigned char matched_key[MAX_KEY_SIZE];
	unsigned char ignored_key[MAX_KEY_SIZE];
	char system_key[KEY_BUFFER_SIZE];
	char *field, *key, *rptr = NULL;
	uint64_t current_record, record_offset;
	uint16_t current_file, file_id, ignored_file;
	int idxno, result;

	*ret = NULL;
	idxno = atoi(cmd + c_off);
	if (idxno < 0 || idxno >= idx_cnt)
		return(EINVMSG);
	if (_indices == NULL)
		return(ENOINDEX);
	idx = _indices + idxno;
	if (!idx->_refcnt)
		return(EIDXNOO);
	if ((field = strchr(cmd + c_off, '|')) == NULL)
		return(EINVMSG);
	hint.generation = strtoull(++field, NULL, 0);
	if ((field = strchr(field, '|')) == NULL)
		return(EINVMSG);
	hint.node_offset = strtoull(++field, NULL, 0);
	if ((field = strchr(field, '|')) == NULL)
		return(EINVMSG);
	hint.entry_index = (uint8_t)strtoul(++field, NULL, 0);
	if ((key = strchr(field, '|')) == NULL)
		return(EINVMSG);
	key++;
	if ((unsigned char)key[idx->_keylen] == 0)
		return(EINVMSG);
	current_file = (uint16_t)(unsigned char)key[idx->_keylen] - 1;
	if (current_file >= (uint16_t)idx->_f_cnt)
		return(EINVMSG);
	current_record = (uint64_t)get_ll(key + idx->_keylen + 1);

	fl_lock(&idx->_lock, LOCK_SH);
	for (;;) {
		file_id = current_file;
		record_offset = current_record;
		if (!index_v2_next(idx->_idxchan, key, &file_id, &record_offset, &hint,
				matched_key, &cursor)) {
			ignored_file = current_file;
			record_offset = current_record;
			result = index_v2_find(idx->_idxchan, key, true, &ignored_file,
				&record_offset, ignored_key, &ignored_cursor) ? 0 : ERMKEY;
			break;
		}
		result = upd_idx(idx, matched_key, file_id, record_offset, &cursor, cmd,
			&rptr);
		if (result != 0)
			break;
		memcpy(system_key, matched_key, idx->_keylen);
		system_key[idx->_keylen] = (char)(file_id + 1);
		put_ll(system_key + idx->_keylen + 1, (int64_t)record_offset);
		fl_lock(&idx->_lock, LOCK_UN);
		rm_key(idxno, NOXACT, system_key);
		fl_lock(&idx->_lock, LOCK_SH);
	}
	fl_lock(&idx->_lock, LOCK_UN);
	*ret = rptr;
	return(result);
}

/* vim: set noet sw=4 sts=4 ts=4 fdm=marker: */
