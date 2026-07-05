// ***************************************************************
//
// CLASS:		DatamanIndexSave.java
//
// PROJECT:		dataman client side java library
// 
// DATE:		Sat Dec 11 10:54:54 MST 2004
// 
// AUTHOR:		Tom Green
// 
// FILES:
//
// MODIFICATION HISTORY:
//
//***************************************************************

// this class lets the user implement the save instruction
// for an index.

package Dataman;

class DatamanIndexSave{

	protected long _savnode;
	protected long _savrec;
	protected int _savfile;
	protected DatamanKey _savkey;
	protected int _savfmt;
	protected int _savoffs;

	protected DatamanIndexSave(long i ,long j ,int k ,DatamanKey key ,int l ,int m)
    {
   		_savnode = i;
		_savrec = j;
		_savfile = k;
		_savkey = key;
		_savfmt = l;
		_savoffs = m;
	}


	protected long getnode() { return (_savnode); }
	protected long getrec() { return (_savrec); }
	protected int getfile() { return (_savfile); }
	protected DatamanKey getkey() { return (_savkey); }
	protected int getfmt() { return (_savfmt); }
	protected int getoffs() { return (_savoffs); }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
