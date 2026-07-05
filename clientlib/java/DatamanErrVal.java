// ***************************************************************
//
// CLASS:		DatamanErrVal.java
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
//				Mon Aug 28 21:27:55 MDT 2006
//				justified some error messages and numbers with
//				the 'C' library and added new xact errors
//				tomg
//
//***************************************************************

//
// this class defines the error strings and values that
// the server uses, and the couple that the client generates.
//
package Dataman;

class DatamanErrVal{
	private static final String[] dbErrStrings = {
		"",
		"can't start new server thread",
		"server couldn't attach to message queue",
		"server couldn't create or attach shared mem",
		"server couldn't create the semaphore",
		"index file not found",
		"error during initialization of index",
		"no space to allocate more index",
		"can't obtain a necessary file lock",
		"can't read node from index",
		"file not found in index",
		"misc can not allocate error",
		"error reading file header",
		"error reading record header",
		"error reading the rec from the file",
		"system key has been removed",
		"system record has been deleted",
		"requested file isn't open",
		"invalid format # for file",
		"can't write new record to file",
		"can't write new begining offset",
		"you can't delete the only rec in the file",
		"can't write header (record, file, index)",
		"can't write node to index",
		"can't find parent offset",
		"error reading during protect",
		"trying to protect a deleted record",
		"error reading during clear",
		"index not open",
		"no work file space",
		"could not open work file",
		"index is already open",
		"can't create new index",
		"can't write file name to index",
		"invalid host name",
		"can't open socket",
		"can't connect to server",
		"received no response from server",
		"initial GET not attempted",
		"invalid message received on socket",
		"can't set socket option",
		"not a work file",
		"invalid data field subscript",
		"dataman is shutting down",
		"Can't get BLOB file",
		"Operation not permitted on Blob type",
		"Already in a transaction",
		"Not in a transaction",
		"Rolling back a failed transaction failed",
		"Java socket connection is not made",
		"Java error reading socket",
		"Multiple errors, your database may be corrupt",
		"Can't write blob to file",
	};
	public static final int ENOTHR = -1;
	public static final int ENOMSGQ = -2;
	public static final int ENOSHM = -3;
	public static final int ENOSEM = -4;
	public static final int ENOINDEX = -5;
	public static final int EINITINDEX = -6;
	public static final int ENOIXSP = -7;
	public static final int ENOLOCK = -8;
	public static final int ENODERD = -9;
	public static final int ENOFILE = -10;
	public static final int ENOALLOC = -11;
	public static final int EFHDRD = -12;
	public static final int ERHREAD = -13;
	public static final int ERECREAD = -14;
	public static final int ERMKEY = -15;
	public static final int ENOREC = -16;
	public static final int ENOTOPEN = -17;
	public static final int EBADFMT = -18;
	public static final int ERECWRT = -19;
	public static final int EBEGWRT = -20;
	public static final int ENODEL = -21;
	public static final int EHDRWRT = -22;
	public static final int ENODWRT = -23;
	public static final int ENOPARENT = -24;
	public static final int EPRTRD = -25;
	public static final int EPRCTDEL = -26;
	public static final int ECLRRD = -27;
	public static final int EIDXNOO = -28;
	public static final int ENOWSP = -29;
	public static final int ENOWFILE = -30;
	public static final int EIDXOPN = -31;
	public static final int EIDXCREAT = -32;
	public static final int EFILWRT = -33;
	public static final int ENOHOST = -34;
	public static final int ENOSOCK = -35;
	public static final int ENOCONN = -36;
	public static final int ENORESP = -37;
	public static final int ENOGET = -38;
	public static final int EINVMSG = -39;
	public static final int ESOCKOPT = -40;
	public static final int ENOTWORK = -41;
	public static final int ESUBSCR = -42;
	public static final int ESHUT = -43;
	public static final int ENOBLOB = -44;
	public static final int EBLOBTYP = -45;
	public static final int EINXACT = -46;
	public static final int ENOXACT = -47;
	public static final int EROLLBACK = -48;
	public static final int EJAVACON = -49;
	public static final int EJAVAREAD = -50;
	public static final int EMULTIPLE = -51;
	public static final int EBLOBWRT = -52;


	DatamanErrVal() {}
//
// return the string used for DatamanRuntimeException()
//
	protected String getDBErr(int i) {
		return (DatamanErrVal.dbErrStrings[-i]);
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
