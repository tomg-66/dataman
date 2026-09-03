/* ***************************************************************
 *
 * PROCEDURE:	match.c
 *
 * PROJECT:		dataman client side
 * 
 * DATE:		legacy, originally writtin in 1988
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
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

#include "visibility.h"

DATAMAN_API int match(const char text[], const char temp[])

{

    int ix1 = 0;       /* index into string text */
    int ix2 = 0;       /* index into string temp */
    char ch1;          /* character pointed by ix1 into text */
    char ch2;          /* character pointed by ix2 into temp */

    while (1) {                         /* do forever */
        ch2 = temp[ix2++];              /* the template charecter */
        ch1 = text[ix1++];              /* the test character */
        switch(ch2) {
            case '*':                   /* magic character */
                if (ch1)
                    break;              /* break the switch */
                else
                    return(1);          /* template > text */

            case '\0':                  /* the template ended */
                return(0);              /* matched as far as it went */

            default:                    /* any other character */
                if (ch1 == ch2) {       /* good so far */
                    if (ch1 == '\0')    /* at the end of both strings */
                        return(0);      /* good golly, they matched! */

                } else
                    if (ch2 > ch1)
                        return(1);
                    else
                        return(-1);    /* too bad, no good */
        }
    }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
