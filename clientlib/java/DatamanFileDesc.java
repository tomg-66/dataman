// ***************************************************************
//
// CLASS:		DatamanFileDesc.java
//
// PROJECT:		dataman client side java library
// 
// DATE:		Thu Dec 9 18:13:21 MST 2004
// 
// AUTHOR:		Tom Green
// 
// FILES:
//
// MODIFICATION HISTORY:
//
//***************************************************************

//
// this is where we keep the description of a file.  there is
// one for each file.  in each file there is an array to
// contain the description of each record format.
//

package Dataman;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

class DatamanFileDesc{
	private short header_len;
	private short n_rformats;
	private short longest;
	private DatamanRecDesc[] record_desc;

	DatamanFileDesc() {
		header_len = 0;
		n_rformats = 0;
		longest = 0;
		return;
   	}
//
// set up the file description.  it is an array of shorts
// that is passed from the server.  the server sends the
// data in network byte order (BIG_ENDIAN).  newly created
// byte buffers are, by definition, BIG_ENDIAN, we might
// need to change the byteorder of the bb.
	protected void DatamanSetupDesc(short len, ByteBuffer buff) {
		if (ByteOrder.nativeOrder() == ByteOrder.LITTLE_ENDIAN) 
			buff.order(ByteOrder.LITTLE_ENDIAN);
         
		header_len = len;
		buff.rewind();
		n_rformats = buff.getShort();
		record_desc = new DatamanRecDesc[n_rformats];
		for (int j = 0;j < n_rformats;j++) {
			short count = buff.getShort();
			record_desc[j] = new DatamanRecDesc();
			record_desc[j].set_nfields(count);
			record_desc[j].set_field_sizes(new int[count]);
			record_desc[j].set_rflen(buff.getShort());
			for (int i = 0;i < record_desc[j].get_nfields();i++) {
				record_desc[j].set_size(i, (int)buff.getShort());
				if (record_desc[j].get_size(i) == 0)
					record_desc[j].inc_blob();
			}
		}
		return;
	}
//
// 'free' up the file description.  as this would normally only
// be used by DatamanRecord.release(), it's not a big deal.  this
// is only used to force these to be garbage collected.
//
	protected void free_rec() {
		for (int i = 0;i < n_rformats;i++) {
			record_desc[i].set_field_sizes(null);
			record_desc[i] = null;
		}
		record_desc = null;
		return;
	}

	protected DatamanRecDesc[] getDesc() { return (record_desc); }
	protected short get_header_len() { return (header_len); }
	protected void set_header_len(short i) { header_len = i; }
	protected short get_longest() { return (longest); }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
