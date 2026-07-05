// ***************************************************************
//
// CLASS:		DatamanRecDesc
//
// PROJECT:		dataman client side java library
// 
// DATE:		Sat Dec 11 10:11:07 MST 2004
// 
// AUTHOR:		Tom Green
// 
// FILES:
//
// MODIFICATION HISTORY:
//
//***************************************************************

// the record description of the record.  the file description
// contains an array of these, one for each record format
// contained in the file.

package Dataman;

class DatamanRecDesc {
	private short n_fields;
	private short rf_len;
	private int[] field_sizes;
	private int has_blob;

	protected DatamanRecDesc() {
		n_fields = 0;
		rf_len = 0;
		field_sizes = null;
		has_blob = 0;
	}

	protected short get_nfields() { return (n_fields); }
	protected void set_nfields(short i) { n_fields = i; }

	protected short get_rflen() { return (rf_len); }
	protected void set_rflen(short i) { rf_len = i; }

	protected void set_size(int idx, int val) { field_sizes[idx] = val; }
	protected int get_size(int i) { return (field_sizes[i]); }
	protected int[] get_field_sizes() { return (field_sizes); }
	protected void set_field_sizes(int[] iArr) { field_sizes = iArr; } 

	protected int hasBlob() { return (has_blob); }
	protected void inc_blob() { has_blob++; };
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
