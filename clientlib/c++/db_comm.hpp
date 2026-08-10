/* ***************************************************************
 *
 * PROCEDURE:	db_comm.hpp
 *
 * PROJECT:		dataman client side c++ header file
 * 
 * DATE:		Wed Jul  7 16:46:59 MDT 2004
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 *
 * MODIFICATION HISTORY:
 * 				Thu Mar 21 16:02:09 MDT 2013
 * 				tom
 * 				added namespace support
 *
 ************************************************************* */
/*
 * define how the communication with the server happens.
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
#if !defined _DATAMAN_COMM_DEFINED_
#define _DATAMAN_COMM_DEFINED_

namespace Dataman {

class db_comm {
	private:
		static int db_sock;					// socket to talk out
		int db_connect(const char *host);	// connect to server
	public:
		db_comm(void);						// construct from nothing
		db_comm(const char *host);			// construct from name
		~db_comm();	
		char *db_send(char *msg, int len);	// send a message
		static int get_sock() { return(db_sock); }
		void db_discon(void);
};

};

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
