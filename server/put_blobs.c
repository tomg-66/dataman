/* ***************************************************************
 *
 * PROCEDURE:	put_blobs.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Mon Mar 14 14:43:21 MST 2005
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */
/*
 * this function is called if a record format contains (a/some)
 * blob(s).  its purpose is to put them out to the disk.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>
#include <fcntl.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "file_desc.h"
#include "errors.h"

extern int dbgsw;

extern int32_t get_long(char *);

int put_blobs (FILES *fptr, int fmt, int64_t recno, char *rptr)
{

	int i, j;					/* standard loop counters */
	int offs;
	int chan;

	long len;					/* the final total length returned */

	char file_name[512];		/* file to open */
	char *path_name;			/* path name */
	char *data_name;			/* datafile name */

	RFDESC *rf_ptr;

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
			len = get_long(rptr+offs);
			offs += sizeof(int32_t);
			if (len > 0) {
				sprintf(file_name, "%s/blobs/%s.%d.%"PRId64".%d", path_name,
							data_name, fmt, recno, j);
				if ((chan = open(file_name, O_CREAT|O_TRUNC|O_RDWR, 0666)) < 0) {
					return(ENOBLOB);
				}
				if (write(chan, rptr+offs, len) < len) {
					close(chan);
					return(EBLOBWRT);
				}
				close(chan);
				offs += len;
			}
		}
	}
	free(path_name);
	free(data_name);
	return(0);				/* return everthing worked */
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
