// ***************************************************************
//
// CLASS:		DatamanIndexFile.java
//
// PROJECT:		dataman client side java library
// 
// DATE:		Sat Dec 11 17:23:11 MST 2004
// 
// AUTHOR:		Tom Green
// 
// FILES:
//
// MODIFICATION HISTORY:
//
//***************************************************************

// This is the description of a datafile that is in use by an
// index.  there is an array of these for each open index.
// each entry in this array will contain the actual file format
// description for this filename.

package Dataman;

class DatamanIndexFile{

	private int _fno;
	private short _longest;
	private int _hlen;
	private DatamanFileDesc _desc;
	private String _fname;

	DatamanIndexFile() {
   		_fname = "";
		_longest = 0;
		_fno = -1;
	}

	protected void set_fno(int i) { _fno = i; }
	protected int get_fno() { return (_fno); }

	protected void set_name(String s) { _fname = s; }
	protected String get_fname() { return (_fname); }

	protected void set_desc(DatamanFileDesc d) { _desc = d; }
	protected DatamanFileDesc get_desc() { return (_desc); }

	protected void set_longest(short i) { _longest = i; }
	protected short get_longest() { return (_longest); }

	protected void set_hlen(int i) { _hlen = i; }
	protected int get_hlen() { return (_hlen); }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
