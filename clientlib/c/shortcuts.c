/* ***************************************************************
 *
 * PROCEDURE:	shortcuts.c
 *
 * PROJECT:		dataman client side 'C'
 * 
 * DATE:		Tue Sep  1 08:38:47 PM MDT 2026
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */
/*
 * this implements some interface routines that the include files
 * and other language bindings use, but don't want to expose the
 * underlying data.
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

#include "visibility.h"
#include "index.h"
#include "dataman_prototypes.h"

DATAMAN_HIDDEN extern INDEX cur_index;
DATAMAN_HIDDEN extern char _file;
DATAMAN_HIDDEN extern char **_fnames;
DATAMAN_HIDDEN extern int _fileno;
DATAMAN_HIDDEN extern FILEDESC *m_fdesc;
DATAMAN_HIDDEN extern FILEDESC *w_fdesc;
DATAMAN_HIDDEN extern char m_fmt;
DATAMAN_HIDDEN extern char w_fmt;
DATAMAN_HIDDEN extern uint32_t *m_blob_lengths;
DATAMAN_HIDDEN extern uint32_t *w_blob_lengths;

DATAMAN_API extern char **mfld;
DATAMAN_API extern char **wfld;

DATAMAN_API const char *_get_indexname(void)
{
	return cur_index._idxname;
}

DATAMAN_API const char *_get_curkey(void)
{
	return cur_index._curkey;
}

DATAMAN_API int _get_keylength(void)
{
	return cur_index._keylen + sizeof(char) + sizeof(int64_t);
}

DATAMAN_API const char *_get_filename(void)
{
	return cur_index._files[cur_index._fno]._fname;
}

DATAMAN_API bool _is_master_format(int f)
{
	return mfld && m_fmt == f;
}

DATAMAN_API bool _is_work_format(int f)
{
	return wfld && w_fmt == f;
}

DATAMAN_API bool _is_new_file()
{
	return wfld && _file;
}

DATAMAN_API int _get_master_format()
{
	return m_fmt;
}

DATAMAN_API int _get_work_format()
{
	return w_fmt;
}

DATAMAN_API const char *_get_workfilename()
{
	return _fnames[_fileno];
}

DATAMAN_API bool _has_record(dataman_record_type type)
{
	if (type == DATAMAN_MASTER_RECORD)
		return m_fdesc != NULL;
	return w_fdesc != NULL;
}

DATAMAN_API int _get_maxfields(dataman_record_type type)
{
	if (type == DATAMAN_MASTER_RECORD)
		return m_fdesc->record_desc[m_fmt-1].n_fields;
	return w_fdesc->record_desc[w_fmt-1].n_fields;
}

DATAMAN_API ssize_t _get_datafield_length(dataman_record_type type, int field_number)
{
	ssize_t field_size;
	if (type == DATAMAN_MASTER_RECORD) {
		field_size = m_fdesc->record_desc[m_fmt-1].field_sizes[field_number-1];
		if (field_size == 0)			// a zero indicates a blob
			field_size = m_blob_lengths[field_number-1];
	} else {
		field_size = w_fdesc->record_desc[w_fmt-1].field_sizes[field_number-1];
		if (field_size == 0)
			field_size = w_blob_lengths[field_number-1];
	}
	return field_size;
}

DATAMAN_API void _set_blob_length(dataman_record_type type, int field_number, ssize_t length)
{
	if (type == DATAMAN_MASTER_RECORD)
		m_blob_lengths[field_number-1] = length;
	else
		w_blob_lengths[field_number-1] = length;
}

DATAMAN_API bool _is_blob(dataman_record_type type, int field_number)
{
	if (type == DATAMAN_MASTER_RECORD)
		return m_fdesc->record_desc[m_fmt-1].field_sizes[field_number-1] == 0;
	return w_fdesc->record_desc[w_fmt-1].field_sizes[field_number-1] == 0;
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
