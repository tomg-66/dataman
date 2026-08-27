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
// this is the implementation of the index constructor.  it calls _iopen to
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

#include <stdio.h>			// need sprintf
#include <errno.h>

#include "fileEdit.hpp"
#include "db_comm.hpp"
#include "datamanError.hpp"

#include "../../server/errors.h"
#include "../../server/dbfunc.h"

#include <memory>

extern void db_err(int, const char *, ...);

using namespace Dataman;

index::index(void) :
	_idxname(NULL),
	_idxno (-1),
	_wrmode(RDONLY),
	_fno(-1),
	_nfiles(0),
	_keylen(0),
	_longest(0),
	_curnode(0),
	_rptr(0),
	_generation(0),
	_offs(0),
	_files(NULL),
	_savptr(NULL)
{
}

index::index(char *name, int mode) : index()
{
	if (mode < RDONLY || mode > UPDATE) {
		throw makeError(-EINVAL, "Invalid open mode for index %s", name);
	}
	this->_iopen(name, mode);
}

void index::_iopen(const char *name, int mode)
{
	int x = -1;
	int idx, i;

	for (idx = 0; idx < 6; idx++) {
		if (!this->_onames[idx]) {
			if (x == -1)
				x = idx;
			continue;
		}
		if (!strcmp(name, this->_onames[idx])) {
			throw makeError(EIDXOPN, "%s: can't init index %s", _progname, name);
		}
	}

	if (x == -1) {
		errno = EMFILE;
		throw makeError(0, "%s: can't init index %s", _progname, name);
	}

	idx = x;
	char comm_buffer[256];
	db_comm comm;
	sprintf(comm_buffer, "%d|%s|%s|", IOPEN, name, _root);

// db_comm will return a buffer only on success.  otherwise it will 
// throw an error. let the user catch the communication error.  if
// the iopen fails that's also a throwable error.
	std::unique_ptr<char[]> buff(comm.db_send(comm_buffer, strlen(comm_buffer)));

	i = atoi(buff.get());
	if (i < 0)
		throw makeError(i, "%s: server error in IOPEN", _progname);

	this->_onames[idx] = new char[strlen(name)+1];
	strcpy(this->_onames[idx], name);
/*
 * parse the message
 */
	char *cptr;
	this->_idxname = this->_onames[idx];
	cptr = strchr(buff.get(), '|') + 1;
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
}

void index::iclose()
{
	try {
		_iclose();
	} catch (const datamanError &) {
		_unwind();
		throw;
	}
}

/*
 * this is a destructor... don't throw anything!
 */
index::~index()
{
	if (this->_idxname) {
		try {
			this->_iclose();
		} catch (const datamanError &) {
			_unwind();
		}
	}
}

void index::_iclose(void)
{
	int i;
	int ret;
	char buff[32];

	if (!_idxname)
		return;

// output the record to the database.  if it fails catch
// the error and propogate it.
	if (cur_index == this && _wrmode) {
		try {
			masterRecord.out_rec();
		} catch (const datamanError &) {
			throw;
		}
	}

	sprintf(buff, "%d|%d|", ICLOSE, this->_idxno);

// send the iclose command, then do cleanup.  if the iclose failed
// throw an error
	db_comm comm;
	std::unique_ptr<char[]> answer(comm.db_send(buff, strlen(buff)));

	ret = atoi(answer.get());
	if (ret < 0)
		throw makeError(ret, "%s: iclose error", _progname);
	_unwind();
}

void index::_unwind()
{
	int i;

	for(i = 0; i < 6; i++) {
		if (this->_idxname == this->_onames[i]) {
			delete[] this->_onames[i];
			this->_onames[i] = (char *)NULL;
			break;
		}
	}

	if (cur_index == this) {
		masterRecord.init();
		cur_index = NULL;
	}

	if (_files)
		delete[] _files;

	if (_savptr)
		free(_savptr);

	_idxname = NULL;
	_files = NULL;
	_idxno = -1;
	_fno = -1;
	_nfiles = 0;

}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
