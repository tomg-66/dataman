/* ***************************************************************
 *
 * PROCEDURE:	index.cc
 *
 * PROJECT:		dataman client side c++ routine
 * 
 * DATE:		Wed Jul  7 16:51:07 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Thu Mar 21 15:49:21 MDT 2013
 * 				Tom Green
 * 				added name space
 *
 ************************************************************* */
//
// this is the implementation of the index constructor.  it calls iopen to
// open the index the destructor calls iclose to terminate the connection
//
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

#include <fileEdit.hh>
#include <db_comm.hh>

#include <errors.h>
#include <dbfunc.h>

#include <stdio.h>			// need sprintf
#include <errno.h>

extern void db_err(int, const char *, ...);

using namespace Dataman;

index::index(void) {
	_idxname = NULL;
	_idxno = 0;
	_wrmode = 0;
	_fno = 0;
	_nfiles = 0;
	_keylen = 0;
	_longest = 0;
	_curnode = 0;
	_rptr = 0;
	_offs = 0;
//	_curkey = NULL;
	_files = NULL;
}

index::index(char *name, int mode)
{
	if (mode < RDONLY || mode > UPDATE) {
		errno = EINVAL;
		db_err(0, "%s: can't init index %s", _progname, name);
	}
	memset(this, '\0', sizeof(class index));
	this->iopen(name, mode);
}

void index::iopen(char *name, int mode)
{
	int x = -1;
	int idx, i;
	char *buff;

	for (idx = 0; idx < 6; idx++) {
		if (!this->_onames[idx]) {
			if (x == -1)
				x = idx;
			continue;
		}
		if (!strcmp(name, this->_onames[idx])) {
			db_err(EIDXOPN, "%s: can't init index %s", _progname, name);
		}
	}
	if (x == -1) {
		errno = EMFILE;
		db_err(0, "%s: can't init index %s", _progname, name);
	}
	idx = x;
	this->_onames[idx] = new char[strlen(name)+1];
	strcpy(this->_onames[idx], name);

	char comm_buffer[256];
	db_comm comm;
	sprintf(comm_buffer, "%d|%s|%s|", IOPEN, name, _root);
	try {
		buff = comm.db_send(comm_buffer, strlen(comm_buffer));
	}
	catch (int i) {
		comm.db_err(0, "%s: socket read error in IOPEN", _progname);
	}

	i = atoi(buff);
	if (i < 0)
		comm.db_err(i, "%s: server error in IOPEN", _progname);
/*
 * parse the message
 */
	char *cptr;
	this->_idxname = this->_onames[idx];
	cptr = strchr(buff, '|') + 1;
	this->_idxno = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	this->_keylen = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;
	i = this->_nfiles = atoi(cptr);
	cptr = strchr(cptr, '|') + 1;

	this->_files = new files[i];
	for (i = 0; i < this->_nfiles; i++) {
		this->_files[i].set_name(cptr);
		this->_files[i].set_fno(i);
		cptr += strlen(cptr) + 1;
	}
	this->_wrmode = mode;
	delete[] buff;
}

index::~index()
{
	if (this->_idxname)
		this->iclose();
}

void index::iclose(void)
{

	int i;

	char buff[32];

	if (cur_index && cur_index->get_wrmode())
		master.out_rec();					/* flush the current record */
	if (cur_index == this)
		cur_index = NULL;

	for(i = 0; i < 6; i++) {
		if (this->_idxname == this->_onames[i]) {
			delete[] this->_onames[i];
			this->_onames[i] = (char *)NULL;
			break;
		}
	}

//	if(_curkey)
//		delete _curkey;

	if (_files)
		delete[] _files;

	if (_savptr)
		free(_savptr);

	sprintf(buff, "%d|%d|", ICLOSE, this->_idxno);
	db_comm comm;
	try {
		comm.db_send(buff, strlen(buff));
	}
	catch (int err) {
		comm.db_err(0, "%s: socket read error in ICLOSE", _progname);
	}

	i = atoi(buff);
	if (i < 0)
		comm.db_err(i, "%s: iclose error", _progname);

}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
