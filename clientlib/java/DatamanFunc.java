// ***************************************************************
//
// CLASS:		DatamanFunc.java
//
// PROJECT:		dataman client side java library
// 
// DATE:		summer, 2004
// 
// AUTHOR:		Tom Green
// 
// FILES:
//
// MODIFICATION HISTORY:
//
//***************************************************************

//
// definition of the command values that the server understands.
// and for debugging purposes the strings that go with them.
//

package Dataman;

class DatamanFunc{
	private String [] funcStrings = {
		"ROLLBACK",
		"COMMIT",
		"START_XACT",
		"GET",
		"GET_FIRST",
		"GET_LAST",
		"GET_NEXT",
		"GET_PRIOR",
		"GET_CURRENT",
		"FORWARD",
		"BACK",
		"PROTECT",
		"GET_DESC",
		"INIT_DAT",
		"RELEASE",
		"MKIDX",
		"RESTORE",
		"GET_REC",
		"UNDELETE",
		"DELETE",
		"INSERT",
		"INCLUDE",
		"REMOVE",
		"CLEAR",
		"IOPEN",
		"ICLOSE",
		"SORT",
		"FLUSH",
		"DISCON",
	};
	protected static final int ROLLBACK = -3;
	protected static final int COMMIT = -2;
	protected static final int START_XACT = -1;
	protected static final int GET = 0;
	protected static final int GET_FIRST = 1;
	protected static final int GET_LAST = 2;
	protected static final int GET_NEXT = 3;
	protected static final int GET_PRIOR = 4;
	protected static final int GET_CURRENT = 5;
	protected static final int FORWARD = 6;
	protected static final int BACK = 7;
	protected static final int PROTECT = 8;
	protected static final int GET_DESC = 9;
	protected static final int INIT_DAT = 10;
	protected static final int RELEASE = 11;
	protected static final int MKIDX = 12;
	protected static final int RESTORE = 13;
	protected static final int GET_REC = 14;
	protected static final int UNDELETE = 15;
	protected static final int DELETE = 16;
	protected static final int INSERT = 17;
	protected static final int INCLUDE = 18;
	protected static final int REMOVE = 19;
	protected static final int CLEAR = 20;
	protected static final int IOPEN = 21;
	protected static final int ICLOSE = 22;
	protected static final int SORT = 23;
	protected static final int FLUSH = 24;
	protected static final int DISCON = 25;

    protected String toString(int value) {
         return (funcStrings[value+3]);
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
