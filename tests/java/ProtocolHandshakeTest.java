package Dataman;

import java.net.ServerSocket;
import java.net.Socket;
import java.net.InetSocketAddress;
import java.nio.channels.SocketChannel;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.atomic.AtomicReference;

/** Standalone handshake test using a disposable loopback peer. */
public final class ProtocolHandshakeTest {
	private static void exercise(String reply, boolean accepted) throws Exception {
		try (ServerSocket listener = new ServerSocket(0, 1,
				java.net.InetAddress.getLoopbackAddress())) {
			AtomicReference<Throwable> failure = new AtomicReference<>();
			Thread peer = new Thread(() -> {
				try (Socket socket = listener.accept()) {
					socket.setSoTimeout(5000);
					byte[] hello = socket.getInputStream().readNBytes(9);
					if (!new String(hello, StandardCharsets.US_ASCII).equals("DMAN0001\n"))
						throw new AssertionError("wrong client greeting");
					for (byte value : reply.getBytes(StandardCharsets.US_ASCII)) {
						socket.getOutputStream().write(value);
						socket.getOutputStream().flush();
						Thread.sleep(2);
					}
				} catch (Throwable error) {
					failure.set(error);
				}
			});
			peer.start();
			try (Socket socket = SocketChannel.open(new InetSocketAddress(
					listener.getInetAddress(), listener.getLocalPort())).socket()) {
				boolean succeeded = false;
				try {
					DatamanComms.handshake(socket);
					succeeded = true;
				} catch (DatamanRuntimeException expected) {
					if (accepted) throw expected;
				}
				if (succeeded != accepted) throw new AssertionError("wrong handshake result");
				if (socket.getSoTimeout() != 0) throw new AssertionError("timeout not restored");
				// Keep the socket alive while the peer finishes a rejected reply.
				peer.join(5000);
				if (peer.isAlive()) throw new AssertionError("handshake peer hung");
				if (failure.get() != null) throw new AssertionError(failure.get());
			}
		}
	}

	public static void main(String[] args) throws Exception {
		exercise("DMAN0001\n", true);
		exercise("DMAN0002\n", false);
		exercise("ok", false);
		exercise("-39\n", false);
		exercise("", false);
		System.out.println("Java protocol handshake: PASS");
	}
}
