/* this is a routine to sequentially dump the nodes of an index file */

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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <inttypes.h>

#include "node.h"
#include "misc.h"

#define LEAF    0200

static void usage(void);

int main(int argc, char *argv[])
{
    int bytes,chan,i,j,offs;

    short keylen,acc;
    short get_short(char *);

    int64_t temp,ptr,prnt, pos;
    int64_t get_ll(char *);

    char buff[1024],*cptr;
	char *substr(char *, int, int);

    if (argc != 2)
        usage();
	if ((cptr = getenv("ROOT")) == NULL)
		strcpy(buff, *(++argv));
	else
		sprintf(buff, "%s/index/%s", cptr, *(++argv));

    if ((chan = open(buff,O_RDONLY)) < 0) {
        fprintf(stderr,"Can't open index %s\n",*argv);
        exit(0);
    }

#if !defined __gnu_linux__
	check_endian();
#endif

    if(read(chan, buff, INDEX_FILE_OFFSET) < INDEX_FILE_OFFSET) {
        fprintf(stderr,"Can't read index header info\n");
        exit(0);
    }
    keylen = get_short(buff);

    printf("key length is %d\n",keylen);
    acc = get_short(buff+sizeof(short));
    printf("number of files = %d\n",acc+1);
    temp = get_ll(buff+INDEX_HEADER_LENGTH);
    printf("root node is %"PRId64"\n",temp);
	pos = INDEX_FILE_OFFSET;
    for (temp = 0;temp <= acc;temp++) {
		for (i = 0; ;i++) {
			read(chan, buff+i, 1);
			pos++;
			if (*(buff+i) == '\0')
				break;
		}
        printf("file number %"PRId64" is %s\n",temp+1,buff);
    }
    bytes = N_KEYS * (keylen + MISC_LEN) + 1;
    offs = keylen + MISC_LEN;

    while (1) {
		printf("\n\nfile position is %"PRId64"\n",pos);
		if (read(chan,buff,bytes) < bytes)
			break;
		if (*buff & LEAF) {
			pos = pos + N_KEYS * (keylen + MISC_LEN) + MISC_LEN;
			printf("this node is a leaf\n");
		} else {
			printf("this node is not a leaf\n");
			pos = pos + N_KEYS * (keylen +MISC_LEN) + (N_KIDS * PTR_LENGTH) + MISC_LEN;
		}
		for(temp = 0;temp < N_KEYS;temp++) {
			i = temp * offs + 1;
			j = i + keylen - 1;
			cptr = substr(buff,i,j);
		    ptr = get_ll(buff+j+2);
			printf("key number %"PRId64" is %s\tfile # is %d\t record pointer is %"PRId64"\n"
							,temp,cptr,*(buff+j+1),ptr);
			free(cptr);
		}
		if (!(*buff & LEAF)) {
			if (read(chan,buff,N_KIDS * PTR_LENGTH) != N_KIDS * PTR_LENGTH) {
                fprintf(stdout,"error reading kid pointers\n");
                exit(0);
			}
			for (temp = 0;temp < N_KIDS; temp++) {
				ptr = get_ll(buff+temp*PTR_LENGTH);
                printf("kid pointer # %"PRId64" is %"PRId64"\n",temp,ptr);
			}
		}
		if (read(chan,buff,sizeof(int64_t)) < sizeof(int64_t)) {
			fprintf(stderr,"error reading parent node\n");
			exit(0);
		}
		prnt = get_ll(buff);
		printf("parent node is %"PRId64"\n",prnt);
	}
}

static void usage(void)
{
	fprintf(stderr,"Usage: dnodes idx_name\n");
	exit(0);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker ft=c:
 */
