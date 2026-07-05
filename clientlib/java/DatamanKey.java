// ***************************************************************
//
// CLASS:		DatamanKey.java
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
// define how a key will look like to the system and the user.
//
package Dataman;

import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/**
 * The index key datatype.  This class is used to retrieve or
 * remove a key from an index.  An index is allowed to have duplicate
 * keys. If searching through many duplicates, you save the system
 * key for later use, it will refer to that particular instance.
 */
public class DatamanKey{

	private byte[] data;
	private int _len;
	private short _fno;
	private long _rec;
	private String key_str;

	static final int MIN_KEY_SIZE = 1;
	static final int MAX_KEY_SIZE = 32;
	static final int HEADER_LEN = 9;
/**
 * Create an empty key.
 */
	public DatamanKey() {
		data = new byte[64];
	}
/**
 * Create a key from an arbitrary String
 * @param string - the string to initialize the key to.
 */
	public DatamanKey(String string) {
		data = new byte[64];
		key_str = string;
		_len = string.length();
		_fno = -1;
		_rec = 0;
	}
/**
 * Copy constructor from another key
 * @param k - the key to copy.
 */
	public DatamanKey(DatamanKey k) {
		data = k.data;
		_len = k._len;
		_fno = k._fno;
		_rec = k._rec;
		key_str = k.key_str;
	}
//
// this is only internal to the routines... don't
// let the user use this!
//
	DatamanKey(ByteBuffer buff, int len) {
		data = new byte[64];
		buff.rewind();
		buff.get(data, 0, len + HEADER_LEN);
		buff.rewind();
		byte[] tmp = new byte[len];
		buff.get(tmp, 0, len);
		_fno = (short)buff.get();
		_rec = buff.getLong();
		_len = len;
		key_str = new String(tmp);
	}
//
// this constructor is only used in include if we are in a
// transaction   finish this!
//
	DatamanKey(String k, int f, long r, int l) {
		data = new byte[64];
		ByteBuffer b = ByteBuffer.wrap(data);
		b.put(k.getBytes());
		b.put(l, (byte)f);
		b.position(l+1);
		b.putLong(r);
		_fno = (short)f;
		_rec = r;
		_len = l;
		key_str = k;
	}

/**
 * Return the String representation of the current key.
 * @return String
 */
	public String keyStr() { return (key_str); }
/**
 * Simple Key pattern matching.  The argument is the template to
 * match against.  Any single character may be wildcarded with an
 * '*' character.  The match is true if the key matches the template
 * up to the length of the template.  If the keyStr is longer than
 * the template it is not a match.
 * @param tem - the template to match against.
 * @return true if the key matched the template, else false
 */
	public boolean match(String tem) {
		int j = 0;
		char ch1, ch2;
		while (j < tem.length()) {				/* do forever, now until done!*/
			if (j >= key_str.length())
				return(false);
			ch2 = tem.charAt(j);					/* the template charecter */
			ch1 = key_str.charAt(j++);			/* the test character */
			if (ch2 == '*')
				continue;
			if (ch1 == ch2)
				continue;
			else
				return(false);
		}
		return(true);
	}
			
	int get_len() { return (_len); }
	short get_fno() { return (_fno); }
	long get_rec() { return (_rec); }
	byte[] get_data() { return (data); }
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
