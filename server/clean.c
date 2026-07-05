/* ***************************************************************
 *
 * PROCEDURE:	clean.c
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
 ************************************************************* */
/*
 * this program reads the file named Cleanfile if no arg is passed. if arg is
 * passed it is taken to be the cleanfile.  It reads the cleanfile to find
 * the root to the database to clean up.  Then, it checks each named file for
 * the amount of space empty in it.  If the file is more than 15% empty, the
 * file gets rebuilt.  This is done for each named file.  After all of the
 * files are checked and rebuilt, each index is rebuilt.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <fcntl.h>
#include <libgen.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

#include <sys/types.h>

#define MAX_DIRTY	   5			   /* clean file when this percent unused */

int dbgsw;
static char *verstr = "0.0.2";

extern int check_it(char *, char *, int);

void usage(char *name)
{
	char str[512];
	char *copy;

/*
 * strdup can modify the buffer, so you want to pass it a copy
 */
	copy = strdup(name);
	strcpy (str, basename(copy));
	free(copy);
	fprintf(stderr, "%s: usage: %s [-v | -f filename -p min -r root]\n"
			"    -f filename where filename is the cleanfile definition\n"
			"    -p min where min is the minimum allowable fragmentation\n"
			"        if the fragmentation is less than or equal to this \n"
			"        percentage the file will not optimised\n"
			"    -r root override the root specified in the cleanfile\n"
			"    -v print version and exit\n\n", str, str);
	exit(1);
}


char *get_filename(char **src)
{
	char *cptr;
	char *ret;

	cptr = *src;
	ret = *src;
	while(*cptr && isspace(*cptr))
		cptr++;
	if (!*cptr)
		return(NULL);

	while(*cptr && !isspace(*cptr))
		cptr++;
	if (*cptr)
		*(cptr++) = '\0';
	while(*cptr && isspace(*cptr))
		cptr++;
	*src = cptr;
	return(ret);
}

int get_command(char *dest, char **src)
{
	int len;

	char *cptr;
	char *eptr;

	cptr = *src;
	while(*cptr && *cptr != '\n' && isspace(*cptr))
		cptr++;
	if (!*cptr || *cptr == '\n') {
		*src = cptr;
		return(0);
	}
	if ((eptr = strchr(cptr, '\n')) == NULL)
		eptr = cptr+strlen(cptr);
	while(!isspace(*eptr))
		eptr--;
	len = eptr-cptr;
	memcpy(dest, cptr, len);
	*(dest+len) = '\0';
	*src = ++eptr;
	return(len);
}


int main(int argc, char *argv[])
{
	int i, j;
	int max = MAX_DIRTY;
	int fd;
	int sw;

	char cmd[512];
	char fname[512] = "Cleanfile";
	char rootpath[256];
	char envstr[260];
	char filename[512];
	char *fbuffer;
	char *files;
	char *cptr, *eptr;		// current pointer, end pointer

	*rootpath = '\0';
	for (i = 1; i < argc; i ++) {
		if (*argv[i] != '-')
			usage(argv[0]);
		for (j = 1; j < strlen(argv[i]); j++) {
			switch(argv[i][j]) {
				case 'f':
					if (argv[i][++j])
						usage(argv[0]);
					if (++i >= argc)
						usage(argv[0]);
					strcpy(fname, argv[i]);
					j = strlen(argv[i]);
					break;
				case 'p':
					if (argv[i][++j])
						usage(argv[0]);
					if (++i >= argc)
						usage(argv[0]);
					max = atoi(argv[i]);
					if (!max)
						usage(argv[0]);
					j = strlen(argv[i]);
					break;
				case 'r':
					if (argv[i][++j])
						usage(argv[0]);
					if (++i >= argc)
						usage(argv[0]);
					strncpy(rootpath, argv[i], 254);
					j = strlen(argv[i]);
					break;
				case 'v':
					printf("\n%s version %s, compiled %s at %s\n\n",
							basename(argv[0]), verstr, __DATE__, __TIME__);
					exit(0);
				default:
					fprintf(stderr, "\nunknown switch: '%c'\n", argv[i][j]);
					usage(argv[0]);
				case 'h':
				case '?':
					fprintf(stderr, "\n");
					usage(argv[0]);
			}
		}
	}


	if(max > 25) {
		fprintf(stderr, "Can't leave a file more than %%25 fragmented, ignoring\n");
		max = 25;
	}

	if ((fd = open(fname, O_RDONLY)) < 0) {
		fprintf(stderr, "Can't open cleanfile named %s: ", fname);
		perror("");
		exit(2);
	}

	j = lseek(fd, 0l, SEEK_END);
	lseek(fd, 0l, SEEK_SET);
	if ((fbuffer = malloc(j+1)) == NULL) {
		fprintf(stderr, "Can't allocate buffer to read cleanfile %s: ", fname);
		perror("");
		exit(3);
	}
	if (read(fd, fbuffer, j) < j) {
		fprintf(stderr, "Can't read cleanfile %s: ", fname);
		perror("");
		exit(4);
	}
	*(fbuffer+j) = '\0';
	j++;
	close(fd);
/*
 * strip out the comments
 */
	cptr = fbuffer;
	while((cptr = strchr(cptr, '#')) != NULL) {
		eptr = strchr(cptr, '\n');
		if (cptr != fbuffer && *(cptr-1) == '\n')
			cptr--;
		j -= eptr-cptr;
		memmove(cptr, eptr, j);
	}
/*
 * strip out line joins
 */
	cptr = fbuffer;
	while((eptr = strstr(cptr, "\\\n")) != NULL) {
		j = strlen(eptr+1);
		memcpy(eptr, eptr+2, j);
		cptr = eptr;
	}
/*
 * get the database root directory if not specified on cmd line
 */
	if (!strlen(rootpath)) {
		if ((cptr = strstr(fbuffer, "ROOT")) == NULL) {
			if ((cptr = getenv("ROOT")) == NULL) {
				fprintf(stderr, "No database ROOT specified\n");
				exit(5);
			}
		} else {
			cptr += 4;
			while(isblank(*cptr))
				cptr++;
			if (*cptr != ':') {
				fprintf(stderr, "No colon seperator after ROOT\n");
				exit(0);
			}
			cptr++;
		}
		while(isblank(*cptr))
			cptr++;
		eptr = cptr;
		while(*eptr && !isspace(*eptr))
			eptr++;
		i = eptr - cptr;
		if (i > 254) {
			fprintf(stderr, "length of the ROOT path is too long (max 254)\n");
			exit(0);
		}
		memcpy(rootpath, cptr, i);
		*(rootpath+i) = '\0';
	}
	if (strlen(rootpath) > 254) {
		fprintf(stderr, "length of the ROOT path is too long (max 254)\n");
		exit(0);
	}
/*
 * set this to be the environemnt...
 */
	strcpy(envstr, "ROOT=");
	strcat(envstr, rootpath);
	if (putenv(envstr)) {
		fprintf(stderr, "Can't set ROOT envrionment:");
		perror("");
		exit(0);
	}
/*
 * start looking for the files....
 */
	if ((cptr = strstr(fbuffer, "FILES")) == NULL) {
		fprintf(stderr, "FILES section not specified!\n");
		exit(0);
	}
	cptr += 5;
	while(isblank(*cptr))
		cptr++;
	if (*cptr != ':') {
		fprintf(stderr, "Missing COLON seperator after FILES keyword\n");
		exit(0);
	}
	cptr++;
	while(isblank(*cptr))
		cptr++;
	if (isspace(*cptr)) {
		fprintf(stderr, "No files specified in FILES section\n");
		exit(0);
	}

	eptr = strchr(cptr, '\n');
	if (!eptr)
		eptr = cptr+j;
	i = eptr-cptr;
	if ((files = malloc(i+1)) == NULL) {
		fprintf(stderr, "Can't allocate space to hold file names:");
		perror("");
		exit(0);
	}
	memcpy(files, cptr, i);
	*(files+i) = '\0';

	strcpy(filename, rootpath);
	strcat(filename, "/files/");
	cptr = filename+strlen(filename);
	eptr = files;
/*
 * check each file in the list
 */
	sw = 0;
	while(1) {
		if ((cptr = get_filename(&eptr)) == NULL)
			break;
		sprintf(filename, "%s/files/%s", rootpath, cptr);
		printf("Checking file %s\n", filename);
		if (check_it(rootpath, cptr, max) && !sw)
			sw = 1;
	}
	printf("Done with files\n");
/*
 * find the INDEX keyword and execute each command line up
 * until a \n\n or end of file
 */
	if (!sw) {
		printf ("No files modified, not rebuilding indexes\n");
		exit(0);
	}
	if ((cptr = strstr(fbuffer, "INDEX")) == NULL)
		exit(0);								/* nothing to do */
	cptr += 5;
	while (*cptr && *cptr != '\n' && isspace(*cptr))
		cptr++;
	if (!*cptr || *cptr == '\n')
		exit(0);
	if (*cptr != ':') {
		fprintf(stderr, "Missing COLON seperator after INDEX keyword\n");
		exit(0);
	}
	cptr++;
/*
 * find out if there is more to this line, else start reading on the next
 */
	while (*cptr && *cptr != '\n' && isspace(*cptr))
		cptr++;
	if (!*cptr)
		exit(0);
	if (*cptr == '\n')
		cptr++;
	while (get_command(cmd, &cptr)) {
		if ((i = system(cmd)) < 0) {
			fprintf(stderr, "failed command %s", cmd);
			exit(errno);
		}
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
