/* Real socket I/O through the native client/server handshake helpers. */
#undef NDEBUG
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>
#include "protocol.h"

static void finish(pid_t pid)
{
	int status;
	assert(waitpid(pid, &status, 0) == pid);
	assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void client_case(const char *reply, int expected)
{
	int pair[2];
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
	pid_t pid = fork();
	assert(pid >= 0);
	if (!pid) {
		char hello[DM_PROTOCOL_SIZE];
		close(pair[0]);
		int64_t deadline = dm_protocol_now() + DM_PROTOCOL_TIMEOUT_MS;
		assert(dm_protocol_io(pair[1], hello, sizeof(hello), 0, deadline));
		assert(!memcmp(hello, DM_PROTOCOL_HELLO, sizeof(hello)));
		/* Fragment every reply to exercise short reads. */
		for (size_t i = 0; i < strlen(reply); i++) {
			if (send(pair[1], reply + i, 1, MSG_NOSIGNAL) != 1)
				break;
			usleep(1000);
		}
		close(pair[1]);
		_exit(0);
	}
	close(pair[1]);
	assert(dm_protocol_connect(pair[0]) == expected);
	close(pair[0]);
	finish(pid);
}

static void server_case(const char *hello, int accepted)
{
	int pair[2];
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
	pid_t pid = fork();
	assert(pid >= 0);
	if (!pid) {
		char reply[4];
		close(pair[0]);
		for (size_t i = 0; i < strlen(hello); i++) {
			assert(write(pair[1], hello + i, 1) == 1);
			usleep(1000);
		}
		if (accepted)
			assert(write(pair[1], "NEXT", 4) == 4);
		shutdown(pair[1], SHUT_WR);
		if (!accepted) {
			assert(dm_protocol_io(pair[1], reply, sizeof(reply), 0, dm_protocol_now() + 1000));
			assert(!memcmp(reply, "-39\n", sizeof(reply)));
		}
		close(pair[1]);
		_exit(0);
	}
	close(pair[1]);
	assert(dm_protocol_accept(pair[0]) == accepted);
	if (accepted) {
		char next[4];
		assert(dm_protocol_io(pair[0], next, sizeof(next), 0, dm_protocol_now() + 1000));
		assert(!memcmp(next, "NEXT", sizeof(next)));
	}
	close(pair[0]);
	finish(pid);
}

int main(void)
{
	client_case(DM_PROTOCOL_HELLO, 0);
	client_case("DMAN0002\n", EPROTOCOL);
	client_case("ok", EPROTOCOL);
	client_case("-39\n", EPROTOCOL);
	client_case("-3\n", ENOSHM);
	client_case("", EPROTOCOL);
	server_case(DM_PROTOCOL_HELLO, 1);
	server_case("DMAN0002\n", 0);
	server_case("9-30-1966", 0);
	server_case("DMAN", 0);
	/* A connected peer that never sends data must not block indefinitely. */
	int pair[2]; char byte;
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0);
	int64_t started = dm_protocol_now();
	assert(!dm_protocol_io(pair[0], &byte, 1, 0, started + 50));
	assert(dm_protocol_now() - started < 1000);
	close(pair[0]); close(pair[1]);
	return 0;
}
