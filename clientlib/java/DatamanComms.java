// ***************************************************************
//
// CLASS:		DatamanComms.java
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
//				Fri Dec 17 19:20:40 MST 2004
//				Added in the code at the end of db_connect to
//				automagically disconnect from the db server at
//				program termination.
//				tomg
//
//***************************************************************

//
// this class implements the communications between a client
// and the dataman server.
//

package Dataman;

import java.io.IOException;
import java.io.PrintStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.net.SocketAddress;
import java.net.UnknownHostException;
import java.nio.Buffer;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.channels.SocketChannel;
import java.nio.channels.UnresolvedAddressException;
import java.nio.charset.Charset;
import java.lang.Runtime;
import java.lang.Thread;


class DatamanComms implements Runnable {

	private static SocketChannel db_sock;
	private static boolean inited = false;
//
// this is called from either of the init routines.  it
// establishes communications to the named host.
//
    protected DatamanComms(String host) {
         if (!inited) 
              db_connect(host);
         return;
    }
//
// this is used to construct for all client routines.
//
    protected DatamanComms() {
         return;
    }
//
// establish the communications.
//
	private void db_connect(String host) {

		ByteBuffer header = ByteBuffer.allocateDirect(4);
		Charset cs = Charset.forName("UTF-8");
		try {
			InetSocketAddress sa = new InetSocketAddress(host , 8758);
			db_sock = SocketChannel.open(sa);
			Socket sock = db_sock.socket();
			sock.setTcpNoDelay(true);
// use christy's birthday as a 'verification' string that we can 'trust'
			db_sock.write(cs.encode("9-30-1966"));
			db_sock.read(header);
			header.flip();
// get the connection response. if it isn't 'ok' it's an error code
			String resp = new String(cs.decode(header).array());
			if (!resp.equals("ok")) {
				db_sock.close();
				DatamanErrVal e = new DatamanErrVal();
				throw new DatamanRuntimeException(e.getDBErr(Integer.parseInt(resp)));
			}
// kewlness!
			inited = true;
		} catch (UnknownHostException e) {
              System.out.println("Host " + host + " is unknown");
              e.printStackTrace();
		} catch (IOException e) {
              System.out.println("Dataman server is not running on '" + host + "'");
              e.printStackTrace();
		} catch (UnresolvedAddressException e) {
              System.out.println("Unresolved address for host " + host);
              e.printStackTrace();
		}
// set up the handler to close down the link on client termination.
		Runtime r = Runtime.getRuntime();
		r.addShutdownHook(new Thread(this));
	}
// this is the method required by the Runnable interface.
	public void run() {
		db_disconnect();
	}
//
// terminate the connection from the server.  this is only called at
// client system shutdown time.
//
	protected void db_disconnect() {

		String cmd;
		ByteBuffer buff;

		if (!inited)
			return;
//
//start by flushing out any records that might have been updated
//
		Dataman.workfile.out_rec();
		if (Dataman.master != null)
			Dataman.master.out_rec();

		try {
			Charset cs = Charset.forName("UTF-8");
			if (Dataman.is_sort) {
				cmd = DatamanFunc.ICLOSE + "|" + Dataman.cur_index.get_idxno() + "|";
				buff = db_send(cs.encode(cmd), cmd.length());
			}
// tell the server to close the workfile
			cmd = DatamanFunc.ICLOSE + "|" + -Dataman.workfile.getchan() + "|";
			buff = db_send(cs.encode(cmd), cmd.length());
// tell the server we are disconnecting.
			cmd = DatamanFunc.DISCON + "|";
			buff = db_send(cs.encode(cmd), cmd.length());
			db_sock.close();
		} catch (IOException e) {
// we don't care that we catch an exception... we're shutting down anyway!
		}
	}

//
// send a message to the server, then hang around and way for the
// response.  send a new bytebuffer back as the resp.
//
	protected ByteBuffer db_send(ByteBuffer msg ,int i)
		throws IOException
	{
		ByteBuffer resp;
		ByteBuffer header;

		if (!inited) {
			DatamanErrVal e = new DatamanErrVal();
			throw new DatamanRuntimeException(e.getDBErr(DatamanErrVal.EJAVACON));
		}
// get the packet 'wrapper' space and put the packet size in
		header = ByteBuffer.allocateDirect(4);
		header.putInt(i);
		header.flip();
		try {
// write the header, then the message.  since we're reading
// 'header', we will get at most 4 bytes from the socket.  this
// is the size of the response.
			db_sock.write(header);
			db_sock.write(msg);
			header.flip();
			header.clear();
			db_sock.read(header);
			header.flip();
			i = header.getInt();
//
// get a new byte buffer to return, then read the message
// changed to use allocate instead of allocateDirect, so that
// the resp buffer would have a backing array.
//
			resp = ByteBuffer.allocate(i);
			while (i > 0) {
				int val;
				if ((val = db_sock.read(resp)) < 0) {
					DatamanErrVal e = new DatamanErrVal();
					throw new DatamanRuntimeException(e.getDBErr(DatamanErrVal.EJAVAREAD));
				}
				i -= val;
			}
		}
		catch (IOException e) {
			db_sock.close();
			System.out.println("Caught IO exception: " + e.toString());
			throw e;
		}
// ok, we're done!
		resp.flip();
		return (resp);
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
