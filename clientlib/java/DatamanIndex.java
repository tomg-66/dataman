// ***************************************************************
//
// CLASS:		DatamanIndex.java
//
// PROJECT:		dataman client side java library
// 
// DATE:		Thu Dec 9 17:27:37 MST 2004
// 
// AUTHOR:		Tom Green
// 
// FILES:
//
// MODIFICATION HISTORY:
//
//***************************************************************

// this is the big mutha of this client classlib.  it implements
// most of the verbs of the editor language/dataman database
// server.

package Dataman;

import java.io.IOException;
import java.io.PrintStream;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;

/**
 * This implements most of verbs of the Dataman Package.
 * All master data file records are associated with an index.
 * When a key is retrieved, it's associated data record is
 * also retrieved and stored in the master data record array.
 * Almost everything in Dataaman is done by referencing an index.
 */
public class DatamanIndex{

	private static String[] _onames = new String [6];
	private String _idxname;
	private int _idxno;
	private int _wrmode;
	private int _fno;
	private int _nfiles;
	private int _keylen;
	private int _longest;
	private long _curnode;
	private long _generation;
	private long _rptr;
	private short _offs;
	private DatamanKey _curkey;
	private DatamanIndexFile[] _files;
	private DatamanIndexSave _savptr;
//
// some useful constants for our users.  we want to tell
// how to open an index.  read-only, or read-write?
//
/**
 * Opens an index and related datafiles in read-only mode.
 */
	public static final int RDONLY = 0;
/**
 * Opens and index an related datafiels in update mode.
 */
	public static final int UPDATE = 1;
//
// when inserting a new record... before or after the
// current one?
//
/**
 * Insert the new record before the current one in memory.
 */
	public static final int BEFORE = 0;
/**
 * Insert the new record after the current one in memory.
 */
	public static final int AFTER = 1;
//
// these are used for internal purposes for offsets...
//
	private static final int k_header_len = 9;
//
// this internal function is used by the get* routines.
// it is what is used to parse the return from the server.
//
    private int parse_get(ByteBuffer buff) {

		ByteBuffer tmp;
		byte [] cmd;
		int size;
		int i;

         Charset cs = Charset.forName("UTF-8");
		
		buff.rewind();
//
// it became obvious when blobs were implemented that using
// cs.decode and split on the entire bytebuffer was a -bad-
// idea.  so we did it this way.  The 'command buffer' is
// the first part of the return packet.  it is what we use
// to get the information needed to parse the record.  it
// will never be > 256 bytes.
//
		i = (256 < buff.capacity() ? 256 : buff.capacity());
		cmd = new byte[i];
		buff.get(cmd, 0, i);
		String[] val = new String(cmd).split("[|]");
		if ((size = Integer.parseInt(val[0])) < 1)
			return(size);
		i = val[0].length() + val[1].length() + val[2].length() + val[3].length() + val[4].length() + 5;
		buff.position(i);
		Dataman.master.setlen(size);
		Dataman.master.setfmt(Short.parseShort(val[1]));
		_generation = Long.parseLong(val[2]);
		_curnode = Long.parseLong(val[3]);
		_offs = Short.parseShort(val[4]);
		tmp = buff.slice();
		_curkey = new DatamanKey(tmp, _keylen);
		tmp = tmp.slice();
		_fno = _curkey.get_fno() - 1;
		Dataman.master.setchan(_fno);
		_rptr = _curkey.get_rec();
		Dataman.master.setcur(_rptr);
		Dataman.master.set_filedesc(_files[_fno].get_desc());
		Dataman.master.sethead((short)_files[_fno].get_hlen());
		Dataman.master.setlongest((short)_files[_fno].get_longest());
		Dataman.cur_index = this;
		Dataman.master.in_rec(tmp);
		return(size);
	}

//
// this constructor is only used inside of mkidx
//
	DatamanIndex() {
		_idxname = null;
		_idxno = 0;
		_wrmode = 0;
		_fno = 0;
		_nfiles = 0;
		_keylen = 0;
		_longest = 0;
		_curnode = 0;
		_generation = 0;
		_rptr = 0;
		_offs = 0;
		_curkey = null;
		_files = new DatamanIndexFile[1];
		_files[0] = new DatamanIndexFile();
		_savptr = null;
	}
//
// this is the constructor that the world uses
//
/**
 * Define and open an index.  Name is the name of the index to open
 * and mode is either of the constants DatamanIndex.UPDATE, or
 * DatmanIndex.RDONLY.
 * @param name - the index name to open
 * @param mode - the open mode either RDONLY or UPDATE
 * @throws DatamanRuntimeException
 */
	public DatamanIndex(String name, int mode) {
		int j;
		int offs;
		int n;

		DatamanComms comm = new DatamanComms();

		DatamanErrVal datamanerrval;

		String cmd;
		ByteBuffer buff;
//
// currently we only allow 6 open indexs... check to
// see if they are exceeding that!
//
		for (j = -1, offs = 0;offs < 6; offs++) {
			if (DatamanIndex._onames[offs] == null || DatamanIndex._onames[offs].equals("")) {
				if (j == -1) 
					j = offs;
			} else if (DatamanIndex._onames[offs].equals(name)) {
				System.out.println(Dataman._progname + ": can't init index " + name);
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(DatamanErrVal.EIDXOPN));
			}
		}
//
// ok, we should be with it.  unless the index is already open!  don't
// want it open more than once!
//
		if (j == -1) {
			System.out.println(Dataman._progname + ": can't init index " + name);
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(DatamanErrVal.EIDXNOO));
		}
		offs = j;

		DatamanIndex._onames[offs] = name;
		cmd = DatamanFunc.IOPEN + "|" + name + "|" + Dataman._root+ "|";
//
// send the command, get the return, parse the result, and we're happy!
//
		try {
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd), cmd.length());
			cmd = new String(cs.decode(buff).array());
			String [] result = cmd.split("[|]" , 5);
			n = Integer.parseInt(result[0]);
			if (n < 0) {
				System.out.println(Dataman._progname + ": server error in IOPEN");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(n));
			}
			_idxname = DatamanIndex._onames[offs];
			_idxno = Integer.parseInt(result[1]);
			_keylen = Integer.parseInt(result[2]);
			_nfiles = Integer.parseInt(result[3]);
			_files = new DatamanIndexFile[_nfiles];
			String [] fnames = result[4].split("[\00]");
			for (n = 0;n < _nfiles;n++) {
				_files[n] = new DatamanIndexFile();
				_files[n].set_name(fnames[n]);
				_files[n].set_fno(n);
				n++;
			}
			_wrmode = mode;
		} catch (IOException e) {
			System.out.println("Caught IO exception in IOPEN" + e);
			e.printStackTrace();
		}
		_curkey = null;
		_savptr = null;
	}

//
// close an open index.  i'd rather have a destructor so that
// the index can just go out of scope and clean up the connection.
// but hey, this isn't C++
//
/**
 * Close an open index.  Any open index should be closed before it goes
 * out of scope
 * @throws DatamanRuntimeException
 */
	public void iclose() {

		DatamanComms comm = new DatamanComms();
	
		int i = 0;

		String cmd;

		ByteBuffer buff;

		for (i = 0;i < 6;i++) {
			if (DatamanIndex._onames[i].equals(_idxname)) {
				DatamanIndex._onames[i] = "";
				break;
			}
		}
		if (_curkey != null) 
			_curkey = null;
         
		if (_files != null) 
			_files = null;
         
		if (_savptr != null) 
			_savptr = null;
         
		cmd = DatamanFunc.ICLOSE + "|" + _idxno + "|";
		try {
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd), cmd.length());
			cmd = new String(cs.decode(buff).array());
			String [] result = cmd.split("[|]");
			i = Integer.parseInt(result[0]);
			if (i < 0) {
				System.out.println(Dataman._progname + ": iclose error");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
		}
		catch (IOException e) {
			System.out.println("Caught IO exception in ICLOSE" + e);
			e.printStackTrace();
		}
	}

//
// the get* routines.  get just retrieves the named key and it's
// associated data record.
//
/**
 * Lookup the named key and return it's associated record.  The key
 * to lookup is typically one returned by a prior call to
 * DatamanIndex.get_key(), which returns the internal representation
 * of the key.  Since the index will allow duplicate keys, this will
 * refer to a particular instance.
 * @param key - the key to search for.
 * @return true if the key was found, false if not.
 * @throws DatamanRuntimeException
 */
	public boolean get(DatamanKey key) {

		DatamanComms comm = new DatamanComms();

		int i;

		String cmd;
		ByteBuffer buff;

		if (Dataman.cur_index != null && Dataman.cur_index.get_wrmode() == DatamanIndex.UPDATE)
			Dataman.master.out_rec();

         Charset cs = Charset.forName("UTF-8");
         if (key.get_fno() > -1) {
			cmd = DatamanFunc.GET_CURRENT + "|" + _idxno + "|" + _generation + "|" + _curnode + "|" + _offs + "|";
			i = cmd.length() + _keylen + k_header_len;
			buff = ByteBuffer.allocate(i);
			buff.put(cs.encode(cmd));
			buff.put(key.get_data(), 0, _keylen + k_header_len);
			buff.rewind();
		} else {
			cmd = DatamanFunc.GET + "|" + _idxno + "|" + key.keyStr() + "|";
			buff = cs.encode(cmd);
			i = cmd.length();
		}

		try {
			buff = comm.db_send(buff, i);
			i = parse_get(buff);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during GET");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
			if (i == 0) 
				return (false);
		} catch (IOException e) {
			System.out.println("Caught IO exception in GET" + e);
			e.printStackTrace();
		}
		return (true);
	}
/**
 * Lookup the named key and return it's associated data record.  If the key is found
 * in the index the associated data record is stored in master datarecord.  Since
 * the index may contain duplicate keys the lowest order matching key is retrieved.
 * @param key - the key to search for.
 * @return true if the key was found, false if not.
 * @throws DatamanRuntimeException
 */
	public boolean get(String key) {
		return (get(new DatamanKey(key)));
	}
/**
 * Lookup the named key an return it's associated data record.  The string
 * representation of the data field is used as the key.  Since the index
 * may contain duplicate keys the lowest order matching key is retrieved.
 * @param key - the key to search for.
 * @return true if the key was found, false if not.
 * @throws DatamanRuntimeException
 */
	public boolean get(DatamanField key) {
   		return (get(new DatamanKey(key.getString())));
	}
/**
 * Retrieve the next higher order key and it's associated data record from the index.
 * @return true if successful, false if the current key is the highest order in the index.
 * @throws DatamanRuntimeException
 */
	public boolean get_next() {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;

		if (Dataman.in_xact && this._curkey.get_rec() < 0)
			return(false);
		if (Dataman.cur_index != null && Dataman.cur_index.get_wrmode() == DatamanIndex.UPDATE)
			Dataman.master.out_rec();

		if (_curkey == null || _curkey.keyStr().length() == 0) {
			System.out.println(Dataman._progname + ": Can't GET_NEXT");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(DatamanErrVal.ENOGET));
		}
		cmd = DatamanFunc.GET_NEXT + "|" + _idxno + "|" + _generation + "|" + _curnode + "|" + _offs + "|";
		i = cmd.length() + _keylen + k_header_len;
		Charset cs = Charset.forName("UTF-8");
		buff = ByteBuffer.allocate(i);
		buff.put(cs.encode(cmd));
		buff.put(_curkey.get_data(), 0, _keylen + k_header_len);
		buff.rewind();
		try {
			buff = comm.db_send(buff , i);
			i = parse_get(buff);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during GET_NEXT");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException( e.getDBErr(i));
			}
			if (i == 0) 
				return (false);
		} catch (IOException e) {
			System.out.println("Caught IO exception in GET_NEXT" + e);
			e.printStackTrace();
		}
		return (true);
	}
/**
 * Retrieve the next lower order key and it's associated data record from the index.
 * @return true if successful, false if the current key is the lowest order in the index.
 * @throws DatamanRuntimeException
 */
	public boolean get_prior() {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;

		if (Dataman.in_xact && this._curkey.get_rec() < 0)
			return(false);
		if (Dataman.cur_index != null && Dataman.cur_index.get_wrmode() == DatamanIndex.UPDATE)
			Dataman.master.out_rec();

		if (_curkey == null || _curkey.keyStr().length() == 0) {
			System.out.println(Dataman._progname + ": Can't GET_PRIOR");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(DatamanErrVal.ENOGET));
		}
		cmd = DatamanFunc.GET_PRIOR + "|" + _idxno + "|" + _generation + "|" + _curnode + "|" + _offs + "|";
		i = cmd.length() + _keylen + k_header_len;
		Charset cs = Charset.forName("UTF-8");
		buff = ByteBuffer.allocate(i);
		buff.put(cs.encode(cmd));
		buff.put(_curkey.get_data(), 0, _keylen + k_header_len);
		buff.rewind();
		try {
			buff = comm.db_send(buff , i);
			i = parse_get(buff);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during GET_PRIOR");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException( e.getDBErr(i));
			}
			if (i == 0) 
				return (false);
		} catch (IOException e) {
			System.out.println("Caught IO exception in GET_PRIOR" + e);
			e.printStackTrace();
		}
		return (true);
	}
/**
 * Retrieve the lowest order key and it's associated data record from the index.
 * @return true if successful, false if the index is empty.
 * @throws DatamanRuntimeException
 */
	public boolean get_first() {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;

		if (Dataman.cur_index != null && Dataman.cur_index.get_wrmode() == DatamanIndex.UPDATE)
			Dataman.master.out_rec();

		cmd = DatamanFunc.GET_FIRST + "|" + _idxno + "|";
		try {
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd), cmd.length());
			i = parse_get(buff);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during GET_FIRST");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
			if (i == 0) 
				return (false);
		} catch (IOException e) {
              System.out.println("Caught IO exception in GET_FIRST" + e);
              e.printStackTrace();
		}
		return (true);
	}
/**
 * Retrieve the highest order key and it's associated data record from the index.
 * @return true if successful, false if the index is empty.
 * @throws DatamanRuntimeException
 */
	public boolean get_last() {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;

		if (Dataman.cur_index != null && Dataman.cur_index.get_wrmode() == DatamanIndex.UPDATE)
			Dataman.master.out_rec();

		cmd = DatamanFunc.GET_LAST + "|" + _idxno + "|";
		try {
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd), cmd.length());
			i = parse_get(buff);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during GET_LAST");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
			if (i == 0) 
				return (false);
		} catch (IOException e) {
			System.out.println("Caught IO exception in GET_LAST" + e);
			e.printStackTrace();
		}
		return (true);
	}
/**
 * Retrieve the record associated with the current key.  Because a key is not
 * required for each record, and use of the forward an back methods, the current
 * record may not be the one associated with the current key.
 * @return true if successful, false if the key has been removed.
 * @throws DatamanRuntimeException
 */
	public boolean get_current() {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;

		if (Dataman.in_xact && this._curkey.get_rec() < 0)
			return(false);
		if (Dataman.cur_index != null && Dataman.cur_index.get_wrmode() == DatamanIndex.UPDATE)
			Dataman.master.out_rec();

		if (_curkey == null || _curkey.keyStr().length() == 0) {
			System.out.println(Dataman._progname + ": Can't GET_CURRENT");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(DatamanErrVal.ENOGET));
		}
		cmd = DatamanFunc.GET_CURRENT + "|" + _idxno + "|" + _generation + "|" + _curnode + "|" + _offs + "|";
		i = cmd.length() + _keylen + k_header_len;
		buff = ByteBuffer.allocate(i);
		Charset cs = Charset.forName("UTF-8");
		buff.put(cs.encode(cmd));
		buff.put(_curkey.get_data(), 0, _keylen + k_header_len);
		buff.rewind();
		try {
			buff = comm.db_send(buff , i);
			i = parse_get(buff);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during GET_CURRENT");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException( e.getDBErr(i));
			}
			if (i == 0) 
				return (false);
		} catch (IOException e) {
			System.out.println("Caught IO exception in GET_CURRENT" + e);
			e.printStackTrace();
		}
		return (true);
	}
/**
 * Retrieve the record following the current one.
 * @return true if successful, false if the current record is the last in the file.
 * @throws DatamanRuntimeException
 */
	public boolean forward() {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;

		if (Dataman.in_xact && this._rptr < 0)
			return(false);
		if (Dataman.cur_index != null && Dataman.cur_index.get_wrmode() == DatamanIndex.UPDATE)
			Dataman.master.out_rec();

		if (_curkey == null || _curkey.get_len() == 0) {
			System.out.println(Dataman._progname + ": Error during FORWARD");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(DatamanErrVal.ENOGET));
		}
		cmd = DatamanFunc.FORWARD + "|" + _idxno + "|" + _fno + "|" + _rptr + "|";
		try {
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd), cmd.length());
			i = (256 < buff.capacity()? 256 : buff.capacity());
			byte [] stuff = new byte[i];
			buff.get(stuff, 0, i);
			String [] result = new String(stuff).split("[|]");
			i = Integer.parseInt(result[0]);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during FORWARD");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException( e.getDBErr(i));
			}
			if (i == 0) 
				return (false);
			i = result[0].length() + result[1].length() + result[2].length() + 3;
			buff.position(i);
			ByteBuffer data = buff.slice();
			_rptr = Long.parseLong(result[1]);
			Dataman.master.setcur(_rptr);
			Dataman.cur_index = this;
			Dataman.master.set_filedesc(_files[_fno].get_desc());
			Dataman.master.sethead((short)_files[_fno].get_hlen());
			Dataman.master.setlen(i);
			Dataman.master.setfmt(Short.parseShort(result[2]));
			Dataman.master.in_rec(data);
		} catch (IOException e) {
			System.out.println("Caught IO exception in FORWARD" + e);
			e.printStackTrace();
		}
		return (true);
	}
/**
 * Retrieve the record preceeding the current one.
 * @return true if successful, false if the current record is the first in the file.
 * @throws DatamanRuntimeException
 */
	public boolean back() {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;

		if (Dataman.in_xact && this._rptr < 0)
			return(false);
		if (Dataman.cur_index != null && Dataman.cur_index.get_wrmode() == DatamanIndex.UPDATE)
			Dataman.master.out_rec();

         if (_curkey == null || _curkey.get_len() == 0) {
			System.out.println(Dataman._progname + ": Error during BACK");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(DatamanErrVal.ENOGET));
		}
		cmd = DatamanFunc.BACK + "|" + _idxno + "|" + _fno + "|" + Dataman.master.getcur() + "|";
		try {
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd), cmd.length());
			i = (256 < buff.capacity() ? 256 : buff.capacity());
			byte [] stuff = new byte[i];
			buff.get(stuff, 0, i);
			String [] result = new String(stuff).split("[|]");
			i = Integer.parseInt(result[0]);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during BACK");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException( e.getDBErr(i));
			}
			if (i == 0) 
				return (false);

			i = result[0].length() + result[1].length() + result[2].length() + 3;
			buff.position(i);
			ByteBuffer rec = buff.slice();
			_rptr = Long.parseLong(result[1]);
			Dataman.master.setcur(_rptr);
			Dataman.cur_index = this;
			Dataman.master.set_filedesc(_files[_fno].get_desc());
			Dataman.master.sethead((short)_files[_fno].get_hlen());
			Dataman.master.setlen(i);
			Dataman.master.setfmt(Short.parseShort(result[2]));
			Dataman.master.in_rec(rec);
		} catch (IOException e) {
			System.out.println("Caught IO exception in BACK" + e);
			e.printStackTrace();
		}
		return (true);
    }
/**
 * Lock the current database record.  This function exerts a co-operative lock on
 * the current data record.  If the record is already protected it will sleep up
 * to 500 msec before returning, during which the lock may still be exerted.
 * @return true if the lock was exerted, false if already protected.
 * @throws DatamanRuntimeException
 */
	public boolean protect() {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;
		short j;

		if (Dataman.in_xact && this._curkey.get_rec() < 0)
			return(false);
		cmd = DatamanFunc.PROTECT + "|" + _idxno + "|" + _fno + "|" + _rptr + "|";
		try {
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd), cmd.length());
			i = (256 < buff.capacity() ? 256 : buff.capacity());
			byte [] rec = new byte[i];
			buff.get(rec, 0, i);
			String [] result = new String(cmd).split("[|]");
			i = Integer.parseInt(result[0]);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during PROTECT");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
			if (i == 0) 
				return (false);

			i = result[0].length() + result[1].length() + result[2].length() + result[3].length() + result[4].length() + 5;
			buff.position(i);
			ByteBuffer data = buff.slice();
			j = Short.parseShort(result[1]);
			Dataman.master.set_filedesc(_files[_fno].get_desc());
			Dataman.master.sethead((short)_files[_fno].get_hlen());
			Dataman.master.setcur(_rptr);
			Dataman.master.setchan(_fno);
			Dataman.master.setlen(i);
			Dataman.master.setfmt(j);
			Dataman.master.in_rec(data);
		}
		catch (IOException e) {
              System.out.println("Caught IO exception in PROTECT" + e);
              e.printStackTrace();
		}
		return (true);
	}
/**
 * Unlock the current database record.  This function removes a co-operative lock on
 * the current data record.  It is an unconditional clear; it doesn't matter who
 * exerted the lock, this will remove it.
 * @throws DatamanRuntimeException
 */
	public void clear() {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;

		if (Dataman.in_xact && this._curkey.get_rec() < 0)
			return;
		if (Dataman.cur_index != null && Dataman.cur_index.get_wrmode() == DatamanIndex.UPDATE)
			Dataman.master.out_rec();

		cmd = DatamanFunc.CLEAR + "|" + _idxno + "|" + _fno + "|" + Dataman.master.getcur() + "|";
		try {
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd) , cmd.length());
			cmd = new String(cs.decode(buff).array());
			String [] result = cmd.split("[|]");
			if ((i = Integer.parseInt(result[0])) < 0) {
				System.out.println(Dataman._progname + ": error during CLEAR");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
		}
		catch (IOException e) {
			System.out.println("Caught IO exception in CLEAR" + e);
			e.printStackTrace();
		}
	}
/**
 * Delete the current database record.  This has the side-effect of
 * making any key pointing to the record invalid.  When any attempt
 * is made to retrieve a key pointing to a deleted record the key
 * is removed from the index.  Thus, it is best to explicitly remove
 * all of a record's keys before deleting the record.
 * @throws DatamanRuntimeException
 */
	public void delete() {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;

		if (Dataman.cur_index != null && Dataman.cur_index.get_wrmode() == DatamanIndex.UPDATE)
			Dataman.master.out_rec();

		if (_rptr == 0) {
			System.out.println(Dataman._progname + ": Error during DELETE");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(DatamanErrVal.ENOGET));
		}
		cmd = DatamanFunc.DELETE + "|" + _idxno + "|" + _fno + "|" + _rptr + "|"
				+ Dataman.in_xact + "|";
		try {
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd), cmd.length());
			i = (256 < buff.capacity() ? 256 : buff.capacity());
			byte [] stuff = new byte[i];
			buff.get(stuff, 0, i);
			String [] result = new String(stuff).split("[|]");
			i = Integer.parseInt(result[0]);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during DELETE");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
			i = result[0].length() + result[1].length() + result[2].length() + 3;
			buff.position(i);
			ByteBuffer data = buff.slice();
			Dataman.master.setlen(i);
			_rptr = Long.parseLong(result[1]);
			Dataman.master.setfmt(Short.parseShort(result[2]));
			Dataman.master.setcur(_rptr);
			Dataman.master.setchan(_fno);
			Dataman.master.set_filedesc(_files[_fno].get_desc());
			Dataman.master.sethead((short)_files[_fno].get_hlen());
			Dataman.cur_index = this;
			Dataman.master.in_rec(data);
		} catch (IOException e) {
			System.out.println("Caught IO exception in DELETE" + e);
			e.printStackTrace();
   		}
	}
/**
 * Remove the specified key from the index.  The key
 * to remove is typically one returned by a prior call to
 * DatamanIndex.get_key(), which returns the internal representation
 * of the key.  Since the index will allow duplicate keys, this will
 * refer to a particular instance to be removed.
 * @param key - the key to remove from the index.
 * @return true if the key was removed, false if no matching key was found
 * @throws DatamanRuntimeException
 */
	public boolean remove(DatamanKey key) {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;

		if (_wrmode == DatamanIndex.RDONLY) {
   			System.out.println(Dataman._progname + ": in remove - index " + _idxname + " not opened for update");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(0));
		}
		if (key.keyStr().indexOf('*') > -1) {
			System.out.println(Dataman._progname + ": can't use wildcard in remove");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(0));
		}
		Charset cs = Charset.forName("UTF-8");
		cmd = DatamanFunc.REMOVE + "|" + _idxno + "|" + Dataman.in_xact + "|";
		if (key.get_fno() > 0) {
			i = cmd.length() + _keylen + k_header_len;
			buff = ByteBuffer.allocate(i);
			buff.put(cs.encode(cmd));
			buff.put(key.get_data(), 0, _keylen + k_header_len);
		} else {
			cmd += key.keyStr();
			i = cmd.length();
			buff = cs.encode(cmd);
		}
		try {
			buff = comm.db_send(buff, i);
			cmd = new String(cs.decode(buff).array());
			String [] result = cmd.split("[|]");
			i = Integer.parseInt(result[0]);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during REMOVE");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
			if (i == 0) 
				return (false);
		} catch (IOException e) {
			System.out.println("Caught IO exception in REMOVE" + e);
			e.printStackTrace();
		}
		return (true);
	}
/**
 * Remove the specified key from the index.
 * Since the index will allow duplicate keys, this will
 * remove the first matching instance of the key
 * @param key - the key to remove from the index.
 * @return true if the key was removed, false if no matching key was found
 * @throws DatamanRuntimeException
 */
	public boolean remove(String key) {
		if (key.length() > _keylen)
			return (remove(new DatamanKey(key.substring(0,_keylen))));
		else
			return (remove(new DatamanKey(key)));
	}
/**
 * Save the current state of the index.  This will allow the user
 * to restore to that state later.
 * @throws DatamanRuntimeException
 */
	public void save() {


		if (_curkey == null || _curkey.get_len() == 0) {
              System.out.println(Dataman._progname + ": Error during SAVE");
              DatamanErrVal e = new DatamanErrVal();
              throw new DatamanRuntimeException(e.getDBErr(DatamanErrVal.ENOGET));
		}
		_savptr = new DatamanIndexSave(_curnode, _rptr, _fno, _curkey, Dataman.master.getfmt(), _offs);
	}
/**
 * Restore the saved state of the index.  This will restore the
 * current index to a saved state.  This is important because
 * the MFRP doesn't necessarily refer to the record that the
 * current key does, so it behaves quite differently from
 * get_current().
 * @return true if the state was successfully restored, else false.
 * @throws DatamanRuntimeException
 */
	public boolean restore() {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		DatamanKey key;

		int i;

		if (Dataman.cur_index != null && Dataman.cur_index.get_wrmode() == DatamanIndex.UPDATE)
			Dataman.master.out_rec();

		if (_savptr == null) {
			System.out.println(Dataman._progname + ": in restore, index " + _idxname + " has not been saved");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(0));
		}
		cmd = DatamanFunc.RESTORE + "|" + _idxno + "|" + _savptr.getnode() +
				"|" + _savptr.getoffs() + "|" + _savptr.getrec() + "|";
		Charset cs = Charset.forName("UTF-8");
		key = _savptr.getkey();
		i = cmd.length() + _keylen + k_header_len;
		buff = ByteBuffer.allocate(i);
		buff.put(cs.encode(cmd));
		buff.put(key.get_data(), 0, _keylen + k_header_len);
		buff.rewind();
		try {
			buff = comm.db_send(buff, i);
			i = parse_get(buff);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during RESTORE");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
			if (i == 0) {
				_savptr = null;
				return (false);
			}
		} catch (IOException e) {
			System.out.println("Caught IO exception in RESTORE" + e);
			e.printStackTrace();
		}
		_savptr = null;
		return (true);
	}
/**
 * Add a key to an index.  This method is used only in sort routines.  It associates
 * the current work record with the passed in key, and stores that key in the index.
 * @param key - the key to sort into the index.
 * @throws DatamanRuntimeException
 */
	public void sort(String key) {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;

		if (!Dataman.is_sort) {
			System.out.println(Dataman._progname + ": trying to use SORT in a "
				 + "non end-sort program");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(0));
		}
		cmd = DatamanFunc.SORT + "|" + Dataman.cur_index.get_idxno() + "|" +
				Dataman._fileno + "|" + Dataman.workfile.getcur() + "|" + key + "|";
		try {
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd), cmd.length());
			cmd = new String(cs.decode(buff).array());
			String [] result = cmd.split("[|]");
			i = Integer.parseInt(result[0]);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during SORT");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
		} catch (IOException e) {
			System.out.println("Caught IO exception in SORT" + e);
			e.printStackTrace();
		}
	}
/**
 * Add a key to an index.  This method is used only in sort routines.  It associates
 * the current work record with the string representation of the passed in DataField,
 * and stores that key in the index.
 * @param key - the key to sort into the index.
 * @throws DatamanRuntimeException
 */
	public void sort(DatamanField key) {
		sort(key.getString());
	}
/**
 * Add a key to an index.  This method is used only in sort routines.  It associates
 * the current work record with the string representation of the passed in integer,
 * and stores that key in the index.
 * @param i - the key to sort into the index.
 * @throws DatamanRuntimeException
 */
	public void sort(int i) {
   		sort(new String("" + i));
	}
/**
 * Insert a new record in the databse.  This takes the current
 * record pointed at in this index, and will put a insert record
 * either following or preceeding it into the database.  That
 * new (blank) record will become the current one.
 * @param fmt - the record format number of the new record.
 * @param pos - either DatamanIndex.BEFORE or DatamanIndex.AFTER
 * @throws DatamanRuntimeException
 */
	public void insert(int fmt, int pos) {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;

		if (Dataman.cur_index != null && Dataman.cur_index.get_wrmode() == DatamanIndex.UPDATE)
			Dataman.master.out_rec();

		if (_wrmode != DatamanIndex.UPDATE) {
			System.out.println(Dataman._progname + ": index " + _idxname + " not opened for update");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(0));
		}
		if (_curkey == null || _curkey.get_len() == 0) {
			System.out.println(Dataman._progname + ": Error during INSERT");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(DatamanErrVal.ENOGET));
		}
		cmd = DatamanFunc.INSERT + "|" + fmt + "|" + pos + "|" + _idxno + "|" + _fno + "|" + _rptr + "|";
		try {
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd), cmd.length());
//
// on insert, we don't have to deal with blobs.  this makes it so we don't have to
// deal with the junk we do on the get*, back, and forward routines.  this might
// be slow with large records, but we'll deal with that if it come up.
//
			cmd = new String(cs.decode(buff).array());
			String [] result = cmd.split("[|]");
			i = Integer.parseInt(result[0]);
			if (i < 0) {
				System.out.println(Dataman._progname + ": error during INSERT");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
			Dataman.master.setfmt((short)fmt);
			_rptr = Long.parseLong(result[1]);
//	tmp = idx->_files[idx->_fno]._filedesc->record_desc[fmt-1].rf_len;
			Dataman.master.setcur(_rptr);
			i = _files[_fno].get_desc().getDesc()[fmt-1].get_rflen();
			Dataman.master.setlen(i);
			Dataman.cur_index = this;
			buff = ByteBuffer.allocate(i);
			for (; i != 0; i--)
				buff.put((byte)' ');

			Dataman.master.in_rec(buff);
		} catch (IOException e) {
			System.out.println("Caught IO exception in INSERT" + e);
			e.printStackTrace();
		}
	}
/**
 * Include a new key into an index.  The symantecs of this is
 * a little tricky.  This will take the record pointed to in
 * the index that is the argument, associate it with the key,
 * and put the named key into 'this' index.
 * @param idx - the source index.
 * @param key - the new key to include
 * @throws DatamanRuntimeException
 */
	public void include(DatamanIndex idx, String key) {

		DatamanComms comm = new DatamanComms();

		String cmd;
		ByteBuffer buff;

		int i;

		if (Dataman.is_sort) {
			System.out.println(Dataman._progname + ": trying to use INCLUDE in a " + "non file-edit program");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(0));
		}
		if (_wrmode != DatamanIndex.UPDATE) {
			System.out.println(Dataman._progname + ": index " + _idxname + "not opened for update");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(0));
		}
		for (i = 0; i < _nfiles; i++) {
			if (_files[i].get_fname().equals(idx.get_file(idx.get_fno()).get_fname())) 
				break;
		}
		if (i >= _nfiles) {
			System.out.println(Dataman._progname + ": Include error: file " +
				idx.get_file(idx.get_fno()).get_fname() +
				"is not a member " + "of index " + _idxname);
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(0));
		}
		_fno = i;
		cmd = DatamanFunc.INCLUDE + "|" + idx.get_idxno() + "|" + idx.get_fno() +
				"|" + _idxno + "|" + _fno + "|" + idx.get_rptr() + "|" + key + "|";
		try {
			Charset cs = Charset.forName("UTF-8");
				buff = comm.db_send(cs.encode(cmd) , cmd.length());
				byte[] response = new byte[buff.remaining()];
				buff.get(response);
				int[] separator = new int[4];
				int count = 0;
				for (int pos = 0; pos < response.length && count < separator.length; pos++) {
					if (response[pos] == (byte)'|')
						separator[count++] = pos;
				}
				if (count != separator.length)
					throw new DatamanRuntimeException("Malformed INCLUDE response");
				i = Integer.parseInt(new String(response, 0, separator[0], cs));
				if (i < 0) {
				System.out.println(Dataman._progname + ": error during INCLUDE");
				DatamanErrVal e = new DatamanErrVal();
					throw new DatamanRuntimeException(e.getDBErr(i));
				}
				if (i == 0)
					return;
				_generation = Long.parseLong(new String(response,
						separator[0] + 1, separator[1] - separator[0] - 1, cs));
				_curnode = Long.parseLong(new String(response,
						separator[1] + 1, separator[2] - separator[1] - 1, cs));
				_offs = Short.parseShort(new String(response,
						separator[2] + 1, separator[3] - separator[2] - 1, cs));
				if (Dataman.in_xact)
					_curkey = new DatamanKey(key, _fno + 1, idx.get_rptr(), _keylen);
				else
					_curkey = new DatamanKey(ByteBuffer.wrap(response,
							separator[3] + 1, response.length - separator[3] - 1).slice(), _keylen);
			_fno = _curkey.get_fno() - 1;
		} catch (IOException e) {
			System.out.println("Caught IO exception in INCLUDE" + e);
			e.printStackTrace();
		}
	}
/**
 * Include a new key into an index.  The symantecs of this is
 * a little tricky.  This will take the record pointed to in
 * the index that is the argument, associate it with the key,
 * and put the named key into 'this' index.
 * @param idx - the source index.
 * @param field - the key to include.
 * @throws DatamanRuntimeException
 */
	public void include(DatamanIndex idx, DatamanField field) {
		include(idx, field.getString());
	}
/**
 * Include a new key into an index.  This will take the current record
 * pointed to in this index, associate it with the key, and put the
 * named key into 'this' index.
 * @param key - the new key to include.
 * @throws DatamanRuntimeException
 */
	public void include(String key) {
		include(this, key);
	}
/**
 * Include a new key into an index.  This will take the current record
 * pointed to in this index, associate it with the string representation
 * of the data field, and put this named key into 'this' index.
 * @param field - the new key to include.
 * @throws DatamanRuntimeException
 */
	public void include(DatamanField field) {
		include(this, field.getString());
	}
/**
 * Return the String name of this index.
 * @return String
 */
	public String get_ixname() { return (_idxname); }
/**
 * Returns this index's current key
 * @return DatamanKey
 */
	public DatamanKey get_key() { return (_curkey); }

	int get_wrmode() { return (_wrmode); }
	int get_idxno() { return (_idxno); }
	void set_idxno(int i) { _idxno = i; }
	int get_nfiles() { return (_nfiles); }
	int get_fno() { return (_fno); }
	int get_keylen() { return (_keylen); }
	void set_keylen(int i) { _keylen = i; }
	long get_curnode() { return (_curnode); }
	void set_curnode(long i) { _curnode = i; }
	void set_ixname(String string) { _idxname = string; }
	long get_rptr() { return (_rptr); }
	DatamanIndexFile get_file(int i) { return (_files[i]); }
	DatamanIndexFile[] get_files() { return (_files); }
/**
 * Return the name of the current data file.  This returns
 * the name of the data file that contains the current record.
 * @return String
 */
	public String fileName() { return _files[_fno].get_fname(); }

	void set_onames(int i ,String name) {
		DatamanIndex._onames[i] = name;
	}
	String get_onames(int i) {
		return (DatamanIndex._onames[i]);
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
