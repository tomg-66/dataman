/* ***************************************************************
 *
 * PROCEDURE:	mkdf.c
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
 * 				Thu Mar 10 18:51:20 MST 2005
 * 				changed the value for size of a field to allow
 * 				zero.  a zero length field indicates that this
 * 				field is a blob
 * 				tomg
 *
 * 				Fri Aug 12 12:55:44 MDT 2005
 * 				changed to use 64 bit offsets for making a big
 * 				database
 * 				tomg
 *
 *				Sat Jul 30 17:29:17 MDT 2005
 *				changed offsets to defines in misc.h for change
 *				to 64 bit offsets.
 *				tomg
 *
 *				Mon Apr 14 19:12:41 MDT 2008
 *				had to delete old blobs that might be associated
 *				with a previous instance of this new file.
 ************************************************************* */

/*
 * this is a routine that creates a forward and reverse linked list as a 
 * data file
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

#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <inttypes.h>

#include <string.h>
#include <stdlib.h>

#include "misc.h"

#define PMODE   0666			/* default file creation mask */
#define MAXLINE 10240			/* max length of inpt buffer line */

int pos, numflds;				/* these don't need to be longs */
short field[10240];				/* file description */

extern void put_ll(void *, int64_t);
extern void put_short(void *, short int);

int main(int argc, char *argv[])

{
	FILE *inp_file;

	int64_t next_rec,cur_rec,prev_rec;		/* record links */
	int64_t last_rec,tmp,kount;				/* general usage */
	int64_t sav;							/* saved pos of last rec ptr */

	int new_file;							/* channel of new file */
	int msk;								/* open mask */
	int longest;							/* longest record length */

	char *new_nam,*inp_nam;					/* file names */
	char cfmt;								/* format number */
	char *substr();							/* substring function */
	char *tst;								/* pointer recieved by substr */
	char buff[MAXLINE];						/* buffer from input file */
	char *stuff;							/* output buffer */
	char *root;								/* path name to et root */
	char path[1024];						/* path name to file */

	char *cptr;
	char *eptr;

	short acc,count,len,fmtcnt,index,i,flen,idx;        /* misc usage vars */

	void usage(void);
	void delfile(char *);
	int reclen(int);				/* get the length of this record */

	if (argc != 3)
		usage();

#if !defined __gnu_linux__
	check_endian();
#endif

	new_nam = strdup(*(++argv));
//	strcpy(new_nam,*(++argv));				/* new file name */
	inp_nam = strdup(*(++argv));
//	strcpy(inp_nam,*(++argv));				/* inpt file name */
	i = strlen(inp_nam);
	tst = substr(inp_nam,i-2,i);
	if (strcmp(tst,".i") != 0)				/* make sure .i extension */
		usage();
	free(tst);

	root = getenv("ROOT");			/* get dirs where file goes */
	if (root == NULL) {
		fprintf(stderr, "ROOT not defined\n");
		exit(-1);
	}

	strcpy(path, root);
	strcat(path,"/files/");		/* put on the files dir */ 
	strcat(path,new_nam);			/* put on the file name */
/*
 * open/create the new data file
 */
	msk = O_CREAT | O_RDWR | O_TRUNC | O_LARGEFILE;
	if ((new_file = open(path,msk,S_IREAD|S_IWRITE)) < 0) {
		fprintf(stderr, "\nError creating file %s, terminating",path);
		perror("");
		exit (errno);
	}
/*
 * remove any blobs associated with a previous version of a
 * file by this name
 */
	sprintf(path, "rm -f %s/blobs/%s.*.*.*", root, new_nam);
	if (system(path) < 0) {
		fprintf(stderr, "Can't delete blobs associated with previous version of this file\n");
		delfile(new_nam);
	}
/*
 * open up the initialization file
 */
	if ((inp_file = fopen(inp_nam,"r")) == NULL) {
		fprintf(stderr,"Cannot open file named %s\n",inp_nam);
		delfile(new_nam);
	}
/*
 * get the number of record formats to be contained in this data file
 */
	printf("number of formats: ");
	if (scanf("%hd",&acc) < 1) {
		fprintf(stderr, "Need to enter the number of record formats this file will contain!\n");
		delfile(new_nam);
	}
	if (acc < 1 || acc > 63)
		delfile(new_nam);
	field[0] = acc;
	flen = sizeof(short);
	longest = 0;
/*
 * now, for each record format, we need to get it's description.
 * that would be how many fields(columns) and the length of each
 * field.
 */
	for (count = 1,idx = 1;count <= acc;count++) {
		len = 0;
		printf("number of fields for fmt %d: ",count);
		if (scanf("%hd",&fmtcnt) < 1) {
			fprintf(stderr, "Need to know how many fields for the record format!\n");
			delfile(new_nam);
		}
		fflush(stdout);
		if (fmtcnt < 1 || fmtcnt > 999)
			delfile(new_nam);
		field[idx] = fmtcnt;
		idx++;
		for (index = 1;index <= fmtcnt;index++) {
			printf("\tlength of field %d: ",index);
			if (scanf("%hd",&i) < 1) {
				fprintf(stderr, "Need to have the length of each field in the format!\n");
				delfile(new_nam);
			}
			fflush(stdout);
			if (i < 0 )
				delfile(new_nam);
			field[idx+index] = i;
			len = len + i;
		}
		longest = len > longest? len : longest;
		field[idx] = len;
		idx = idx + fmtcnt + 1;
		flen = flen + (fmtcnt*2) + 4;
		if (idx >= 10240) {
			fprintf(stderr,"Too many field descriptions defined\n");
			delfile(new_nam);
		}
	}
	last_rec = flen + 2;
	sav = last_rec + sizeof(int64_t);

	put_short(buff, flen);			/* save the header length */
	for(i = 0; i <= flen/2; i++)		/* copy in the file desc */
		put_short(buff+(i+1)*2, *(field+i));

	if (write(new_file,buff,flen+2) < flen+2)   /* write out file description */
		delfile(new_nam);

	last_rec = last_rec + 2 * sizeof(int64_t);
	next_rec = last_rec;
	prev_rec = last_rec;
	cur_rec = 0;
	put_ll(buff,last_rec);						/* pointer to "first record */
	put_ll(buff+sizeof(int64_t),last_rec);	/* pointer to "last" record */

	if (write(new_file,buff,2 * sizeof(int64_t)) < 2 * sizeof(int64_t))     /* write out pointers */
		delfile(new_nam);
	if ((stuff = malloc(longest+DATARECORD_HEADER_LENGTH)) == NULL)
		delfile(new_nam);

	for (count = 1;fgets(buff,MAXLINE,inp_file) != NULL;count++) {
/*
 * all lines in the init file have the format of:
 * 	fmt#:field1:field2::field4::\n
 * a format number followed by a colon as a field seperator.
 * empty fields have no data between the colons.  any fields
 * left after the last colon in the init file are blank filled.
 * and of course a trailing colon before the last newline.
 * you can't have a blob in the initialization file.
 * so there is allways at least 1 colon on a line.
 * if there is a colon that is a part of the data escape it with
 * a backslash '\'.
 */
		cptr = strchr(buff, ':') + 1;
		cfmt = atoi(buff);
		if (cfmt > field[0] || cfmt < 1) {
			fprintf(stderr,"Record format out of bounds, record %d\n",count);
			delfile(new_nam);
		}

		prev_rec = cur_rec;				/* current becomes previous */
		cur_rec = next_rec;				/* and next current */

		len = reclen(cfmt);				/* get the length of this record */
		next_rec = cur_rec + DATARECORD_HEADER_LENGTH + len;
		stuff[0] = cfmt;				/* format number */

		put_ll(stuff+OFFSET_TO_PREV,prev_rec);		/* save pointer to prev rec */
		put_ll(stuff+OFFSET_TO_NEXT,next_rec);		/* and the pointer to next */

		index = DATARECORD_HEADER_LENGTH;			/* point past header */
		tmp = 0;

		if (*(cptr) == '\n') {
			memset(stuff+DATARECORD_HEADER_LENGTH,' ',len);		/* empty record */
		} else {
			for (kount = 1;kount <= numflds;kount++) {
				if (*(cptr) == '\n') {
					memset(stuff+index,' ',len-tmp);
					break;
				}
				eptr = strchr(cptr, ':');
				while(1) {
					if (*(eptr-1) == '\\') {
						memmove(eptr-1, eptr, strlen(eptr)+1);
						eptr = strchr(eptr, ':');
					} else
						break;
				}
				i = eptr - cptr;
				if (i != 0 && field[pos] == 0) {
					fprintf(stderr, "Can't put data in blob field in init "
									"file, record %hd, field %"PRId64"\n", count,
									kount);
					delfile(new_nam);
				}
				if (i == 0) {
					memset(stuff+index, ' ', field[pos]);
					cptr++;
				} else if (i > field[pos]) {
					fprintf(stderr, "Field too long, record %hd, field %"PRId64"\n",
									count, kount);
					printf("buff = %s\n", buff);
					delfile(new_nam);
				} else if (i < field[pos]) {
					memset(stuff+index, ' ', field[pos]);
					memcpy(stuff+index, cptr, i);
					cptr = eptr + 1;
				} else {
					memcpy(stuff+index, cptr, i);
					cptr = eptr + 1;
				}
				index += field[pos];
				tmp += field[pos];
				pos++;
			}
		}
		if (write(new_file,stuff,len+DATARECORD_HEADER_LENGTH) != len+DATARECORD_HEADER_LENGTH) {
			fprintf(stderr,"Error writing output buffer ... terminating\n");
			delfile(new_nam);
		}
	}
	llseek(new_file, cur_rec+OFFSET_TO_NEXT, SEEK_SET);
	cur_rec = 0;
	put_ll(buff,cur_rec);			/* point to null because end */

	if (write(new_file,buff,sizeof(int64_t)) != sizeof(int64_t)) {
		fprintf(stderr,"Error writing last pointer\n");
		delfile(new_nam);
	}

	put_short(buff, longest);			/* pointer to last */
	llseek(new_file, sav, SEEK_SET);			/* pos to save longest rec */
	if (write(new_file,buff,sizeof(short)) != sizeof(short)) {
		fprintf(stderr,"Error writing pointer to last \n");
		delfile(new_nam);
	}
	printf("normal termination\n");
	return(0);
}


int reclen(fmt)
int fmt;

{
	int temp;
   	for (temp = 1,pos = 1;temp < fmt;temp++) 
		pos = pos + field[pos] + 2;

	temp = field[pos+1];
	numflds = field[pos];
	pos += 2;
	return (temp);
}

void usage()

{
	fprintf(stderr,"Usage: mkdf new_file input_file.i\n");
	exit (-1);
}

void delfile(name)
char *name;

{
	fprintf(stderr,"Error encountered during MKDF ... deleting file %s\n",name);
	unlink(name);
	exit (-1);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
