/* ***************************************************************
 *
 * PROCEDURE:	match.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 *				Thu Mar 21 16:07:37 MDT 2013
 *				tomg
 *				changed how match strings with no wild cards.  it
 *				runs -much- faster now.
 ************************************************************* */

/* this is a pattern matching routine. The wild card is '*'. This will match
 * any single character.
 * usage is:   var = match(text,temp),   where var is an integer that recieves
 * -1 if the string is less than the template, 0 if the string matches the
 * template, and 1 if the string is greater than the template.  argument 1 is
 * the string to be tested, and argument 2 is the mask (template).
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

#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <arpa/inet.h>		/* byteorder functions */


int match(text,temp,len)
register char text[];                           /* test string */
register char temp[];                           /* template string */
int len;										/* max length for search */

{

	register int ix1 = 0;		/* index into string text */
	register int ix2 = 0;		/* index into string temp */
	register char ch1;			/* character pointed by ix1 into text */
    register char ch2;			/* character pointed by ix2 into temp */

	int i;

	uint16_t compare16;
	uint32_t compare32;

	/*
	 * does this have a magic character in it? if so we need to to char by char
	 * comparison any way.
	 */
	if (strchr(temp, '*') != NULL) {
		i = 0;
		while (i < len) {					/* do forever, now until done!*/
			ch2 = temp[ix2++];				/* the template charecter */
			ch1 = text[ix1++];				/* the test character */
			switch(ch2) {
				case '*':					/* magic character */
					if (ch1)
						break;				/* break the switch */
					else
						return(1);			/* template > text */

				case '\0':					/* the template ended */
					return(0);				/* matched as far as it went */

				default:					/* any other character */
					if (ch1 == ch2) {		/* good so far */
						if (ch1 == '\0')	/* at the end of both strings */
							return(0);		/* good golly, they matched! */
					} else
						if (ch2 > ch1)
							return(1);
						else
							return(-1);		/* too bad, no good */
			}
			i++;
		}
		return(0);							/* there were no differences */
	}										/* for the length specified */

/*
 * we can do this here because this will never be used to compare anything
 * other than the actual string that makes up the key
 */
	switch (len) {
		case 1:
			i = (*temp - *text);
			break;
		case 2:
			i = (htons(*(uint16_t *)temp) - htons(*(int16_t *)text));
			break;
		case 3:
			i = htons(*(uint16_t *)temp) - htons(*(uint16_t *)text);
			if (!i)
				i = *(temp+sizeof(int16_t))-*(text+sizeof(int16_t));
			break;
		default:
			i = htonl(*(uint32_t *)temp) - htonl(*(uint32_t *)text);
			if (!i && len > 4)
				i = memcmp(temp+sizeof(int32_t), text+sizeof(int32_t), len-sizeof(int32_t));
			break;
	}
	return(i);
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
