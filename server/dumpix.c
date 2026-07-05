/* ***************************************************************
 *
 * PROCEDURE:	dumpix.c
 *
 * PROJECT:		dataman system utilities
 * 
 * DATE:		legacy, written in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */

/*
 * this routine does a dump of an index file to the standard output
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
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <inttypes.h>

#include "node.h"
#include "misc.h"

#define LEAF    0200                    /* the leaf bit mask */

char name[32];							/* name of index */
int pos[999];							/* the positions in the leve number */
int level;								/* our level in the index */
int chan;								/* channel of index */
int loop,count,tmp,acc,bytes,idx;		/* misc usage */
int64_t offs;								/* lseek offset */
int64_t cur_node;							/* posittion of current node */
int64_t prnt_node;							/* position of parent node */
short keylen;							/* internal length of key */
short fno;								/* number of files */
char **fnames;						/* files the index point to */
char file;

int cntr;

extern char *substr(char *, int ,int);	/* substring function */
extern short get_short(char *);
extern int64_t get_ll(void *);

static void usage(void);
static void write_node(char *);
static void all_done(void);

int main(int argc, char *argv[])

{

	int i;

	char path[512];
	char stuff[1024];						/* output buffer */
	char *root;
	char *key;								/* key to write out */

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

	if (!root && (root =  getenv("ROOT")) == NULL) {		/* get root path name */
		fprintf(stderr, "ROOT not defined!\n");
		exit(-1);
	}
	strcpy(path, root);
	strcat(path,"/index/");		/* tack on index sub dir */
	strcat(path,name);			/* tack on the index name */

	if ((chan = open(path,O_RDONLY|O_LARGEFILE)) < 0) {
		fprintf(stderr, "Can't open index named %s ", path);
		perror("");
		exit(errno);
	}
#if !defined __gnu_linux__
	check_endian();
#endif
	if (read(chan, stuff, INDEX_FILE_OFFSET) <  INDEX_FILE_OFFSET) {
		fprintf(stderr, "Can't read index header ");
		perror("");
	}

	keylen = get_short(stuff);					/* get key length */
	fno = get_short(stuff+sizeof(short));		/* get number of files */
	cur_node = get_ll(stuff+2*sizeof(short));	/* get position of root node */
	count = keylen + MISC_LEN;					/* length of internal key */

	if ((fnames = (char **)calloc(fno+1, sizeof(char *))) == NULL) {
		fprintf(stderr, "Can't allocate file name space");
		perror("");
		exit(errno);
	}
	for (idx = 0;idx <= fno;idx++) {    /* get the names of the files */
		for(i = 0; ; i++) {
			if (read(chan, stuff+i, 1) < 1) {
				fprintf(stderr,"Error reading file name number %d\n",idx);
				exit(errno);
			}
			if (*(stuff+i) == '\0')
				break;
		}
		fnames[idx] = strdup(stuff);
    }

	level= 0;
	printf("Index is a member of %d datasets\n",fno+1);
	cntr = 0;

	while(1) {								/* could take a long time */
		for ( ;level < 999;level++) {		/* assumes max of */
			llseek(chan,cur_node,SEEK_SET);			/* 999 levels in idx */
			pos[level] = 0;
			if (read(chan,stuff,1) < 1) {
				fprintf(stderr, "Can't read node desc byte ");
				perror("");
				exit(errno);
			}
			if (*stuff & LEAF)
				break;
			llseek(chan, (int64_t)(N_KEYS*(keylen+MISC_LEN)), SEEK_CUR);
			if (read(chan, (char *)&cur_node, PTR_LENGTH) < PTR_LENGTH) {
				fprintf(stderr, "Cant read node %"PRId64" ", cur_node);
				perror("");
				exit(errno);
			}
			cur_node = get_ll((char *)&cur_node);
		}

		bytes = N_KEYS * count;
		if (read(chan, stuff, bytes+PTR_LENGTH) < bytes+PTR_LENGTH) {       /* get keys */
			fprintf(stderr, "Can't read key buffer ");
			perror("");
			exit(errno);
		}
		prnt_node = get_ll(stuff+bytes);		/* get parent node */

		write_node(stuff);
		if (prnt_node == 0)
			all_done();                         /* the root node is a leaf */
		acc = 1;

		while(acc) {                            /* get to next position */
			level--;
			llseek(chan, prnt_node+1, SEEK_SET);          /* get to parent node */
			bytes = N_KEYS * count;
			acc = 0;
			if (read(chan,stuff,bytes) < bytes) {
				fprintf(stderr, "Can't read parent node keys ");
				perror("");
				exit(errno);
			}
			tmp = pos[level]  * count;
			key = substr(stuff, tmp, tmp+count-1);
			if ((*key == '\0') || (pos[level] > N_KEYS)) {
				llseek(chan, (int64_t)(N_KIDS * PTR_LENGTH), SEEK_CUR);
				if (read(chan, stuff, PTR_LENGTH) < PTR_LENGTH) {
					fprintf(stderr, "Can't read child pointer ");
					perror("");
					exit(errno);
				}
				prnt_node = get_ll(stuff);
				if (prnt_node == 0 || level == 0)
					all_done();                  /* this function exits */
				else
					acc = 1;
				free(key);
			}
		}

		cur_node = prnt_node;                           /* current node */
		offs = *(key+keylen) - 1;                       /* file offset */
		prnt_node = get_ll(key+keylen+1);		/* get rec pointer */
		*(key+keylen) = '\0';                           /* terminate key */
        
		printf("key %s, file %s, pointer %"PRId64"\n", key, fnames[offs], prnt_node);
		cntr++;
		free(key);

		offs = PTR_LENGTH * (pos[level]+1);
		llseek(chan, offs, SEEK_CUR);
		if (read(chan, (char *)&cur_node, PTR_LENGTH) < PTR_LENGTH) {
			fprintf(stderr, "Can't read node pointer ");
			perror("");
			exit(errno);
		}
		cur_node = get_ll((char *)&cur_node);

		pos[level] += 1;
		level++;
	}
}

static void usage(void)
{
	fprintf(stderr, "Usage: dumpix [-r root] idx_name\n");
	fprintf(stderr, "    -r database_root_directory\n");
	exit(-1);
}

static void write_node(char *stuff)

{
	int64_t ptr;
	register int i,j,idx;
	char *key;

	for (idx=0; idx < N_KEYS; idx++) {
		i = idx * count;					/* offset to key */
		j = *(stuff+i+keylen) - 1;
		ptr = get_ll(stuff+i+keylen+1);		/* the rec pointer */
		key = substr(stuff, i, i+keylen-1);	/* key */
		if (*key == '\0') {
			free(key);
			break;
		}
		printf("key %s, file %s, pointer %"PRId64"\n",key,fnames[j],ptr);
		cntr++;
		free(key);
	}
}


static void all_done(void)                      /* this is if finished with endex */
{
	fprintf(stderr,"Keys dumped: %d\nNormal termination\n",cntr);
	exit(0);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
