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

#include <sys/types.h>
#include <sys/ioctl.h>


#include "m_params.h"
#include "globs.h"
#include "../../server/dbfunc.h"
#include "../../server/datafile_header.h"

extern INDEX *findex(char *);
extern char *db_send(char *, int, char *);
extern void db_err(int, char *, ...);

extern int32_t get_long(char *);
extern void put_long(char *, int32_t);

extern int dm_sock;
extern char *_progname;

#define FALSE 0
#define TRUE  1

int in_rec(int type, char *buff)
{
	int idx,i,j;               /* misc usage */

	int fmt,				/* the working format number */
		len,				/* length of record */
		chan,
		b_offs;				/* offset to blob in record */

	unsigned long b_len;

    short *desc;			/* point at the unparsed file description */

	char **old_flds;			/* pointers to the fields that are going away*/
	char **new_flds = NULL;		/* pointers to the fields that are being read*/
	short *new_sizes = NULL;
	char string[128];
	char *cptr = NULL;
	char *desc_buff = NULL;
	char *tptr;

	int retval = TRUE;

	FILEDESC *fdesc;		/* the parsed file description */
	FILEDESC *new_fdesc = NULL;
	RFDESC *rfdesc;
	INDEX *master_idx = NULL;

	if (type == MASTER) {
		chan = m_chan;
		fdesc = m_fdesc;
		fmt = m_fmt;
		old_flds = mfld;
	} else {
		chan = -w_chan;
		fdesc = w_fdesc;
		fmt = w_fmt;
		old_flds = wfld;
	}

/*
 * do we need to now manually retrieve the file description?
 */
	if (fdesc == NULL) {
		if (type == MASTER)
			sprintf(string, "%d|%d|%d|", GET_DESC, cur_index._idxno, chan);
		else
			sprintf(string, "%d|%d|", GET_DESC, chan);
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

		tptr = strchr(desc_buff, '|') + 1;
		tptr = strchr(tptr, '|') + 1;
		desc = (short *)tptr;
		if (type == MASTER) {
			if ((master_idx = findex(cur_index._idxname)) == NULL) {
				retval = FALSE;
				goto fail;
			}
		}
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
		for (idx = 0; idx < fdesc->n_rformats; idx++) {
			rfdesc = fdesc->record_desc+idx;
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

		/* Do not publish a descriptor until it is completely constructed. */
		if (type == MASTER) {
			master_idx->_files[master_idx->_fno]._hlen = m_head = len;
			master_idx->_files[master_idx->_fno]._filedesc = new_fdesc;
			m_fdesc = new_fdesc;
		} else {
			w_fdesc = new_fdesc;
		}
		new_fdesc = NULL;
		free(desc_buff);
		desc_buff = NULL;
	}

	if (!buff || !fdesc || fmt < 1 || fmt > fdesc->n_rformats) {
		retval = FALSE;
		goto fail;
	}
	rfdesc = fdesc->record_desc+fmt-1;
	new_flds = (char **)calloc((rfdesc->n_fields)+2, sizeof(char *));
	new_sizes = malloc(rfdesc->n_fields * sizeof(*new_sizes));
/*
 * if this allocate has failed we can just fail without freeing up any
 * file description we might have read in above.  it's fine.  This is
 * just the actual array of fields that failed
 */
	if (!new_flds || !new_sizes) {
		retval = FALSE;
		goto fail;
	}
	memcpy(new_sizes, rfdesc->field_sizes,
			rfdesc->n_fields * sizeof(*new_sizes));

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
			b_len = get_long(buff+b_offs);
			b_offs += sizeof(int32_t);
			cptr = malloc(b_len+1);
			if (!cptr) {
				retval = FALSE;
				goto fail;
			}
			memcpy(cptr, buff+b_offs, b_len);
			new_sizes[i] = -b_len;
			b_offs += b_len;
		} else {
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
	}
	memcpy(rfdesc->field_sizes, new_sizes,
			rfdesc->n_fields * sizeof(*new_sizes));
	free(new_sizes);
	new_sizes = NULL;
/*
 * get rid of the prior recrd
 */
	if (old_flds) {
		for (i = 1; old_flds[i]; i++)
			free(old_flds[i]);
		free(old_flds);
	}

	if (type == MASTER) {
		mfld = new_flds;
	} else {
		wfld = new_flds;
    }

fail:
	free(desc_buff);
	free(new_sizes);
	if (new_flds && retval == FALSE) {
		for (i = 1; i <= (fdesc ? fdesc->record_desc[fmt-1].n_fields : 0); i++)
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
	long b_size;			/* size of rec inc. blob data */

//	short *desc;
	FILEDESC *desc;
	RFDESC *rfdesc;

	char *rec_buff;
	char **flds;
	char *nbuf;

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
		idx = cur_index._idxno;
	} else {
		fmt = w_fmt & MSK;
		cur = w_cur;
		chan = 0;
		desc = w_fdesc;
		flds = wfld;
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
		if (rfdesc->field_sizes[i] < 1) {
			b_size += (-rfdesc->field_sizes[i] + sizeof(int32_t));
			tmp++;
		}
	}
	rec_buff = malloc(b_size+64);

	if (!rec_buff)
		return FALSE;

	i = sprintf(rec_buff, "%d|%d|%d|%"PRId64"|%d|%ld|", FLUSH, idx, chan, cur, fmt, b_size);
	val = i + r_len;

	for (tmp = 0; tmp < rfdesc->n_fields; tmp++) {
		if (rfdesc->field_sizes[tmp] < 1) {
			cur = -rfdesc->field_sizes[tmp];
			put_long(rec_buff+val, (int32_t)cur);
			val += sizeof(int32_t);
			memcpy(rec_buff+val, flds[tmp+1], cur);
		    val += cur;
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
		for (tmp = 0; tmp < rfdesc->n_fields; tmp++) {
			if (rfdesc->field_sizes[tmp] < 0)
				rfdesc->field_sizes[tmp] = 0;
		}
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
