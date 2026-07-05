/* ***************************************************************
 *
 * PROCEDURE:	mask.c
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

/*
 * this routine masks the recieved integer (the first argument) into
 * the mask of the second arguemt, and returns a pointer to the masked
 * string.  Its calling sequence is:
 *      ptr = mask(num,msk);
 * again the pointer returned is the address of msk.  a zero in the mask
 * means to suppress all zeros leading and including the one in the mask.
 * commas are left in the mask to enhance readability.  periods are always
 * left as a decimal point.
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

#include <stdint.h>
#include <string.h>

char *mask(int64_t num, char *msk)
{
    register char zero_found = 0;               /* zero in the mask? */

    register int tmp;                           /* temporary integer */
    register int offs;                          /* offset into mask */

    offs = strlen(msk) -1;                      /* get offset */

    if (*(msk+offs) == '-') {                   /* negative sign in mask? */
        if (num >= 0)
            *(msk+offs) = ' ';                  /* non-neg so remove */
        offs--;                                 /* decrement offset */
    }
    if (num < 0)
	num *= -1;				/* convert to pos num */

    while (1) {
        tmp = num % 10;                         /* this number to insert */
        num /= 10;                              /* divide by 10 */
        if (*(msk+offs) == '.')
            offs--;                             /* skip the period */
        if (*(msk+offs) == ',') {
            if(num)
                offs--;                         /* skip the comma */
            else if (tmp == 0 && !zero_found)
                offs--;
	    else if (tmp != 0)
		offs--;
        }

        if (*(msk+offs) == '0')
            zero_found = 1;                     /* zero is in the mask */
        if (tmp == 0) {
            if (zero_found && num == 0)
                *(msk+offs) = ' ';              /* blank the position */
            else
                *(msk+offs) = '0';              /* put a zero in */
        } else
            *(msk+offs) = tmp + 060;            /* put the digit in */
        offs--;                                 /* decrement mask offset */
        if (offs < 0)
            return(msk);                        /* all done */
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
