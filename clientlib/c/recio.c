/* ***************************************************************
 *
 * PROCEDURE:	recio.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		legacy, originally writtin in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				March 2002
 *				Tom Green
 *				modified to use call interface to server
 ************************************************************* */

/*
 * this routine reads in the master file current record and allocates each
 * field in that record.  field zero is never allocated.
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

#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/ioctl.h>

#include "m_params.h"
#include "globs.h"
#include "client_internal.h"
#include "../../server/dbfunc.h"
#include "../../server/datafile_header.h"
#include "../../server/errors.h"

extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);

extern uint32_t get_long(char *);
extern void put_long(char *, uint32_t);

extern int dm_sock;
extern char *_progname;

#define FALSE 0
#define TRUE  1

static void free_fdesc(FILEDESC *fdesc)
{
	int i;

	if (!fdesc)
		return;
	for (i = 0; i < fdesc->n_rformats; i++)
		free(fdesc->record_desc[i].field_sizes);
	free(fdesc->record_desc);
	free(fdesc);
}

static int in_rec_impl(int type, char *buff, size_t buff_len, INDEX *idx,
		int fmt, int chan, int reload_desc)
{
	int tmp,i,j;               /* misc usage */
	int allocated_fields = 0;

	int len,				/* length of record */
		b_offs;				/* offset to blob in record */

	uint32_t b_len;

    short *desc;			/* point at the unparsed file description */

	char **old_flds;			/* pointers to the fields that are going away*/
	char **new_flds = NULL;		/* pointers to the fields that are being read*/
	uint32_t *old_blob_lengths;
	uint32_t *new_blob_lengths = NULL;
	char string[128];
	char *cptr = NULL;
	char *desc_buff = NULL;
	char *tptr;

	int retval = TRUE;

	FILEDESC *fdesc;		/* the parsed file description */
	FILEDESC *old_fdesc;
	FILEDESC *new_fdesc = NULL;
	RFDESC *rfdesc;
	if (type == MASTER) {
		if (!idx || chan < 0 || chan >= idx->_nfiles) {
			retval = FALSE;
			goto fail;
		}
		old_fdesc = idx->_files[chan]._filedesc;
		fdesc = old_fdesc;
		old_flds = mfld;
		old_blob_lengths = m_blob_lengths;
	} else {
		old_fdesc = w_fdesc;
		fdesc = reload_desc ? NULL : old_fdesc;
		old_flds = wfld;
		old_blob_lengths = w_blob_lengths;
	}

/*
 * do we need to now manually retrieve the file description?
 */
	if (fdesc == NULL) {
		if (type == MASTER)
			sprintf(string, "%d|%d|%d|", GET_DESC, idx->_idxno, chan);
		else
			sprintf(string, "%d|%d|", GET_DESC, -chan);
		desc_buff = db_send(string, strlen(string), __FILE__);

		if (!desc_buff) {
			retval = FALSE;
			goto fail;
		}

		len = atoi(desc_buff);
		if (len < 0) {
			db_err(len, "%s: get_desc failed", _progname);
			retval = FALSE;
			goto fail;
		}

		tptr = desc_buff;
		if (!dm_next_field(&tptr))
			goto invalid_response;
		if (!dm_next_field(&tptr))
			goto invalid_response;
		desc = (short *)tptr;
		new_fdesc = calloc(1, sizeof(FILEDESC));
		fdesc = new_fdesc;

		if (!fdesc) {
			retval = FALSE;
			goto fail;
		}

		fdesc->header_len = len;
		fdesc->n_rformats = *desc;
		fdesc->record_desc = calloc(fdesc->n_rformats, sizeof(RFDESC));

		if (!fdesc->record_desc) {
			retval = FALSE;
			goto fail;
		}

		i = 1;
		for (tmp = 0; tmp < fdesc->n_rformats; tmp++) {
			rfdesc = fdesc->record_desc+tmp;
			rfdesc->n_fields = *(desc+i);
			i++;
			rfdesc->rf_len = *(desc+i);
			rfdesc->field_sizes = (short *)calloc(rfdesc->n_fields, sizeof(short));
			if (!rfdesc->field_sizes) {
				retval = FALSE;
				goto fail;
			}
			for (++i, j = 0; j < rfdesc->n_fields; j++, i++) {
				rfdesc->field_sizes[j] = *(desc+i);
				if (*(desc+i) == 0)
					rfdesc->has_blob++;
			}
		}

		free(desc_buff);
		desc_buff = NULL;
	}

	if (!buff || !fdesc || fmt < 1 || fmt > fdesc->n_rformats) {
		retval = FALSE;
		goto fail;
	}
	rfdesc = fdesc->record_desc+fmt-1;
	if (rfdesc->rf_len < 0 || (size_t)rfdesc->rf_len > buff_len) {
		retval = FALSE;
		goto fail;
	}
	new_flds = (char **)calloc((rfdesc->n_fields)+2, sizeof(char *));
	new_blob_lengths = calloc(rfdesc->n_fields, sizeof(*new_blob_lengths));
/*
 * if this allocate has failed we can just fail without freeing up any
 * file description we might have read in above.  it's fine.  This is
 * just the actual array of fields that failed
 */
	if (!new_flds || !new_blob_lengths) {
		retval = FALSE;
		goto fail;
	}

    *new_flds = NULL;						/* zero the dirty bit */
    *(new_flds+rfdesc->n_fields+1) = NULL;	/* end of record indicator */
	b_offs = rfdesc->rf_len;
/*
 * save each data field.  if the length of the datafield is 0
 * it is a blob.  blobs are tacked on the end of the record
 * format that we do know the length of.  blobs have a 4 byte
 * network byte order length then the data.  save the length
 * as a negative number so when we send the data back, we know
 * it is a blob, and reset the length to 0
 */
	j = 0;
	for (i = 0; i < rfdesc->n_fields; i++) {
		if (rfdesc->field_sizes[i] <= 0) {
			if ((size_t)b_offs > buff_len ||
					buff_len - (size_t)b_offs < sizeof(uint32_t)) {
				retval = FALSE;
				goto fail;
			}
			b_len = get_long(buff+b_offs);
			b_offs += sizeof(int32_t);
			if ((size_t)b_len > buff_len - (size_t)b_offs) {
				retval = FALSE;
				goto fail;
			}
			cptr = malloc((size_t)b_len+1);
			if (!cptr) {
				retval = FALSE;
				goto fail;
			}
			memcpy(cptr, buff+b_offs, b_len);
			cptr[b_len] = '\0';
			new_blob_lengths[i] = b_len;
			b_offs += (int)b_len;
		} else {
			if (j < 0 || j > rfdesc->rf_len ||
					rfdesc->field_sizes[i] > rfdesc->rf_len - j) {
				retval = FALSE;
				goto fail;
			}
			cptr = calloc(1, rfdesc->field_sizes[i]+1);
			if (!cptr) {
				retval = FALSE;
				goto fail;
			}
			memcpy(cptr, buff+j, rfdesc->field_sizes[i]);
			j += rfdesc->field_sizes[i];
		}
/*
 * if this allocate has failed, just free the fields we've made.  there
 * is nothing wrong with the file description.  we can carry that around
 */
		new_flds[i+1] = cptr;
		cptr = NULL;
		allocated_fields++;
	}
	if (j != rfdesc->rf_len || (size_t)b_offs != buff_len) {
		retval = FALSE;
		goto fail;
	}
/*
 * get rid of the prior recrd
 */
	if (old_flds) {
		for (i = 1; old_flds[i]; i++)
			free(old_flds[i]);
		free(old_flds);
	}
	free(old_blob_lengths);

	/* Publish a newly loaded description only with its completed record. */
	if (new_fdesc) {
		if (type == MASTER) {
			idx->_files[chan]._hlen = new_fdesc->header_len;
			idx->_files[chan]._filedesc = new_fdesc;
		} else {
			w_fdesc = new_fdesc;
			if (reload_desc)
				free_fdesc(old_fdesc);
		}
		new_fdesc = NULL;
	}

	if (type == MASTER) {
		mfld = new_flds;
		m_blob_lengths = new_blob_lengths;
	} else {
		wfld = new_flds;
		w_blob_lengths = new_blob_lengths;
    }
	new_blob_lengths = NULL;

fail:
	free(desc_buff);
	free(new_blob_lengths);
	if (new_flds && retval == FALSE) {
		for (i = 1; i <= allocated_fields; i++)
			free(new_flds[i]);
		free(new_flds);
	}
	if (new_fdesc) {
		if (new_fdesc->record_desc) {
			for (i = 0; i < new_fdesc->n_rformats; i++)
				free(new_fdesc->record_desc[i].field_sizes);
			free(new_fdesc->record_desc);
		}
		free(new_fdesc);
	}
	return retval;

invalid_response:
	db_err(EINVMSG, "%s: invalid IN_REC response", _progname);
	retval = FALSE;
	goto fail;
}

int in_rec(int type, char *buff, size_t buff_len, INDEX *idx, int fmt, int chan)
{
	return in_rec_impl(type, buff, buff_len, idx, fmt, chan, FALSE);
}

int dm_in_rec_reload(int type, char *buff, size_t buff_len, INDEX *idx,
		int fmt, int chan)
{
	return in_rec_impl(type, buff, buff_len, idx, fmt, chan, TRUE);
}

/*
 * this procedure re writes the current record that is being used to
 * its palce in it's particular file.
 * just flush/output the record.  this is so we don't have to re-read
 * it if the calling function (like forward or back) didn't return
 * true.
 */

#define MSK	077
int out_rec(int type)

{
	int i;
	int idx;		/* loop counter */
	int tmp;		/* more counter */
	int fmt,
		chan,
		r_len,
		val;

	int64_t cur;
	uint64_t b_size;		/* size of rec including blob data */
	uint32_t blob_len;

//	short *desc;
	FILEDESC *desc;
	RFDESC *rfdesc;

	char *rec_buff;
	char **flds;
	char *nbuf;
	uint32_t *blob_lengths;

/*
 * if mfld is equal to zero (null) there has never been an assignment to
 * any of the fields and this procedure would invalidate any record
 * there will allways be a work field
 */
	if (type == MASTER) {
		if (mfld == NULL)
			return TRUE;
		fmt = m_fmt & MSK;
		cur = m_cur;
		chan = m_chan;
		desc = m_fdesc;
		flds = mfld;
		blob_lengths = m_blob_lengths;
		idx = cur_index._idxno;
	} else {
		fmt = w_fmt & MSK;
		cur = w_cur;
		chan = 0;
		desc = w_fdesc;
		flds = wfld;
		blob_lengths = w_blob_lengths;
		idx = -w_chan;
	}
	if (!flds)
		return TRUE;
	if (!*flds)
		return TRUE;
	if (!desc || fmt < 1 || fmt > desc->n_rformats)
		return FALSE;

	rfdesc = desc->record_desc+fmt-1;
	r_len = rfdesc->rf_len;

	b_size = r_len;
	for (i = 0, tmp = 0; tmp < rfdesc->has_blob && i < rfdesc->n_fields; i++) {
		if (rfdesc->field_sizes[i] <= 0) {
			blob_len = blob_lengths ? blob_lengths[i] : 0;
			b_size += (uint64_t)blob_len + sizeof(uint32_t);
			if (b_size > INT32_MAX)
				return FALSE;
			tmp++;
		}
	}
	rec_buff = malloc((size_t)b_size+64);

	if (!rec_buff)
		return FALSE;

	i = sprintf(rec_buff, "%d|%d|%d|%"PRId64"|%d|%"PRIu64"|",
			FLUSH, idx, chan, cur, fmt, b_size);
	val = i + r_len;

	for (tmp = 0; tmp < rfdesc->n_fields; tmp++) {
		if (rfdesc->field_sizes[tmp] <= 0) {
			blob_len = blob_lengths ? blob_lengths[tmp] : 0;
			put_long(rec_buff+val, blob_len);
			val += sizeof(int32_t);
			memcpy(rec_buff+val, flds[tmp+1], blob_len);
		    val += (int)blob_len;
		} else {
			memcpy(rec_buff+i, flds[tmp+1], rfdesc->field_sizes[tmp]);
			i += rfdesc->field_sizes[tmp];
		}
	}

	nbuf = db_send(rec_buff, val, __FILE__);

	if (!nbuf) {
		free (rec_buff);
		return FALSE;
	}

	i = atoi(nbuf);
	if (i < 1) {
		if (i < 0)
			db_err(i, "%s: error in out_rec", _progname);
		i = FALSE;
	}

fail:
	if (i) {
		*flds = NULL;
	}
	if (rec_buff)
		free(rec_buff);
	if (nbuf)
		free(nbuf);
	return i;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
