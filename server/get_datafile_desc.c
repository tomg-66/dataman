/* ***************************************************************
 *
 * PROCEDURE:	get_datafile_desc
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Wed Jul  7 16:16:33 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *				Tue Apr 15 19:39:30 MDT 2008
 *				modified to open/read/parse a list of free space
 *				associated with the file description being parsed.
 *				tomg
 *
 * 				Sat Jul 25 09:34:53 MDT 2009
 * 				removed references to the freelist and fixed a
 * 				constant
 * 				tomg
 *
 ************************************************************* */
/*
 * this function opens a data file, then parses out the data
 * file description into a nice little package that is easy to
 * use.
 *
 * this file is being opened; the first reference to it, so
 * there is a mutex being held on the index... no one else can
 * open this file up, and since no one else is yet referring to
 * it, no one else will be trying to read from the file... no
 * more locks are needed in here....
 *
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
#include <unistd.h>
#include <stddef.h>
#include <stdio.h>

#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>

#include "file_desc.h"
#include "errors.h"
#include "misc.h"

extern int dbgsw;

extern int16_t get_short(char *);
extern int32_t get_long(char *);

int get_datafile_desc(FILES *fptr)
{

	int tmp_chn;
	int i, j, k;			/* loop counters */

	short m_head;
	short *m_desc;

	int64_t m_longest;

	char *cptr;

	off_t len;

	FILEDESC *fdesc;
	RFDESC *rptr;

	if (dbgsw) {
		fprintf(stderr, "file name to open is %s\n", fptr->_fname);
		fflush(stderr);
	}
/*
 * open up the file
 */
	if ((tmp_chn = open(fptr->_fname,O_RDWR|O_LARGEFILE)) < 0)
		return (ENOFILE);

	llseek(tmp_chn, (loff_t)0, SEEK_SET);
	if (dbgsw) {
		fprintf(stderr, "opened file \n");
		fflush(stderr);
	}
/*
 * read the file header length, allocate the array of the file header
 * then read it
 */
	m_longest = 0;
	if (read(tmp_chn,(char *)&m_head,sizeof(short)) != sizeof(short)) {
		close(tmp_chn);
		return(EFHDRD);
	}
	fdesc = calloc(1, sizeof(FILEDESC));
	m_head = fdesc->header_len = get_short((char *)&m_head);
/*
 * allocate a buffer for the un-parsed file description, then
 * read the file description contained in the file header
 */
	if ((m_desc = (short *)malloc(m_head)) == NULL) {
		if (dbgsw) {
			fprintf(stderr, "alloc error %d, size = %d", errno, m_head);
			perror("");
			fflush(stderr);
		}
		close(tmp_chn);
		return(ENOALLOC);
	}
	if (read(tmp_chn,(char *)m_desc,m_head) != m_head) {
		close(tmp_chn);
		free(m_desc);
		m_desc = NULL;
		return(EFHDRD);
	}
/*
 * now start parsing out the description
 */
	fdesc->n_rformats = get_short((char *)m_desc);
	fdesc->record_desc = calloc(fdesc->n_rformats, sizeof(RFDESC));
	j = 1;
/*
 * allocate a record format description for each
 * one defined in the file and parse the rest of the record desc
 */
	for (i = 0; i < fdesc->n_rformats; i++) {
		rptr = fdesc->record_desc+i;
		rptr->n_fields = get_short((char *)(m_desc+j));
		j++;
		rptr->rf_len = get_short((char *)(m_desc+j));
		if (rptr->rf_len > m_longest)
			m_longest = rptr->rf_len;
		j++;
		rptr->field_sizes = calloc(rptr->n_fields, sizeof(int));
/*
 * get the lengths for each field in the record format.  if there
 * is a field with a length of zero, that indicates that this
 * rf contains a blob, so we keep track of how many for each rf.
 */
		for(k=0;k<rptr->n_fields;k++) {
			rptr->field_sizes[k] = get_short((char *)(m_desc+j));
			if (rptr->field_sizes[k] == 0)
				rptr->has_blob++;
			j++;
		}
	}
/*
 * save the pertinent information
 */
	fdesc->longest = m_longest;
	for (i = 0; i < m_head/2; i++)
		*(m_desc+i) = get_short((char *)(m_desc+i));
	fptr->_filedesc = fdesc;
	fptr->_longest = m_longest;
	fptr->_desc = m_desc;
	fptr->_chan = tmp_chn;
	fptr->_hlen = m_head;

	pthread_mutex_init(&fptr->_lock.mutex, NULL);
	pthread_mutex_init(&fptr->_mutex, NULL);

	if (dbgsw) {
		fprintf(stderr, "datafile description\n"
						"	number of formats - %d\n"
						"	header length - %d\n"
						"	longest record - %d\n",
						fdesc->n_rformats, fdesc->header_len,
						fdesc->longest);
		for (i = 0; i < fdesc->n_rformats; i++) {
			fprintf(stderr, "	number of fields for format %d - %d\n"
							"	length of data record - %d\n",
							i+1, fdesc->record_desc[i].n_fields,
							fdesc->record_desc[i].rf_len);
			for (k = 0; k < fdesc->record_desc[i].n_fields; k++) {
				fprintf(stderr, "		field %d - %d\n",
							k, fdesc->record_desc[i].field_sizes[k]);
			}
		}
		fprintf(stderr, "\n");
	}
	return (1);

}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
