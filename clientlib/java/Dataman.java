// ***************************************************************
//
// CLASS:		Dataman.java
//
// PROJECT:		dataman client side java library
// 
// DATE:		Mon Aug 28 21:01:37 MDT 2006
// 
// AUTHOR:		Tom Green
// 
// FILES:
//
// MODIFICATION HISTORY:
//				Mon Aug 28 21:01:37 MDT 2006
//				combined DatamanGlobs.java and DatamanInit.java
//				into this class named Dataman.java.  Makes
//				referencing things much nicer and cuts down on
//				things you need to know.
//				added new transaction processing methods.
//				tomg
//
//***************************************************************

//
// define the global values that the user will need.  those
// are the public ones.  the client routines will need to
// use the package visible ones.
//
package Dataman;

import java.io.IOException;
import java.io.PrintStream;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;

/**
 * Global entries for Dataman programs.  The master and workfile records and the
 * last accessed index are made available here.  The master record is where data
 * retrieved from indexes are stored.  The workfile record is where data from the
 * command line workfile is stored.
 * Also, methods to initialize the two types of Dataman processes.  There
 * are two types of Dataman processes.  The first is the end sort
 * program, where new indexes are built.  The second is the file edit
 * program, where records are retrieved, perused, and/or modified.
 * makeIndex initializes the sort program, and initDataman initializes
 * the edit programs.
 */
public class Dataman{
/**
 * This is the master data record.  This is where any data record is stored from
 * any of the index routines.
 */
	public static DatamanRecord master;
/**
 * This is the workfile record. This is where the data record for the named work
 * file is stored.
 */
	public static DatamanRecord workfile;
/**
 * This is a reference to the last operated on index.
 */
	public static DatamanIndex cur_index;
	static int _fileno;
	static int _maxfil;
	static String[] _fnames;
	static String _root;
	static String _progname;
	static boolean is_sort;
	static boolean in_xact;
/**
 * Construct an instance to reference Dataman.
 */
    public Dataman() {}

/**
 * Initialize a Dataman file edit routine.  This is the first function
 * to call to initialize the dataman system.  The arugments are the
 * main class name (what you want displayed in error messages), and
 * the argv argument vector that was passed to main.  The calling
 * sequence is currently: java ClassName -h hostname -r root workfile.
 * where hostname is the name of the server hosting the database,
 * root is the root directory of the database, and workfile is the name
 * of the workfile.
 * @param ClassName - name of the calling class
 * @param argv - the argument vector to main()
 * @throws DatamanRuntimeException
 */
    public static void initDataman(String ClassName, String[] argv)
    {
		DatamanComms comm;

		master = new DatamanRecord(DatamanRecord.MASTER);
		workfile = new DatamanRecord(DatamanRecord.WORK);
		cur_index = null;
		_fileno = 0;
		_maxfil = 0;
		_fnames = null;
		_root = "";
		is_sort = false;
		in_xact = false;
		_progname = ClassName;
		boolean traditional = false;

		String cmd;
		String host = "";
		ByteBuffer buff;

		int argc = argv.length;
		int i, j;

		for (i = 0; i < argc; i++) {
			int k;
			if (argv[i].charAt(0) != '-') 
				break;
			for (j = 1; j < argv[i].length(); j++) {
				switch (argv[i].charAt(j)) {
					case 'h': 
						if (host.length() > 0) 
							useage();
						host = argv[++i];
						j = argv[i].length() + 1;
						break;
					case 'n':
						if (traditional)
							useage();
						traditional = true;
						break;
					case 'r': 
						if (_root.length() > 0) 
							useage();
                             
						_root = argv[++i];
						j = argv[i].length() + 1;
						break;
					default:
						System.out.println("unknown switch '" + argv[i].charAt(j) + "'");
						useage();
				}
			}
		}
		if (i >= argc) 
			useage();

		if (_root.length() == 0) {
			System.out.println("ROOT not defined");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(0));
		}
		if (host.length() == 0)
			host = "localhost";

		try {
   			comm = new DatamanComms(host);
			if (!traditional) {
				cmd = DatamanFunc.INIT_DAT + "|" + _root + "/files/" + argv[i] + "|";
				Charset cs = StandardCharsets.UTF_8;
				buff = comm.db_send(cs.encode(cmd), cmd.length());
				String[] result = readFields(buff, 1, "INIT_DATAMAN");
				i = Integer.parseInt(result[0]);
				if (i < 0) {
					System.out.println(_progname + ": Error during INIT_DATAMAN");
					DatamanErrVal e = new DatamanErrVal();
					throw new DatamanRuntimeException(e.getDBErr(i));
				}
				result = readFields(buff, 6, "INIT_DATAMAN");
				workfile.setlen(i);
				workfile.setchan(Integer.parseInt(result[0]));
				workfile.setlongest(Short.parseShort(result[1]));
				workfile.setfmt(Short.parseShort(result[2]));
				workfile.setcur(Long.parseLong(result[3]));
				workfile.setprev(Long.parseLong(result[4]));
				workfile.setnext(Long.parseLong(result[5]));
				workfile.in_rec(buff.slice());
			}
		} catch (IOException e) {
			throw transportError("INIT_DATAMAN", e);
		}
	};
/**
 * Initialize a Dataman end sort routine.  This is the first function
 * to call to initialize a dataman sort program.  The arugments are the
 * main class name (what you want displayed in error messages), and
 * the argv argument vector that was passed to main.  The calling
 * sequence is currently:
 * java ClassName -h hostname -r root [-size] idxname file_1, file_2, ... file_n
 * where hostname is the name of the server hosting the database,
 * root is the root directory of the database, size is the maximum key length
 * allowed in the index (1-32), idxname is the index to create, and each
 * subsequent file name are the files to sort through and build the index on.
 * @param ClassName - name of the calling class
 * @param argv - the argument vector to main()
 * @throws DatamanRuntimeException
 */
	public static DatamanIndex makeIndex(String ClassName, String[] argv) {

		DatamanComms comm;

		workfile = new DatamanRecord(DatamanRecord.WORK);
		cur_index = new DatamanIndex();
		_fileno = 0;
		_maxfil = 0;
		_fnames = null;
		_root = "";
		_progname = ClassName;

		String cmd;
		String host = "";
		ByteBuffer buff;

		int argc = argv.length;
		int i, j;

		for (i = 0; i < argc; i++) {
			if (argv[i].charAt(0) != '-') 
				break;
			for (j = 1; j < argv[i].length(); j++) {
				switch (argv[i].charAt(j)) {
					case 'h': 
						if (host.length() > 0)
							useage(_progname);
						host = argv[++i];
						j = argv[i].length() + 1;
						break;
					case 'r': 
						if (_root.length() > 0)
							useage(_progname);
						_root = argv[++i];
						j = argv[i].length() + 1;
						break;
                                                
					case '0': case '1': case '2': case '3': case '4': 
					case '5': case '6': case '7': case '8': case '9': 
						if (cur_index.get_keylen() > 0)
							useage(_progname);
						int k = Integer.parseInt(argv[i].substring(j));
						cur_index.set_keylen(k);
						if (k > DatamanKey.MAX_KEY_SIZE || k < DatamanKey.MIN_KEY_SIZE) {
                             System.out.println(_progname + ": keysize " +
									k + " invalid - min " +DatamanKey.MIN_KEY_SIZE +
									", max " + DatamanKey.MAX_KEY_SIZE);
                             DatamanErrVal e = new DatamanErrVal();
                             throw new DatamanRuntimeException(e.getDBErr(0));
						}
						j = argv[i].length() + 1;
						break;
                                                
					default: 
						System.out.println("unknown switch '" + argv[i].charAt(j) + "'");
						useage(_progname);
				}
			}
		}

   		if (i >= argc-1)
			useage(_progname);
		cur_index.set_onames(0 , argv[i++]);
		cur_index.set_ixname(cur_index.get_onames(0));
		_maxfil = argc - i;
		_fnames = new String[_maxfil];
		for (j = 0;i < argc; i++, j++) {
			if (argv[i].startsWith("-"))
				useage(_progname);
			_fnames[j] = argv[i];
		}
		if (_root.length() == 0) {
			System.out.println(_progname + ": no database ROOT set");
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(0));
		}
		if (host.length() == 0) 
			host = "localhost";
		cmd = DatamanFunc.MKIDX + "|" + cur_index.get_keylen() + "|" +
			cur_index.get_ixname() + "|" + _root + "|" + _maxfil + "|";
		for (i = 0; i < _maxfil; i++)
			cmd += _fnames[i] + "|";
		try {
			comm = new DatamanComms(host);
			Charset cs = StandardCharsets.UTF_8;
			buff = comm.db_send(cs.encode(cmd), cmd.length());
			String[] result = readFields(buff, 1, "MKIDX");
			i = Integer.parseInt(result[0]);
			if (i < 0) {
     			System.out.println(_progname + ": Error during MKIDX");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
			result = readFields(buff, 8, "MKIDX");
			workfile.setlen(i);
			cur_index.set_idxno(Integer.parseInt(result[0]));
			cur_index.set_curnode(Long.parseLong(result[1]));
			DatamanIndexFile f = cur_index.get_file(0);
			f.set_fno(0);
			f.set_name(_fnames[0]);
			f.set_hlen(Short.parseShort(result[4]));

			workfile.setchan(Integer.parseInt(result[2]));
			workfile.sethead(Short.parseShort(result[4]));
			workfile.setcur(Long.parseLong(result[5]));
			workfile.setfmt(Short.parseShort(result[6]));
			workfile.setnext(Long.parseLong(result[7]));
			workfile.in_rec(buff.slice());

			f.set_desc(workfile.get_desc());
			workfile.setfile(true);
			is_sort = true;
		} catch (IOException e) {
			throw transportError("MKIDX", e);
		}
		return (cur_index);
	}


	static void useage() {

		System.out.println(_progname + ": usage: " + _progname + " [-n][-h host] [-r root] workfile\n" +
				"\t-h host is database server host to connect to\n" +
				"\t-n non traditional (no workfile record\n" +
				"\t-r root is database root directory on server\n");
		DatamanErrVal e = new DatamanErrVal();
		throw new DatamanRuntimeException(e.getDBErr(0));
	}


	static void useage(String name) {
		System.out.println(name + ": Usage: " + name + " [-len] [-D] [-h host] [-r rootdir] " +
				"idx_name file1 [file2 file3 ...]\n\t-len where len is the max " +
				"key length\n\t-D   turn on global debugging" +
				"\n\t-h   host is the database server host" +
				"\n\t-r   rootdir is the database root directory\n");
		DatamanErrVal e = new DatamanErrVal();
		throw new DatamanRuntimeException(e.getDBErr(0));
	}

/**
 * Start a new transaction.  Any changes to a database between this
 * command and either commit or rollback will not be committed to
 * the database until the commit method is called.
 * @throws DatamanRuntimeException
 */
	public static void startTransaction() {

		String cmd;
		DatamanComms comm;
		ByteBuffer buff;

		cmd = DatamanFunc.START_XACT + "|";
		try {
			comm = new DatamanComms();
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd), cmd.length());
			cmd = new String(cs.decode(buff).array());
			String [] result = cmd.split("[|]");
			int i = Integer.parseInt(result[0]);
			if (i < 0) {
     			System.out.println(_progname + ": Error during MKIDX");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
		} catch (IOException e) {
			throw transportError("START_XACT", e);
		}
		in_xact = true;
	}

/**
 * Roll back a transaction.  This command terminates a transaction and
 * does not commit any part of the transaction to the database.
 * @throws DatamanRuntimeException
 */
	public static void rollback() {

		String cmd;
		DatamanComms comm;
		ByteBuffer buff;

		cmd = DatamanFunc.ROLLBACK + "|";
		try {
			comm = new DatamanComms();
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd), cmd.length());
			cmd = new String(cs.decode(buff).array());
			String [] result = cmd.split("[|]");
			int i = Integer.parseInt(result[0]);
			if (i < 0) {
     			System.out.println(_progname + ": Error during ROLLBACK");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
		} catch (IOException e) {
			throw transportError("ROLLBACK", e);
		}
		in_xact = false;
	}

/**
 * Commit a transaction.  This command terminates a transaction and
 * commits the transaction to the database.  It is atomic.  If any
 * part of the transaction fails, all parts fail and it is as if a
 * rollback were called.
 * @return true if the commit was successful, else false.
 * @throws DatamanRuntimeException
 */
	public static boolean commit() {

		String cmd;
		DatamanComms comm;
		ByteBuffer buff;
		int i;

		if (cur_index != null && cur_index.get_wrmode() == DatamanIndex.UPDATE)
			master.out_rec();

		cmd = DatamanFunc.COMMIT + "|";
		try {
			comm = new DatamanComms();
			Charset cs = Charset.forName("UTF-8");
			buff = comm.db_send(cs.encode(cmd), cmd.length());
			cmd = new String(cs.decode(buff).array());
			String [] result = cmd.split("[|]");
			i = Integer.parseInt(result[0]);
			if (i < 0) {
     			System.out.println(_progname + ": Error during COMMIT");
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(i));
			}
			in_xact = false;
			if (i == 0)
				return(false);
			return(true);
		} catch (IOException e) {
			throw transportError("COMMIT", e);
		}
	}

	private static DatamanRuntimeException transportError(String operation,
		IOException cause) {
		return new DatamanRuntimeException(
			_progname + ": transport error during " + operation, cause);
	}

	private static String[] readFields(ByteBuffer buffer, int count,
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
