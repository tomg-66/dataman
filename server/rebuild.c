/* ***************************************************************
 *
 * PROCEDURE:	rebuild.c
 *
 * PROJECT:		dataman utilities
 * 
 * DATE:		legacy, originally writtin in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Fri Aug 24 10:43:00 MDT 2007
 *				Tom Green
 *				re-wrote for *nix based and uses the newer
 *				functions.
 *
 *				Wed Feb 13 19:03:33 MDT 2008
 *				Tom Green
 *				this didn't update blobs when the file was re-
 *				built so that they would have the old record
 *				numbers in the file name.  had to fix that up.
 *				tomg
 *
 *				Sat Jul 25 09:09:22 MDT 2009
 *				removed reference to the freelist
 *				tomg
 *
 ************************************************************* */
/*
 * this procedure rebuilds a data file, cleaning out the empty space, and
 * ordering the the file's physical structure to meet the logical structure.
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

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include <malloc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "file_desc.h"
#include "misc.h"

#include <sys/mman.h>

extern short get_short(char *);
extern void put_short(char *, short);
extern int64_t get_ll(void *);
extern void put_ll(void *, int64_t);

typedef struct mapping {
	int64_t old;
	int64_t new;
	int fno;
	struct mapping *next;
} MAPPING;

static void err(char *str, char *fname)
{
	perror(str);
	unlink(fname);
}

/*
 * since we've already done a stat on the source file we are passed the
 * length of the data we are copying
 */
int copy_file(char *src, char *dest, off_t len)
{
	int ifd, ofd;
	int ret;
	char *buff;

	ret = 0;
	ifd = -1;
	ofd = -1;
	buff = MAP_FAILED;
	if ((ifd = open(src, O_RDWR)) < 0)
		goto fail;
	if ((ofd = open(dest, O_RDWR|O_EXCL|O_CREAT|O_TRUNC, 0666)) < 0)
		goto fail;

	if ((buff = mmap(NULL, len, PROT_READ, MAP_PRIVATE, ifd, 0)) == MAP_FAILED)
		goto fail;
	if (write(ofd, buff, len) != len)
		goto fail;
	ret = 1;
fail:
	if (ifd > -1)
		close(ifd);
	if (buff != MAP_FAILED)
		munmap(buff, len);
	if (ofd > -1)
		close(ofd);
	if (!ret && errno != EEXIST)
		unlink(dest);
	return(ret);
}

void rebuild(char *root, char *filename, FILES *fptr)
{
	int i;
    int chan;					/* channel for dest file */
    int len;					/* record length */
	int fno;

	short *m_desc;				/* tmp buffer for header */

    int64_t prev,				/* prev rec pointer */
		 next,					/* next rec pointer */
		 curr;					/* current rec pointer */

    loff_t seekpt;				/* point to seek to */


	char old_name[1024], new_name[1024];
	char fmt;
    char *tmp_file;				/* temporary file name */
    char *buff;					/* write buffer */

	MAPPING *head, *link;

	struct stat stbuf;

	RFDESC *rptr;

	head = NULL;
	len = fptr->_longest + DATARECORD_HEADER_LENGTH;
	if (fptr->_hlen > len)
		len = fptr->_hlen;
	if ((buff = malloc(len)) == NULL) {
		fprintf(stderr, "Can't allocate read/write buffer in rebuild:");
		perror("");
		exit(0);
	}

    tmp_file = malloc(strlen(fptr->_fname)+5);		/* space for tmp name */
    strcpy(tmp_file, fptr->_fname);					/* copy in the name */
    strcat(tmp_file, "$tmp");						/* cat on the temp descript */

    if ((chan = open(tmp_file,O_CREAT|O_TRUNC|O_RDWR|O_LARGEFILE,0666)) == -1) {
        printf("Can't create file %s\n",tmp_file);      /* error */
        exit(0);
    }

	put_short(buff, fptr->_hlen);
	if ((m_desc = (short *)malloc(fptr->_hlen)) == NULL) {
		fprintf(stderr, "Can't allocate tempoary header buffer");
		perror("");
		exit(0);
	}
	for (i = 0; i < fptr->_hlen/2; i++)
		*(m_desc+i) = get_short((char *)(fptr->_desc+i));
	if (write(chan, buff, sizeof(short)) < sizeof(short)) {				/* write the header length */
		fprintf(stderr, "Error writing header length\n");
		goto fail;
	}
	if (write(chan, (char *)m_desc, fptr->_hlen) < fptr->_hlen) {		/* write the header */
		fprintf(stderr, "Error writing the header\n");
		goto fail;
	}
	free(m_desc);

	curr = fptr->_hlen + sizeof(short) + 2*sizeof(int64_t);
	put_ll(buff, curr);
	put_ll(buff+sizeof(int64_t), curr);
	if (write(chan, buff, 2*sizeof(int64_t)) < 2*sizeof(int64_t)) {			/* write pointers to first and last */
		fprintf(stderr, "Error writing initial pointers\n");
		goto fail;
	}

	seekpt = fptr->_hlen + sizeof(short);
	llseek(fptr->_chan, seekpt, SEEK_SET);
	prev = 0;
	next = 0;
	seekpt = 0;
	if (read(fptr->_chan, (char *)&seekpt, sizeof(int64_t)) != sizeof(int64_t)) {
		err("Can't read first record pos", tmp_file);
		goto fail;
	}
/*
 * get position of first logical record in the file
 */
    if (read(fptr->_chan, (char *)&seekpt, sizeof(loff_t)) != sizeof(loff_t)) {
        err("Can't read first record position", tmp_file);
		goto fail;
	}
    seekpt = get_ll((char *)&seekpt);			/* convert the seekpt */
/*
 * go until the logical end of the file
 */
    while(seekpt) {								/* do as long as possible */
		llseek(fptr->_chan, seekpt, SEEK_SET);	/* get to current record */
        if (read(fptr->_chan, buff, DATARECORD_HEADER_LENGTH) != DATARECORD_HEADER_LENGTH) {
            err("Can't read record header in rebuild", tmp_file);
			goto fail;
		}

		rptr = &(fptr->_filedesc->record_desc[*buff-1]);
/*
 * if this record can have blobs, look and see if there are any.  if so
 * make a copy that will refer to the updated record number, and make the
 * old name a temp.
 */
		if (seekpt != curr && rptr->has_blob) {
			i = rptr->has_blob;
			fmt = *buff;
			for (fno = 0; fno < rptr->n_fields && i; fno++) {
				if (rptr->field_sizes[fno] == 0) {
					i--;
					sprintf(old_name, "%s/blobs/%s.%d.%"PRId64".%d", root, filename, fmt, seekpt, fno);
					if (stat(old_name, &stbuf) < 0)
						continue;
					if (!head) {
						head = malloc(sizeof(MAPPING));
						link = head;
					} else {
						link->next = malloc(sizeof(MAPPING));
						link = link->next;
					}
					link->old = seekpt;
					link->new = curr;
					link->fno = fno;
					link->next = NULL;
					sprintf(new_name, "%s/blobs/%s.%d.%"PRId64".%d$new", root, filename, fmt, curr, fno);
					if (!copy_file(old_name, new_name, stbuf.st_size)) {
						err("Can't back up blob file", tmp_file);
						goto fail;
					}
					strcpy(new_name, old_name);
					strcat(new_name, "$tmp");
					rename(old_name, new_name);
				}
			}
		}
        len = rptr->rf_len;
		seekpt = get_ll(buff+OFFSET_TO_NEXT);
		put_ll(buff+OFFSET_TO_PREV, prev);
        if (seekpt == 0)                        /* at end of data file? */
            next = 0;                           /* no next rec */
        else
			next = curr+len+DATARECORD_HEADER_LENGTH;
		put_ll(buff+OFFSET_TO_NEXT, next);

		if (read(fptr->_chan, buff+DATARECORD_HEADER_LENGTH, len) != len) {
			err("Can't read record in rebuild", tmp_file);
			goto fail;
		}
		len += DATARECORD_HEADER_LENGTH;
        if (write(chan, buff, len) != len) {		/* write new rec */
            err("Can't write record in rebuild", tmp_file);
			goto fail;
		}
		prev = curr;
		curr = next;
    }
    close(chan);							/* close tmp file */
	close(fptr->_chan);						/* close source file */
	rename(tmp_file, fptr->_fname);			/* rename it */
	while(head) {
		sprintf(old_name, "%s/blobs/%s.%d.%"PRId64".%d$tmp", root, filename, fmt, head->old, head->fno);
		unlink(old_name);
		sprintf(old_name, "%s/blobs/%s.%d.%"PRId64".%d$new", root, filename, fmt, head->new, head->fno);
		sprintf(new_name, "%s/blobs/%s.%d.%"PRId64".%d", root, filename, fmt, head->new, head->fno);
		rename(old_name, new_name);
		link = head->next;
		free(head);
		head = link;
	}
	return;

fail:
/*
 * restore any temporay blob files
 */
	while(head) {
		sprintf(old_name, "%s/blobs/%s.%d.%"PRId64".%d$tmp", root, filename, fmt, head->old, head->fno);
		sprintf(new_name, "%s/blobs/%s.%d.%"PRId64".%d", root, filename, fmt, head->old, head->fno);
		rename(old_name, new_name);
		link = head->next;
		free(head);
		head = link;
	}
	exit(0);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
