/* ***************************************************************
 *
 * PROCEDURE:	dumpdf.c
 *
 * PROJECT:		dataman system utilities
 * 
 * DATE:		legacy, written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 
 * 				Fri Aug 12 12:57:06 MDT 2005
 * 				modified to use 64 bit offsets to make this
 * 				handle -big- databases.
 * 				tomg
 ************************************************************* */

/*
 * this program will dump the contents of a dataman data file.
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

int dbgsw;					/* this is needed for get_datafile_desc */

#include <stdio.h>
#include <malloc.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "file_desc.h"
#define DBERROR
#include "errors.h"
#include "misc.h"

extern int64_t get_ll(char *);
extern short get_short(char *);
extern char *substr(char *, int, int);
extern int get_datafile_desc(FILES *);

static void usage(void);

int main(int argc,char *argv[])

{
	int index,count,acc;      /* misc vars */

	int64_t next_rec;     /* record pointers */

	int fmt;				/* current format number */
	int i;

	char misc_buf[512];			/* general buffer */
	char name[32];				/* index file name */
	char *cp;              	    /* character pointer */
	char *buff;					/* record buffer */
	char *root;					/* the data base root dir */

	FILES file_desc;			/* file description */
	RFDESC *rfdesc;				/* record format description */

	memset((char *)&file_desc, '\0', sizeof(FILES));
	root = NULL;
	if (argc != 2 && argc != 4)
		usage();

	if (argc == 4) {
	   	if (strcmp("-r", argv[1]))
			usage();
		root = strdup(argv[2]);
		strcpy(name, argv[3]);
	} else {
		strcpy(name, argv[1]);
		if (*name == '-')
			usage();
	}

	if (!root && (root = getenv("ROOT")) == NULL) {		/* get dataman root */
		fprintf(stderr, "ROOT not defined!\n");
		exit(-1);
	}
	sprintf(misc_buf, "%s/files/%s", root, name);
	file_desc._fname = strdup(misc_buf);
#if !defined __gnu_linux__
	endian_check();
#endif
/*
 * open the datafile, read the description, then allocate the
 * read buffer for the data records.
 */
	if ((i = get_datafile_desc(&file_desc)) < 0) {
		fprintf(stderr, "Can't read datafile description: %s\n",
						db_err_strings[-i]);
		exit(-1);
	}
	if ((buff = malloc(file_desc._longest)) == NULL) {
		fprintf(stderr, "Can't allocate data buffer: ");
		perror("");
		exit(errno);
	}
/*
 * get the pointer to the first record.  it's not set up in
 * get_datafile_desc
 */
	if (read(file_desc._chan, misc_buf, sizeof(int64_t)) != sizeof(int64_t)) {
		fprintf(stderr, "Can't read pointer to first rec: ");
		perror("");
		exit(errno);
	}
	next_rec = get_ll(misc_buf);
/*
 * go for ever reading records, and displaying the data
 */
	for (count = 1; ;count++) {
		if (next_rec == 0)									/* all done */
			break;
		llseek(file_desc._chan,next_rec,0);					/* get to current rec */
		if(read(file_desc._chan,misc_buf,DATARECORD_HEADER_LENGTH) < DATARECORD_HEADER_LENGTH) {
			fprintf(stderr, "Can't read record header ");
			perror("");
			exit(errno);
		}

		fmt = misc_buf[0] & 077;							/* format number */
		rfdesc = file_desc._filedesc->record_desc+(fmt-1);
		next_rec = get_ll(misc_buf+OFFSET_TO_NEXT);					/* get next record pointer */

		if (read(file_desc._chan,buff,rfdesc->rf_len) < rfdesc->rf_len) {      /* read the record */
			fprintf(stderr, "Can't read record data");
			perror("");
			exit(errno);
		}
/*
 * print each data field seperately.  easy to read that way.
 */
		for(index = 1,acc=0;index <= rfdesc->n_fields;index++) {
			if (rfdesc->field_sizes[index-1] == 0)
				cp = strdup("Blob Data");
			else
				cp = substr(buff,acc,acc+rfdesc->field_sizes[index-1]-1);
			printf("record # %d (fmt %d fld %d): %s\n",count,fmt,index,cp);
			free(cp);
			acc += rfdesc->field_sizes[index-1];
		}
    }
}

static void usage(void)

{
	fprintf(stderr,"Usage: dumpdf [-r root] filename\n");
	fprintf(stderr, "    -r database_root_directory\n");
	exit(-1);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
