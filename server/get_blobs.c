/* ***************************************************************
 *
 * PROCEDURE:	get_blobs.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Mon Mar 14 13:21:48 MST 2005
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */
/*
 * this function is c alled if a record format contains (a/some)
 * blob(s).  its purpose is to read them, tack them onto the end
 * of the buffer going back to the client.  fun, right?
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>
#include <fcntl.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "file_desc.h"
#include "errors.h"

extern int dbgsw;

extern void put_long(char *, int32_t);

int get_blobs (FILES *fptr, int fmt, int64_t recno, char **p_rptr, int *max)
{

	int i, j;					/* standard loop counters */
	int len;					/* the final total length returned */
	int offs;
	int chan;

	char file_name[512];		/* file to open */
	char *path_name;			/* path name */
	char *data_name;			/* datafile name */
	char *rptr;

	RFDESC *rf_ptr;

	struct stat sbuff;

	rptr = *p_rptr;
/*
 * the name of a blob is:
 * 		database_root/blobs/filename.format#.record#.field#
 * 	so we need to get all of these things together.  also the
 * 	returned record buffer and length need to be modified.
 */
	rf_ptr = fptr->_filedesc->record_desc+(fmt-1);
	offs = len = rf_ptr->rf_len;

	strcpy(file_name, fptr->_fname);
	path_name = dirname(file_name);
	strcpy(file_name, path_name);
	path_name = strdup(dirname(file_name));
	strcpy(file_name, fptr->_fname);
	data_name = strdup(basename(file_name));

	for (i = 0, j = 0; i < rf_ptr->has_blob; j++) {
		if (rf_ptr->field_sizes[j] == 0) {
			i++;
			sprintf(file_name, "%s/blobs/%s.%d.%"PRId64".%d", path_name,
							data_name, fmt, recno, j);
			if (stat(file_name, &sbuff) < 0 || sbuff.st_size == 0) {
				len += sizeof(int32_t);
				if (!max || (max && (len > *max))) {
					rptr = realloc(rptr, len);
					if (max)
						*max = len;
				}
				put_long(rptr+offs, (int32_t)0);
				offs += sizeof(int32_t);
			} else {
				if (sbuff.st_size > 017777777777)
					return(ENOBLOB);
				len += sizeof(int32_t) + sbuff.st_size;
				if (!max || (max && len > *max)) {
					rptr = realloc(rptr, len);
					if (max)
						*max = len;
				}
/*
 * if st_size here is > 2 GB we're in trouble!
 */
				put_long(rptr+offs, (int32_t)(sbuff.st_size));
				offs += sizeof(int32_t);
				if ((chan = open(file_name, O_RDONLY)) < 0)
					return(ENOBLOB);
				if (read(chan, rptr+offs, sbuff.st_size) != sbuff.st_size) {
					close(chan);
					return(ENOBLOB);
				}
				close(chan);
				offs += sbuff.st_size;
			}
		}
	}
	free(path_name);
	free(data_name);
	*p_rptr = rptr;
	return(offs);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
