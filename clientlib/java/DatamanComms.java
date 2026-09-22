package Dataman;

import java.io.IOException;
import java.io.InputStream;
import java.net.SocketTimeoutException;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.ByteBuffer;
import java.nio.channels.SocketChannel;
import java.nio.charset.StandardCharsets;

/** Package-private transport shared by the Java client classes. */
class DatamanComms implements Runnable {

	static final int PROTOCOL_VERSION = 1;
	static final String PROTOCOL_HELLO = "DMAN0001\n";
	private static final int HANDSHAKE_TIMEOUT_MS = 5000;

	private static final int MAX_RESPONSE_SIZE = 64 * 1024 * 1024;
	private static SocketChannel db_sock;
	private static boolean inited;
	private static boolean shutdownHookInstalled;

	protected DatamanComms(String host) {
		if (!inited)
			db_connect(host);
	}

	protected DatamanComms() {
	}

	private synchronized void db_connect(String host) {
		if (inited)
			return;

		try {
			db_sock = SocketChannel.open(new InetSocketAddress(host, 8758));
			Socket socket = db_sock.socket();
			socket.setTcpNoDelay(true);
			handshake(socket);
			inited = true;
		} catch (IOException | RuntimeException error) {
			closeSocket();
			if (error instanceof DatamanRuntimeException)
				throw (DatamanRuntimeException)error;
			DatamanErrVal errors = new DatamanErrVal();
			throw new DatamanRuntimeException(
				errors.getDBErr(DatamanErrVal.ENOCONN), error);
		}

		if (!shutdownHookInstalled) {
			Runtime.getRuntime().addShutdownHook(new Thread(this));
			shutdownHookInstalled = true;
		}
	}

	/** Exchange the wire version before sending initialization or database commands. */
	static void handshake(Socket socket) throws IOException {
		int savedTimeout = socket.getSoTimeout();
		long deadline = System.nanoTime() + HANDSHAKE_TIMEOUT_MS * 1_000_000L;
		try {
			socket.getOutputStream().write(PROTOCOL_HELLO.getBytes(StandardCharsets.US_ASCII));
			InputStream input = socket.getInputStream();
			for (int i = 0; i < PROTOCOL_HELLO.length(); i++) {
				long remaining = deadline - System.nanoTime();
				if (remaining <= 0)
					throw new SocketTimeoutException("Dataman protocol handshake timed out");
				socket.setSoTimeout((int)Math.max(1, remaining / 1_000_000L));
				if (input.read() != PROTOCOL_HELLO.charAt(i))
					throw new DatamanRuntimeException(
						"Incompatible Dataman protocol; update client and server together");
			}
		} finally {
			socket.setSoTimeout(savedTimeout);
		}
	}

	@Override
	public void run() {
		db_disconnect();
	}

	protected void db_disconnect() {
		if (!inited)
			return;

		try {
			if (Dataman.workfile != null)
				Dataman.workfile.out_rec();
			if (Dataman.master != null)
				Dataman.master.out_rec();

			if (Dataman.is_sort && Dataman.cur_index != null)
				sendText(DatamanFunc.ICLOSE + "|" +
					Dataman.cur_index.get_idxno() + "|");
			if (Dataman.workfile != null)
				sendText(DatamanFunc.ICLOSE + "|" +
					-Dataman.workfile.getchan() + "|");
			sendText(DatamanFunc.DISCON + "|");
		} catch (IOException | RuntimeException ignored) {
			/* Shutdown cleanup is best effort. */
		} finally {
			closeSocket();
		}
	}

	private void sendText(String command) throws IOException {
		db_send(StandardCharsets.UTF_8.encode(command),
			command.getBytes(StandardCharsets.UTF_8).length);
	}

	protected synchronized ByteBuffer db_send(ByteBuffer message, int length)
		throws IOException {
		if (!inited || db_sock == null) {
			DatamanErrVal errors = new DatamanErrVal();
			throw new DatamanRuntimeException(
				errors.getDBErr(DatamanErrVal.EJAVACON));
		}
		if (length < 0 || length != message.remaining())
			throw new IOException("invalid Dataman request length");

		try {
			ByteBuffer header = ByteBuffer.allocate(4);
			header.putInt(length);
			header.flip();
			writeFully(db_sock, header);
			writeFully(db_sock, message);

			header.clear();
			readFully(db_sock, header);
			header.flip();
			int responseLength = header.getInt();
			if (responseLength < 0 || responseLength > MAX_RESPONSE_SIZE)
				throw new IOException(
					"invalid Dataman response length " + responseLength);

			ByteBuffer response = ByteBuffer.allocate(responseLength);
			readFully(db_sock, response);
			response.flip();
			return response;
		} catch (IOException error) {
			closeSocket();
			throw error;
		}
	}

	private static void writeFully(SocketChannel socket, ByteBuffer buffer)
		throws IOException {
		while (buffer.hasRemaining()) {
			if (socket.write(buffer) < 0)
				throw new IOException("socket closed while writing");
		}
	}

	private static void readFully(SocketChannel socket, ByteBuffer buffer)
		throws IOException {
		while (buffer.hasRemaining()) {
			if (socket.read(buffer) < 0)
				throw new IOException("socket closed while reading");
		}
	}

	private static void closeSocket() {
		inited = false;
		if (db_sock == null)
			return;
		try {
			db_sock.close();
		} catch (IOException ignored) {
		} finally {
			db_sock = null;
		}
	}
}

/* vim: set noet sw=4 sts=4 ts=4 ft=java: */
