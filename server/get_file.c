/* ***************************************************************
 *
 * PROCEDURE:	get_file.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Sun Jan 16 09:31:34 MST 2005
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */
/*
 * this routine gets a reference to a file, or creates 
 * a new entry in the hash table for the file and returns
 * the reference to it
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
#include <stdlib.h>
#include <string.h>
#include "srv_index.h"

typedef struct _open_files_ {
	int 				 _ref_cnt;		/* reference count */
	FILES				*_file;			/* pointer to file struct */
	struct _open_files_ * next;			/* next in chain */
} O_FILES;

static O_FILES *open_hashtab[128];		/* hash table of open files */
static pthread_mutex_t f_mutex = PTHREAD_MUTEX_INITIALIZER;

FILES *get_file(char *filename)
{

	unsigned char hash;

	char *cptr;

	O_FILES *optr;

	cptr = filename;
	hash = 0;
/*
 * do a simple xor hash of the name.  since we are using only
 * simple ascii text strings, the high bit will never be set
 * and the hash tab need to be only 128 entries long.
 * (but use a mod, just to be safe!)
 */
	while(*cptr)
		hash ^= *cptr++;
	hash %= 128;

	pthread_mutex_lock(&f_mutex);
/*
 * if there is not an entry for this hash slot, start a new one.
 * if the file is there, just increment the use count, and return
 * the reference to it.
 */
	if (open_hashtab[hash] == NULL) {
		open_hashtab[hash] = (O_FILES *)calloc(1, sizeof(O_FILES));
		optr = open_hashtab[hash];
	} else {
		optr = open_hashtab[hash];
		while(optr) {
			if (!strcmp(optr->_file->_fname, filename)) {
				optr->_ref_cnt++;
				pthread_mutex_unlock(&f_mutex);
				return(optr->_file);
			}
			optr = optr->next;
		}
/*
 * add a new member to the hash chain
 */
		optr = (O_FILES *)calloc(1, sizeof(O_FILES));
		optr->next = open_hashtab[hash];
		open_hashtab[hash] = optr;
	}
/*
 * save the informaion.
 */
	optr->_file = calloc(1,sizeof(FILES));
	optr->_file->_fname = strdup(filename);
	optr->_ref_cnt = 1;
	pthread_cond_init(&optr->_file->_lock.cond, NULL);
	pthread_mutex_init(&optr->_file->_lock.mutex, NULL);
	pthread_mutex_unlock(&f_mutex);
	return(optr->_file);
}

/*
 * this will dela with files that 'go out of scope' ie, the
 * index referring to them have closed.  if there are multiple
 * references to them, just decrement the use count, otherwise
 * free up the information and the hash entry too.
 */
void rm_file(char *name)
{
	int i;

	unsigned char hash;

	char *cptr;

	O_FILES *optr;
	O_FILES *bptr;

	FILEDESC *fdesc;

	cptr = name;
	hash = 0;
/*
 * hash the name
 */
	while(*cptr)
		hash ^= *cptr++;
	hash %= 128;

	pthread_mutex_lock(&f_mutex);
	if ((optr = open_hashtab[hash]) == NULL)
		goto done;
	
	bptr = NULL;
	while(optr) {
		if (!strcmp(optr->_file->_fname, name))
			break;
		bptr = optr;
		optr = optr->next;
	}
	if (!optr)
		goto done;

	if (!--optr->_ref_cnt) {
		free(optr->_file->_fname);
/*
 * free up the parsed file description.  if you have the parsed file
 * description, then you also have the binary one and it needs to
 * be freed as well, and close the open channel too.
 */
		if ((fdesc = optr->_file->_filedesc) != NULL) {
			for (i = 0; i < fdesc->n_rformats; i++)
				free(fdesc->record_desc[i].field_sizes);
			free(fdesc->record_desc);
			free(fdesc);
			free(optr->_file->_desc);
			close(optr->_file->_chan);
		}
		pthread_cond_destroy(&optr->_file->_lock.cond);
		pthread_mutex_destroy(&optr->_file->_lock.mutex);
		pthread_mutex_destroy(&optr->_file->_mutex);
		free(optr->_file);
/*
 * ok, remove the entry from the hash tab
 */
		if (!bptr)
			open_hashtab[hash] = optr->next;
		else
			bptr->next = optr->next;
		free(optr);
	}

done:
	pthread_mutex_unlock(&f_mutex);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
