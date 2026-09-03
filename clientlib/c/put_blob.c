/* ***************************************************************
 *
 * PROCEDURE:	put_blob.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		Fri Mar 18 17:39:42 MST 2005
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *
 ************************************************************* */

/*
 * we need to a way to -intelligently- store a blob in the
 * data record.  since neither of the defines in the header
 * will do what we need, we will do it this way.  it's ugly
 * and I think I can find a better way, but for now, let's
 * do what we can.
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

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "visibility.h"

#include "m_params.h"
#include "w_params.h"

#include "../../server/datafile_header.h"

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

DATAMAN_API int put_blob(char *field, void *blob, int size)
{

	int i, j;
	char **rec;
	void *buff;
	uint32_t *blob_lengths;

	RFDESC *rfdesc;

/*
 * since blobs are slow in the first place, I'm not currently
 * worried about this.  locate the field that is passed in to
 * assign the new blob to.
 */
	for (i = 0; i < 2; i++) {
		if (i == 0) {
			if (!mfld || !m_fdesc || !m_fdesc->record_desc ||
					m_fmt < 1 || m_fmt > m_fdesc->n_rformats)
				continue;
			rec = mfld;
			blob_lengths = m_blob_lengths;
			rfdesc = m_fdesc->record_desc+(m_fmt-1);
		} else {
			if (!wfld || !w_fdesc || !w_fdesc->record_desc ||
					w_fmt < 1 || w_fmt > w_fdesc->n_rformats)
				continue;
			rec = wfld;
			blob_lengths = w_blob_lengths;
			rfdesc = w_fdesc->record_desc+(w_fmt-1);
		}
		if (!rfdesc->has_blob)
			continue;
		for (j = 1; j <= rfdesc->n_fields; j++)
			if (field == rec[j])
				goto found;
	}
	return(FALSE);

found:
/*
 * if the field they selected isn't a blob, return false.  a
 * blob has a schema field length of zero.  Its current runtime
 * length is kept separately in the record's blob-length array.
 */
	if (rfdesc->field_sizes[j-1] != 0 || !blob_lengths || size < 0 ||
			(size > 0 && !blob))
		return(FALSE);

	if ((buff = malloc(size ? (size_t)size : 1U)) == NULL)
		return(FALSE);

	if (size)
		memcpy(buff, blob, (size_t)size);	/* save the blob */
	free(rec[j]);							/* free the old one */
	rec[j] = (char *)buff;					/* make rec point to current one */

	rec[0] = (char *)((uintptr_t)rec[0] | 1);	/* flag record as dirty */
	blob_lengths[j-1] = (uint32_t)size;
	return(TRUE);
}

/*
 * get the size of a blob that is contained in the datafield
 */
DATAMAN_HIDDEN long get_blob_size(char *field)
{
	int i, j;
	char **rec;
	uint32_t *blob_lengths;

	RFDESC *rfdesc;

	for (i = 0; i < 2; i++) {
		if (i == 0) {
			if (!mfld || !m_fdesc || m_fmt < 1 || m_fmt > m_fdesc->n_rformats)
				continue;
			rec = mfld;
			blob_lengths = m_blob_lengths;
			rfdesc = m_fdesc->record_desc+(m_fmt-1);
		} else {
			if (!wfld || !w_fdesc || w_fmt < 1 || w_fmt > w_fdesc->n_rformats)
				continue;
			rec = wfld;
			blob_lengths = w_blob_lengths;
			rfdesc = w_fdesc->record_desc+(w_fmt-1);
		}
		if (!rfdesc->has_blob)
			continue;
		for (j = 1; j <= rfdesc->n_fields; j++)
			if (field == rec[j])
				goto found;
	}
	return (0xffffffff);

found:
	j--;
	if (rfdesc->field_sizes[j] != 0 || !blob_lengths)
		return (0xffffffff);
	return((long)blob_lengths[j]);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
