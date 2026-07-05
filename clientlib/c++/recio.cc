/* ***************************************************************
 *
 * PROCEDURE:	recio.cc
 *
 * PROJECT:		dataman client side c++ library routine
 * 
 * DATE:		Mon Jun 14 09:43:33 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Sat Mar 26 18:50:05 MST 2005
 * 				add functionality to support blobs.  if a
 * 				datafield has a length of 0 that indicates that
 * 				it is a blob.  so we copy it out of the end of
 * 				the fixed portion of the data record.  when we
 * 				send it back, we copy it in at the end in the
 * 				same manner.
 * 				tomg
 *
 * 				Thu Mar 21 15:49:21 MDT 2013
 * 				Tom Green
 * 				added name space
 *
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
#include <errno.h>
#include <inttypes.h>

#include <netinet/in.h>				// for declaration of htonl

#include <endSort.hh>
#include <datafile_header.h>
#include <db_comm.hh>

#include "../../server/dbfunc.h"

extern unsigned long get_long(char *);

using namespace Dataman;

void datarecord::in_rec(char *buff)
{
	int idx,i,j;			// misc usage

	int len;				// length of record
	int offs;				// offset to end of fixed portion of rec

	unsigned long b_len;	// length of blob

	short *desc;			// point to unparsed description

    datafield *flds;		// pointers to the fields
	char string[128];
	char *cptr;
	char *tptr;

	FILEDESC *fdesc;		// the parsed file description
	RFDESC *rfdesc;

	db_comm comm;

	files *cur_file;
//
// get rid of the prior recrd
//
	if (this->field)
		delete[] this->field;

//
// do we need to now manually retrieve the file description?
//
	if ((fdesc = this->get_desc()) == NULL) {
		if (this->getwhich() == MASTER)
			sprintf(string, "%d|%d|%d|", GET_DESC, cur_index->get_idxno(),
							this->getchan());
		else
			sprintf(string, "%d|%d|", GET_DESC, -(this->getchan()));
		try {
			cptr = comm.db_send(string, strlen(string));
		}
		catch(int errval) {
			comm.db_err(0, "%s: Can't read socket in RECIO", _progname);
		}

		len = atoi(cptr);
		if (len < 0)
			comm.db_err(len, "%s: get_desc failed", _progname);
//
//ok, we now have the description of the file from the server, we now
//need to parse the results
//
//we do this here for one purpose only.  do it only once!  if we don't
//do it here, then we need to search through the description for lengths
//and offsets every time we need to input and output a record.  we are
//trying to make this a tad snappier!
//
		tptr = strchr(cptr, '|') + 1;
		tptr = strchr(tptr, '|') + 1;
		desc = (short *)tptr;
//
//master files need a bit more information saved than the work file
//
		if (this->getwhich() == MASTER) {
			cur_file = cur_index->get_files() + cur_index->get_fno();
			cur_file->set_hlen(len);
			this->head = len;
			fdesc = (FILEDESC *)calloc(1, sizeof(FILEDESC));
			cur_file->set_desc(fdesc);
			this->_filedesc = fdesc;
		} else {
			fdesc = (FILEDESC *)calloc(1, sizeof(FILEDESC));
			this->_filedesc = fdesc;
		}
		fdesc->header_len = len;
		fdesc->n_rformats = *desc;
		fdesc->record_desc = (RFDESC *)calloc(fdesc->n_rformats, sizeof(RFDESC));
		i = 1;
		for (idx = 0; idx < fdesc->n_rformats; idx++) {
			rfdesc = fdesc->record_desc+idx;
			rfdesc->n_fields = *(desc+i);
			i++;
			rfdesc->rf_len = *(desc+i);
			rfdesc->field_sizes = (short int *)calloc(rfdesc->n_fields, sizeof(short int));
			for (++i, j = 0; j < rfdesc->n_fields; j++, i++) {
				rfdesc->field_sizes[j] = *(desc+i);
				if (*(desc+i) == 0)
					rfdesc->has_blob++;
			}
		}
		delete[] cptr;
	}
//
//i know, i know, i'm allocating two more datafields than is needed for the
//record.  but that gives a buffer of one on each side for now, so that the
//user doesn't mess up.  i'm going to write a bounded array into things in
//the future, but this is ok for now.  should i leave this in, even though
//the bounded array is implemented?
//
	rfdesc = fdesc->record_desc+this->getfmt()-1;
	flds = new datafield[rfdesc->n_fields+2];
	offs = rfdesc->rf_len;

	j = 0;
	for (i = 0; i < rfdesc->n_fields; i++) {
		if (rfdesc->field_sizes[i] != 0) {
			flds[i+1].make_field(buff+j, rfdesc->field_sizes[i], this->which);
		} else {
			b_len = get_long(buff+offs);
			offs += sizeof(long);
			flds[i+1].make_field(buff+offs, -((int)b_len), this->which);
			offs += b_len;
		}
		j += rfdesc->field_sizes[i];
	}
	this->field = flds;
	this->setdirty(0);
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
void datarecord::out_rec()

{
	int i;
	int idx;		/* loop counter */
	int tmp;		/* more counter */
	int chan,
		val;

	long b_size;

	RFDESC *rfdesc;

	char *rec_buff;
	char *nbuf;

	db_comm comm;

/*
 * if mfld is equal to zero (null) there has never been an assignment to
 * any of the fields and this procedure would invalidate any record
 * there will allways be a work field
 */
	if (this->field == NULL)
		return;

//
//check if the record has been modified... it is dirty.  if it isn't
//dirty, no need to save it to the database...
//
	if (!this->getdirty())
		return;

	if (this->getwhich() == MASTER) {
		chan = this->getchan();
		idx = cur_index->get_idxno();
	} else {
		chan = 0;
		idx = -this->getchan();
	}
	rfdesc = (this->get_desc())->record_desc+this->getfmt()-1;
	b_size = (long)rfdesc->rf_len;
	for (i = 0, tmp = 0; tmp < rfdesc->has_blob; i++) {
		if (this->field[i].get_type() == type_blob) {
			b_size += this->field[i].datalen() + sizeof(long);
			tmp++;
		}
	}
	rec_buff = (char *)malloc(b_size+64);
	sprintf(rec_buff, "%d|%d|%d|%"PRId64"|%d|%ld|", FLUSH, idx, chan,
					this->getcur(), this->getfmt(), b_size);
	i = strlen(rec_buff);
	val = i + rfdesc->rf_len;

	for (tmp = 1; tmp <= rfdesc->n_fields; tmp++) {
		if (this->field[tmp].get_type() == type_blob) {
//			put_long(rec_buff+val, (long)this->field[tmp].datalen());
// use the inline function ntohl instead of put_long
			*(unsigned long *)(rec_buff+val) = ntohl((long)this->field[tmp].datalen());
			val += sizeof(long);
			::memcpy(rec_buff+val, this->field[tmp].getptr(), this->field[tmp].datalen());
			val += this->field[tmp].datalen();
		} else {
			::memcpy(rec_buff+i, ((this->field)+tmp)->getptr(),
						rfdesc->field_sizes[tmp-1]);
		}
		i += rfdesc->field_sizes[tmp-1];
	}

	try {
		nbuf = comm.db_send(rec_buff, val);
	}
	catch(int err_val) {
		comm.db_err(0, "%s: Can't read socket in OUT_REC", _progname);
	}
	free(rec_buff);

	i = atoi(nbuf);
	if (i < 0)
		comm.db_err(i, "%s: error in out_rec", _progname);
	delete[] nbuf;
	this->setdirty(0);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
