/* Restore a saved logical index position using an exact v2 lookup. */
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

int restore(char *cmd, int c_off, char **ret)
{
	INDEX *idx;
	INDEX_V2_CURSOR cursor;
	unsigned char matched_key[MAX_KEY_SIZE];
	char system_key[KEY_BUFFER_SIZE];
	char *field, *key, *rptr = NULL;
	uint64_t index_record, saved_record;
	uint16_t file_id;
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

	/* Skip the saved node and entry fields; v2 validates via the key. */
	if ((field = strchr(cmd + c_off, '|')) == NULL ||
			(field = strchr(field + 1, '|')) == NULL ||
			(field = strchr(field + 1, '|')) == NULL)
		return(EINVMSG);
	saved_record = strtoull(field + 1, NULL, 0);
	if ((key = strchr(field + 1, '|')) == NULL)
		return(EINVMSG);
	key++;
	if ((unsigned char)key[idx->_keylen] == 0)
		return(EINVMSG);
	file_id = (uint16_t)(unsigned char)key[idx->_keylen] - 1;
	if (file_id >= (uint16_t)idx->_f_cnt)
		return(EINVMSG);
	index_record = (uint64_t)get_ll(key + idx->_keylen + 1);

	fl_lock(&idx->_lock, LOCK_SH);
	if (!index_v2_find(idx->_idxchan, key, true, &file_id, &index_record,
			matched_key, &cursor)) {
		result = 0;
		goto done;
	}
	result = upd_idx(idx, matched_key, file_id, saved_record, &cursor, cmd,
		&rptr);
	if (result == 0) {
		memcpy(system_key, matched_key, idx->_keylen);
		system_key[idx->_keylen] = (char)(file_id + 1);
		put_ll(system_key + idx->_keylen + 1, (int64_t)index_record);
		fl_lock(&idx->_lock, LOCK_UN);
		rm_key(idxno, NOXACT, system_key);
		fl_lock(&idx->_lock, LOCK_SH);
	}

done:
	fl_lock(&idx->_lock, LOCK_UN);
	*ret = rptr;
	return(result);
}

/* vim: set noet sw=4 sts=4 ts=4 fdm=marker: */
