/* ***************************************************************
 *
 * PROCEDURE:	version.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Thu Jan 20 19:08:28 MST 2005
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */

/*
 * this displays the version of dataman and exits.
 */

#include <stdlib.h>
#include <stdio.h>

static char dataman_version[] = "4.0.0";

void print_version(char *name)
{
	fprintf(stderr, "\n%s version %s: compiled %s at %s\n\n",
			name, dataman_version, __DATE__, __TIME__);
	exit(0);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
