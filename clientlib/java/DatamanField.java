// ***************************************************************
//
// CLASS:		DatamanField.java
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
// implement the actual field that the client routines will
// read, manipulate, and store back in the database
//

package Dataman;

import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;

/**
 * References to, and operations for an individual data field.  Datafields
 * are referenced by either the master data record or the workfile data
 * record.  They each have an array of data fields.  When referencing a
 * datafield, DO NOT use array offsets, use field numbers.  Field numbers
 * range from 1 through n, NOT 0 througn n-1.
 */
public class DatamanField {

	private int len;
	private ByteBuffer data;
	private int type;

	int getLen() { return (len); }
	ByteBuffer getData() { return(data); }
	int getType() { return(type); }

	static final int type_non = -1;
	static final int type_chr = 0;
	static final int type_int = 1;
	static final int type_flt = 2;
	static final int type_blob = 4;
	static final int type_unk = 6;
//
// various constructors.  I don't know if we'll need them
//
/**
 * Construct an empty data field.
 * @deprecated.
 */
	DatamanField() {
		len = 0;
		data = null;
		type = type_non;
		return;
	}

/**
 * Copy constructor for a field.
 */
	DatamanField(DatamanField f) {
		data = ByteBuffer.allocate(f.getData().capacity());
		data.put(f.getData());
		data.rewind();
		len = data.capacity();
		type = f.type;
		return;
	}

/**
 * Construct a  data field from a String.
 * @deprecated.
 * @param s The string to store
 */
	DatamanField(String s) {
		data = ByteBuffer.allocate(s.length());
		data.put(s.getBytes());
		data.rewind();
		len = s.length();
		type = type_chr;
		return;
	}

/**
 * Construct a  data field from an integer
 * @deprecated.
 * @param int i The integer to store
 */
	DatamanField(int i) {
		String tmp = "" + i;
		data = ByteBuffer.allocate(tmp.length());
		data.put(tmp.getBytes());
		data.rewind();
		len = tmp.length();
		type = type_int;
		return;
	}

/**
 * Construct a  data field from a long integer.
 * @deprecated.
 * @param long i The integer to store
 */
	DatamanField(long i) {
		String tmp = "" + i;
		data = ByteBuffer.allocate(tmp.length());
		data.put(tmp.getBytes());
		data.rewind();
		len = tmp.length();
		type = type_int;
		return;
	}

/**
 * Construct a  data field from a short integer.
 * @deprecated.
 * @param short i The integer to store
 */
	DatamanField(short i) {
		String tmp = "" + i;
		data = ByteBuffer.allocate(tmp.length());
		data.put(tmp.getBytes());
		data.rewind();
		len = tmp.length();
		type = type_int;
		return;
	}

/**
 * Construct a  data field from a float.
 * @deprecated.
 * @param float f The float to store
 */
	DatamanField(float f) {
		String tmp = "" + f;
		data = ByteBuffer.allocate(tmp.length());
		data.put(tmp.getBytes());
		data.rewind();
		len = tmp.length();
		type = type_flt;
		return;
	}
//
// this constructor is used only for internal building of 
// data records.  it is called from in_rec.  if a bytebuffer
// is made with allocateDirect() then it doesn't necessarily
// have a backing array.  those created with allocate() -do-
// have a backing array.  do which is easier for each.
//
	DatamanField(ByteBuffer b, int i, int j, boolean is_blob) {
		if (i < 0) {
			i = 0;
		}
		if (i+j > b.capacity()) {
			j = b.capacity() - i;
		}
		b.position(i);
		b.mark();
		int save = b.limit();
		b.limit(i+j);
		data = ByteBuffer.allocate(j);
		data.put(b);
		data.rewind();
		b.reset();
		b.limit(save);
		len = j;
		if (is_blob)
			type = type_blob;
		else
			type = type_unk;
		return;
	}

//
// various ways to get data into a data field.  strings,
// longs, ints, shorts, floats.  datafields with this
// types are fixed by the length they were constructed
// with, so that when rebuilding the packet to go back,
// it is allways the correct size.
//
// blobs, on the other hand, are (by definition) undefined,
// so we have to have different ways to put them.  and we
// have a different way to construct them.  we start by
// using the 'null' constructor, then calling putBlob, we
// don't use the above constructor, so 
//
/**
 * Store a String in a data field.  This stores the given String
 * in the field, based on the field length, and marks the
 * appropriate data record (master or workfile) as needing to be
 * flushed.
 * @param s The string to store.
 */
	public void putString(String s) {
		if (type == type_blob) {
// this is an error
		}
		byte[] encoded = s.getBytes(StandardCharsets.ISO_8859_1);
		if (data == null) {
			data = ByteBuffer.allocate(encoded.length);
			data.put(encoded);
			len = encoded.length;
		} else if (encoded.length >= len) {
			data.clear();
			data.put(encoded, 0, len);
		} else {
			data.clear();
			data.put(encoded, 0, encoded.length);
			while(data.remaining() != 0)
				data.put((byte)' ');
		}
		type = type_chr;
		data.rewind();
		if (Dataman.master.contains(this))
			Dataman.master.setdirty(true);
		else
			Dataman.workfile.setdirty(true);
		return;
	}


/**
 * Store a long integer in a data field.  This stores the given long integer
 * in the field, based on the field length, and marks the
 * appropriate data record (master or workfile) as needing to be
 * flushed.
 * @param i The integer to store
 */
	public void putLong(long i)
	{
		 String s = "" + i;
		 putString(s);
		 type = type_int;
		 return;
	}


/**
 * Store a short integer in a data field.  This stores the given short integer
 * in the field, based on the field length, and marks the
 * appropriate data record (master or workfile) as needing to be
 * flushed.
 * @param i The integer to store
 */
	public void putShort(short i) {
		String s = "" + i;
		putString(s);
		type = type_int;
		 return;
	}


/**
 * Store a integer in a data field.  This stores the given integer
 * in the field, based on the field length, and marks the
 * appropriate data record (master or workfile) as needing to be
 * flushed.
 * @param i The integer to store
 */
	public void putInt(int i) {
		String s = "" + i;
		putString(s);
		type = type_int;
		 return;
	}


/**
 * Store a float in a data field.  This stores the given float
 * in the field, based on the field length, and marks the
 * appropriate data record (master or workfile) as needing to be
 * flushed.
 * @param f The float to store
 */
	public void putFloat(float f) {
		String s = "" + f;
		putString(s);
		type = type_flt;
		return;
	}
//
// using a ByteBuffer here is like using other ByteBuffer routines in
// that at the end of this routine, b.offset() is equal to
// offset+length
//
/**
 * Store a Blob in a data field.  This stores the data from the
 * ByteBuffer b beggining at offset and stores length (up to
 * b.capacity()) bytes.
 * The dataField must have been originally defined as a Blob
 * in mkdf.  If the dataField is not a Blob, the datafield is
 * not altered.
 * On return the ByteBuffers offset (b.offset()) is
 * set to offset+length.  If 
 * @param b The ByteBuffer containing the blob
 * @param offset Begining offset of the blob
 * @param length The length of the blob
 */
	public void putBlob(ByteBuffer b, int offset, int length) {
		if (this.type != type_blob && type != type_non) {
			// do an error here
		}
		if (offset < 0 || offset > b.capacity()) {
			// this is another error
		}
		b.position(offset);
		if (b.remaining() < length)
			length = b.remaining();

		data = ByteBuffer.allocate(length);

		if (b.hasArray()) {
			data.put(b.array(), offset, length);
			b.position(offset+length);
		} else {
			for (int i = 0; i < length; i++)
				data.put(b.get());
		}
		data.rewind();
		len = length;
		type = type_blob;

		if (Dataman.master.contains(this))
			Dataman.master.setdirty(true);
		else
			Dataman.workfile.setdirty(true);
		return;
	}
//
// here this puts the blob from the source 'b' (at it's current position)
// to the end of the byte buffer.  at the end, the buffer's position is
// b.capacity()
//
/**
 * Store a Blob in a data field.  This stores the data from the
 * Bytebuffer b starting at the current position, through the
 * remaining number of bytes.  The dataField must have been defined
 * as a Blob in mkdf.  If the dataField is not, nothing will be
 * changed.
 * @param b The blob to store
 */

	public void putBlob(ByteBuffer b) {
		putBlob(b, b.position(), b.remaining());
		return;
	}
//
// put a blob from the buffer b.  it begins at the buffers current postion
// and puts length bytes.  if length > b.remaining() length gets set to
// b.remaining().
//
/**
 * Store a Blob in a data field.  This stores the data from the
 * Bytebuffer b starting at the current position, through a maximum
 * of length bytes.  The dataField must have been defined
 * as a Blob in mkdf.  If the dataField is not, nothing will be
 * changed.
 * @param b The blob to store.
 * @param length The length of the blob.
 */
	public void putBlob(ByteBuffer b, int length) {
		putBlob(b, b.position(), length);
		return;
	}

//
// get data from out of a datafield.  you might just want
// to use the data!
//
/**
 * Return an integer value from the datafield.  If the 
 * data field does not contain an integer, 0 is returned.
 */
	public int getInt() {
		try {
			return (Integer.parseInt(getString().trim()));
		} catch (NumberFormatException e) {
			return (0);
		}
	}


/**
 * Return a long integer value from the datafield.  if
 * the data field does not contain an integer, 0 is returned.
 */
	public long getLong() {
		try {
			return (Long.parseLong(getString().trim()));
		}
		catch (NumberFormatException e) {
			return (0);
		}
	}


/**
 * Return a short integer value from the datafield.  if
 * the data field does not contain an integer, 0 is returned.
 */
	public short getShort() {
		try {
			return (Short.parseShort(getString().trim()));
		}
		catch (NumberFormatException e) {
			return (0);
		}
	}


/**
 * Return a float value from the datafield.  If the data field
 * does not contain a float, 0.0 is returned.
 */
	public float getFloat() {
		try {
			 return (Float.parseFloat(getString().trim()));
		}
		catch (NumberFormatException e) {
			return ((float)0.0);
		}
	}

//
// these are most likely to be used as well.
//
/**
 * Return a String representation of the data in the
 * field.
 * @return String
 */
	public String getString() {
		 return (new String(data.array(), StandardCharsets.ISO_8859_1));
	}
/**
 * Return a substring of the data in the field.  A string
 * begining at offset off, through the end of the datafield
 * is returned
 * @param off - where to begin the substring
 * @return String
 */
	public String substring(int off) {
		return (getString().substring(off));
	}

/**
 * Return a substring of the data in the field.  A string
 * begining at offset off for a maximun of length characters.
 * 
 * @param off  Offset to begin the substring
 * @param length  Length of the substring
 * @return String
 */
	public String substring(int off, int length) {
		return (getString().substring(off,
			Math.min(getString().length(), off + length)));
	}
/*
 * and finally, the method to get a blob.
 */
/**
 * Return a blob.  This returns a ByteBuffer containing the data
 * actually contained in the field with no implications as to any
 * type.
 * @return ByteBuffer
 */
	public ByteBuffer getBlob() {
		return(data);
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
