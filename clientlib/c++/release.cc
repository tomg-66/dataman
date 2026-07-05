/* ***************************************************************
 *
 * PROCEDURE:	release.c
 *
 * PROJECT:		dataman client side c++ library routine
 * 
 * DATE:		Mon Jun 14 08:03:33 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Sat Mar 26 18:53:59 MST 2005
 * 				for supporting blobs we changed the name of the
 * 				include file from file_desc.h to datafile_header.h
 * 				tomg
 *
 * 				Thu Mar 21 15:49:21 MDT 2013
 * 				Tom Green
 * 				added name space
 *
 ************************************************************* */

/*
 * this routine is very similar to forward.  The first defference is that
 * when this routine comes to the end of a data file it tries to get the
 * first record in the next named data file, if it is at the en of the data
 * file list it returns false.  The second difference is that it works only
 * on a work file.
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

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <inttypes.h>

#include <endSort.hh>
#include <db_comm.hh>

#include "../../server/dbfunc.h"
#include "../../server/errors.h"
#include "../../server/datafile_header.h"

#define FALSE   0
#define TRUE    1

extern short _maxfil;						/* maximum number of files */

using namespace Dataman;

int datarecord::release(void)

{
    char cmd[256];                     /* path to file */
	char *ptr;
	char *cptr;

    int tmp;

	db_comm comm;

	FILEDESC *fdesc;		// the parsed file description
	RFDESC *rfdesc;

	if (this->getwhich() != WORK)
		comm.db_err(ENOTWORK, "%s: Error in release", _progname);

    this->_file = 0;
    this->out_rec();
    if (!this->getnext()) {
		if (++_fileno == _maxfil) {
			_fileno--;
			return (FALSE);
		}
//
//ok, we need to free up the old description before in_rec allocates
//us a new one
//
		fdesc = this->_filedesc;
		rfdesc = fdesc->record_desc;
		for (tmp = 0;tmp < fdesc->n_rformats; tmp++)
			free((rfdesc+tmp)->field_sizes);
		free(rfdesc);
		free(fdesc);
		this->_filedesc = NULL;

		sprintf(cmd, "%d|%d|0|%s/files/%s|", RELEASE, this->chan, _root,
						_fnames[_fileno]);
		this->_file = 1;
	} else
		sprintf(cmd, "%d|%d|%"PRId64"|", RELEASE, this->chan, this->next);
/*
 * send the command and wait for the response.  then allocate space for it
 * then readit.
 */
	try {
		ptr = comm.db_send(cmd, strlen(cmd));
	}
	catch (int comm_err) {
		comm.db_err(0, "%s: Can't read socket response in RELEASE", _progname);
	}

	cptr = ptr;
	this->len = atoi(cptr);
	if (this->len < 0)
		comm.db_err(this->len, "%s: error in release", _progname);
/*
 * parse the response and save the appropriate stuff
 */
	cptr = strchr(cptr, '|') + 1;
	this->longest = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	this->fmt = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	this->cur = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	this->prev = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') + 1;
	this->next = strtoll(cptr, NULL, 0);
	cptr = strchr(cptr, '|') +1 ;
//	tmp = cptr - ptr;
//	memmove(ptr, cptr, this->len);
//	memset(ptr+this->len, '\0', tmp);
/*
 * ok, done!
 */
    this->in_rec(cptr);					/* read record into memory */
	delete[] ptr;
    return (TRUE);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
