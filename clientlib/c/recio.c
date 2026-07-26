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

void in_rec(int type, char *buff)
{
	int idx,i,j;               /* misc usage */

	int fmt,				/* the working format number */
		len,				/* length of record */
		chan,
		b_offs;				/* offset to blob in record */

	unsigned long b_len;

    short *desc;			/* point at the unparsed file description */

    char **flds;			/* pointers to the fields */
	char string[128];
	char *cptr;
	char *tptr;

	FILEDESC *fdesc;		/* the parsed file description */
	RFDESC *rfdesc;

	if (type == MASTER) {
		chan = m_chan;
		fdesc = m_fdesc;
		fmt = m_fmt;
		flds = mfld;
	} else {
		chan = -w_chan;
		fdesc = w_fdesc;
		fmt = w_fmt;
		flds = wfld;
	}
/*
 * get rid of the prior recrd
 */
	if (flds) {
		for (i = 1; flds[i]; i++)
			free(flds[i]);
		free(flds);
	}

/*
 * do we need to now manually retrieve the file description?
 */
	if (fdesc == NULL) {
		if (type == MASTER)
			sprintf(string, "%d|%d|%d|", GET_DESC, cur_index._idxno, chan);
		else
			sprintf(string, "%d|%d|", GET_DESC, chan);
		cptr = db_send(string, strlen(string), __FILE__);

		len = atoi(cptr);
		if (len < 0)
			db_err(len, "%s: get_desc failed", _progname);

		tptr = strchr(cptr, '|') + 1;
		tptr = strchr(tptr, '|') + 1;
		desc = (short *)tptr;
		if (type == MASTER) {
			INDEX *idx;
			idx = findex(cur_index._idxname);
			idx->_files[idx->_fno]._hlen = m_head = len;
			fdesc = idx->_files[idx->_fno]._filedesc = calloc(1, sizeof(FILEDESC));
			m_fdesc = fdesc;
		} else {
			fdesc = w_fdesc = calloc(1, sizeof(FILEDESC));
			w_fdesc = fdesc;
		}
		fdesc->header_len = len;
		fdesc->n_rformats = *desc;
		fdesc->record_desc = calloc(fdesc->n_rformats, sizeof(RFDESC));
		i = 1;
		for (idx = 0; idx < fdesc->n_rformats; idx++) {
			rfdesc = fdesc->record_desc+idx;
			rfdesc->n_fields = *(desc+i);
			i++;
			rfdesc->rf_len = *(desc+i);
			rfdesc->field_sizes = (short *)calloc(rfdesc->n_fields, sizeof(short));
			for (++i, j = 0; j < rfdesc->n_fields; j++, i++) {
				rfdesc->field_sizes[j] = *(desc+i);
				if (*(desc+i) == 0)
					rfdesc->has_blob++;
			}
		}
		free(cptr);
	}

	rfdesc = fdesc->record_desc+fmt-1;
	flds = (char **)calloc((rfdesc->n_fields)+2, sizeof(char *));

    *flds = NULL;						/* zero the dirty bit */
    *(flds+rfdesc->n_fields+1) = NULL;	/* end of record indicator */
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
		if (rfdesc->field_sizes[i] == 0) {
			b_len = get_long(buff+b_offs);
			b_offs += sizeof(int32_t);
			cptr = malloc(b_len+1);
			memcpy(cptr, buff+b_offs, b_len);
			rfdesc->field_sizes[i] = -b_len;
			b_offs += b_len;
		} else {
			cptr = calloc(1, rfdesc->field_sizes[i]+1);
			memcpy(cptr, buff+j, rfdesc->field_sizes[i]);
			j += rfdesc->field_sizes[i];
		}
		flds[i+1] = cptr;
	}

	if (type == MASTER) {
		mfld = flds;
	} else {
		wfld = flds;
    }
	return;
}

/*
 * this procedure re writes the current record that is being used to
 * its palce in it's particular file.
 * just flush/output the record.  this is so we don't have to re-read
 * it if the calling function (like forward or back) didn't return
 * true.
 */

#define MSK	077
void out_rec(int type)

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
			return;
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
	if (!*flds)
		return;

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

	i = sprintf(rec_buff, "%d|%d|%d|%"PRId64"|%d|%ld|", FLUSH, idx, chan, cur, fmt, b_size);
	val = i + r_len;

	for (tmp = 0; tmp < rfdesc->n_fields; tmp++) {
		if (rfdesc->field_sizes[tmp] < 1) {
			cur = -rfdesc->field_sizes[tmp];
			put_long(rec_buff+val, (int32_t)cur);
			val += sizeof(int32_t);
			memcpy(rec_buff+val, flds[tmp+1], cur);
		    val += cur;
			rfdesc->field_sizes[tmp] = 0;
		} else
			memcpy(rec_buff+i, flds[tmp+1], rfdesc->field_sizes[tmp]);
		i += rfdesc->field_sizes[tmp];
	}

	nbuf = db_send(rec_buff, val, __FILE__);
	i = atoi(nbuf);
	if (i < 0)
		db_err(i, "%s: error in out_rec", _progname);
	*flds = NULL;
	free(rec_buff);
	free(nbuf);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
