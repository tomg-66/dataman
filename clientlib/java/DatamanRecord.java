// ***************************************************************
//
// CLASS:		DatamanRecord
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
// the is the user accessable data record there are two that
// are available to the user.  they are globally available
// as well.  they are the work record, and the master record.
// each contain an array of data fields.  this is what the
// user is really interested in.
//
package Dataman;

import java.io.IOException;
import java.io.PrintStream;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;

/**
 * Definitions for the master and workfile records.  When any
 * data is retrieved it is placed in either of the two data
 * records.  The master record refers to anything retrieved
 * via a reference to an index, and the workfile record is
 * data that is retrieved from the workfile named on the command
 * line.
 */
public class DatamanRecord{

	private short head;
	private short longest;
	private long cur;
	private long prev;
	private long next;
	private int chan;
	private int len;
	private short fmt;
	private boolean _file;
	private boolean _dirty;
	private int which;
	private DatamanFileDesc _filedesc;

	public DatamanField[] field;

	public static final int WORK = 0;
	public static final int MASTER = 1;

	DatamanRecord(int i) {
		head = 0;
		cur = 0;
		chan = 0;
		fmt = 0;
		_file = false;
		_dirty = false;
		_filedesc = null;
		which = i;
	}

	int getwhich() { return (which); } 
	int gethead() { return (head); }
	void sethead(short i) { head = i; }
	int getlongest() { return (longest); }
	void setlongest(short i) { longest = i; }
	long getcur() { return (cur); }
	void setcur(long i) { cur = i; }
	long getnext() { return (next); }
	void setnext(long i) { next = i; }
	long getprev() { return (prev); }
	void setprev(long i) { prev = i; }
	int getchan() { return (chan); }
	void setchan(int i) { chan = i; }
	int getlen() { return (len); }
	void setlen(int i) { len = i; }
	void setfmt(short i) { fmt = i; }
	void setfile(boolean i) { _file = i; }
	boolean getdirty() { return (_dirty); }
	void setdirty(boolean i) { _dirty = i; }
	DatamanFileDesc get_desc() { return (_filedesc); }
	void set_filedesc(DatamanFileDesc d) { _filedesc = d; }
	boolean contains(DatamanField item) {
		int lim = _filedesc.getDesc()[fmt-1].get_nfields();
		for (int i = 1; i <= lim; i++)
			if (field[i].equals(item))
				return(true);
		return(false);
	}
/**
 * Return the format number of the current record.
 * @return short
 */
	public short getfmt() { return (fmt); }
/**
 * Return if the file in a sort routine has been newly opened.
 * This is useful in sort routines to change states depending on
 * which file is being processed.
 * @return true if the file is newly opened, false if a release has been performed.
 */
	public boolean getfile() { return (_file); }

//
// take a record retrieved from the server, parse it into
// the fields as the description dictates.
//
	void in_rec(ByteBuffer rec) {

		DatamanComms comm = new DatamanComms();

		DatamanFileDesc fdesc;
		DatamanRecDesc rfdesc;

//		DatamanField [] field;

		int i, j;
//
// this first part is to retrieve and parse the data file
// in case it has never been retrieved before
//
		if ((fdesc = _filedesc) == null) {
			fdesc = new DatamanFileDesc();

			String cmd;
			ByteBuffer buff;
			DatamanErrVal datamanerrval;
//
// this is the command to get the datafile description.
//
			if (getwhich() == DatamanRecord.MASTER) 
				cmd = DatamanFunc.GET_DESC + "|" + Dataman.cur_index.get_idxno() +
						"|" + getchan() + "|";
			else
				cmd = DatamanFunc.GET_DESC + "|" + -getchan() + "|";
//
// db_send send the command to the server, and returns the response
//
			try {
				Charset cs = Charset.forName("UTF-8");
				buff = comm.db_send(cs.encode(cmd), cmd.length());
				buff.rewind();
				for (i = 0;i < 2; i++) {
					while (buff.get() != '|')
						;
				}
				ByteBuffer data = buff.slice();
				buff.rewind();
// the return code is in the first 'field' of the return.  if it
// is < 0, that is an error
				cmd = new String(cs.decode(buff).array());
				String [] result = cmd.split("[|]" , 3);
				i = Integer.parseInt(result[0]);
				if (i < 0) {
					DatamanErrVal e = new DatamanErrVal();
					throw new DatamanRuntimeException(e.getDBErr(i));
				}
// if this is the master file we need to set some extra stuff.
				if (getwhich() == DatamanRecord.MASTER) {
					DatamanIndexFile ixfile = Dataman.cur_index.get_files()[Dataman.cur_index.get_fno()];
					ixfile.set_hlen(i);
					head = (short)i;
					ixfile.set_desc(fdesc);
				}
				fdesc.DatamanSetupDesc((short)i , data);
				_filedesc = fdesc;
			} catch (IOException e) {
				throw transportError("GET_DESC", e);
			}
		}
//
// now that we know we have the correct and parse header, break the
// record up to is individual fields.  the first loop gets the fixed
// length fields, the second gets any blobs that are there.
//
		rfdesc = fdesc.getDesc()[getfmt() - 1];
		field = new DatamanField[rfdesc.get_nfields() + 2];
		int offset = rfdesc.get_rflen();
		for (i = 0, j = 0;i < rfdesc.get_nfields(); i++) {
			if (rfdesc.get_size(i) != 0) {
				field[i + 1] = new DatamanField(rec, j, rfdesc.get_size(i), false);
				j += rfdesc.get_size(i);
			} else {
				rec.position(offset);
				int size = rec.getInt();
				rec.rewind();
				offset += 4;
				field[i+1] = new DatamanField(rec, offset, size, true);
				offset += size;
			}
		}
//		this.field = field;
		setdirty(false);
	}

//
// take the current record , put it together so the server
// knows what to do, and if needed send it off to the server.
	void out_rec() {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i, j;
		int length;

		if (field == null) 
			return;
         
		if (!getdirty()) 
			return;
//
// this first part gets the lenght of the record to output including
// the size of any blobs.
//
		DatamanRecDesc rfdesc = get_desc().getDesc()[getfmt() - 1];
		length = rfdesc.get_rflen();
		for (i = 1, j= 0; j < rfdesc.hasBlob(); i++) {
			if (field[i].getType() == DatamanField.type_blob) {
				length += field[i].getLen() + 4;
				j++;
			}
		}
//
// put together the packet header for the flush command.  work records
// are slightly different than master records.
//
		if (getwhich() == DatamanRecord.MASTER) 
			cmd = DatamanFunc.FLUSH + "|" + Dataman.cur_index.get_idxno() + "|" +
					getchan();
		else
			cmd = DatamanFunc.FLUSH  + "|" + -getchan() + "|0";
		cmd += "|" + getcur() + "|" + getfmt() + "|" + length + "|";
//
// now allocate the entire buffer to transmit to the server
// then copy in the header
//
		buff = ByteBuffer.allocateDirect(length + cmd.length());
		length += cmd.length();
		Charset cs = Charset.forName("UTF-8");
		buff.put(cs.encode(cmd));
//
// now put the data in the buffer.  first all of the non-blob
// fields.  the second for loop is to put in the blob data.
//
		for (j = 1;j <= rfdesc.get_nfields(); j++) {
			if (field[j].getType() != DatamanField.type_blob)
				buff.put(field[j].getData());
		}
		for (i = 1, j = 0; j < rfdesc.hasBlob(); i++) {
			if (field[i].getType() == DatamanField.type_blob) {
				buff.putInt(field[i].getLen());
				buff.put(field[i].getData());
				j++;
			}
		}
		buff.rewind();
//
//send the data, and get the return value
//
		try {
			buff = comm.db_send(buff, length);
			cmd = new String(cs.decode(buff).array());
			String [] result = cmd.split("[|]");
			i = Integer.parseInt(result[0]);
			if (i < 0) {
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
		} catch (IOException e) {
			throw transportError("FLUSH", e);
		}
		setdirty(false);
	}
/**
 * Release the current record and retrieve the next from the datafile.  This
 * is used in sort routines to retrieve records to be sorted.  If at the end
 * of the current work file, the file is closed, the next is opened, the first
 * record of that file is returned, and the new file flag is set to true.  If
 * the current record is the last in the last named file, false is returned.
 * @return true if there was another record, else false
 * @throws DatamanRuntimeException
 */
	public boolean release() {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		if (getwhich() != DatamanRecord.WORK) {
			DatamanErrVal datamanerrval = new DatamanErrVal();
			throw new DatamanRuntimeException(datamanerrval.getDBErr(DatamanErrVal.ENOTWORK));
		}
		_file = false;
		out_rec();
		if (next == 0) {
			if (++Dataman._fileno == Dataman._maxfil) {
				Dataman._fileno--;
				return (false);
			}
			_filedesc.free_rec();
			_filedesc = null;
			cmd = DatamanFunc.RELEASE + "|" + chan + "|0|" + Dataman._root +
					"/files/" + Dataman._fnames[Dataman._fileno] + "|";
			Dataman.cur_index.get_file(0).set_name(null);
			_file = true;
		} else
			cmd = DatamanFunc.RELEASE + "|" + chan + "|" + next + "|";
         
		try {
			Charset cs = StandardCharsets.UTF_8;
			buff = comm.db_send(cs.encode(cmd) , cmd.length());
			String[] result = readFields(buff, 1, "RELEASE");
			len = Integer.parseInt(result[0]);
			if (len < 0) {
				System.out.println(Dataman._progname + ":  error in release");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(len));
			}
			result = readFields(buff, 5, "RELEASE");
			longest = Short.parseShort(result[0]);
			fmt = Short.parseShort(result[1]);
			cur = Long.parseLong(result[2]);
			prev = Long.parseLong(result[3]);
			next = Long.parseLong(result[4]);
			in_rec(buff.slice());
			if (Dataman.cur_index.get_file(0).get_fname() == null) {
				Dataman.cur_index.get_file(0).set_name(Dataman._fnames[Dataman._fileno]);
				Dataman.cur_index.get_file(0).set_desc(_filedesc);
				Dataman.cur_index.get_file(0).set_longest(_filedesc.get_longest());
			}
		} catch (IOException e) {
			throw transportError("RELEASE", e);
		} catch (NumberFormatException e) {
			throw new DatamanRuntimeException("Malformed RELEASE response", e);
		}
		return (true);
	}

	private DatamanRuntimeException transportError(String operation,
		IOException cause) {
		return new DatamanRuntimeException(
			Dataman._progname + ": transport error during " + operation, cause);
	}

	private String[] readFields(ByteBuffer buffer, int count,
		String operation) {
		String[] values = new String[count];
		for (int field = 0; field < count; field++) {
			int start = buffer.position();
			while (buffer.hasRemaining() && buffer.get() != (byte)'|')
				;
			if (buffer.position() == start ||
				buffer.get(buffer.position() - 1) != (byte)'|')
				throw new DatamanRuntimeException(
					"Malformed " + operation + " response");
			int end = buffer.position() - 1;
			byte[] text = new byte[end - start];
			ByteBuffer copy = buffer.duplicate();
			copy.position(start);
			copy.get(text);
			values[field] = new String(text, StandardCharsets.US_ASCII);
		}
		return values;
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
