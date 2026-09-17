/* Root registration without a running service. GPL-2.0-or-later. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>

#define CHECK(expr) do { if (!(expr)) { \
	fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); exit(1); \
} } while (0)

static int live_id = 100, work_result = 12, work_calls;
static int index_result = 12, index_calls;
static int attached = 1, abort_result, abort_calls, dead;
static int fake_kill(pid_t pid, int signal)
{
	CHECK(pid == 123 && signal == 0);
	if (dead) { errno = ESRCH; return -1; }
	return 0;
}
int dm_tx_disconnect(int shmid)
{
	(void)shmid; ++abort_calls; return abort_result;
}
static int fake_shmget(key_t key, size_t size, int flags)
{
	(void)size; (void)flags;
	if (key != 123 || live_id < 0) { errno = ENOENT; return -1; }
	return live_id;
}
static int fake_shmctl(int id, int cmd, struct shmid_ds *status)
{
	memset(status, 0, sizeof(*status));
	status->shm_nattch = attached;
	CHECK(cmd == IPC_STAT);
	if (id != live_id) { errno = EINVAL; return -1; }
	return 0;
}

#define shmget fake_shmget
#define shmctl fake_shmctl
#define kill fake_kill
#include "../session_root.c"
#undef kill
#undef shmget
#undef shmctl

int init_dataman(char *cmd, int offset, char **data)
{
	(void)offset;
	++work_calls;
	if (work_result > 0) { strcpy(cmd, "4|1|4|1|0|0|0|"); *data = strdup("work"); }
	return work_result;
}

int mkidx(char *cmd, int offset, char **data)
{
	CHECK(strstr(cmd + offset, "20|test-index|") == cmd + offset);
	++index_calls;
	if (index_result > 0) { strcpy(cmd, "4|0|1|1|0|0|0|"); *data = strdup("work"); }
	return index_result;
}

static int request(const char *payload, int work)
{
	char cmd[1024];
	char *data = NULL;
	int offset = snprintf(cmd, sizeof(cmd), "123|%d|", work == 2 ? 12 : work ? 10 : 24);
	CHECK(snprintf(cmd + offset, sizeof(cmd) - offset, "%s", payload) < (int)sizeof(cmd) - offset);
	int result = work == 2 ? mkidx_session(cmd, offset, &data) :
		work ? init_session(cmd, offset, &data) : def_root(cmd, offset, &data);
	if (result > 0 && !work) CHECK(result == 4 && !strcmp(cmd, "0|1|") && !data);
	if (result > 0 && work) CHECK(data && !strcmp(data, "work"));
	free(data);
	return result;
}

int main(void)
{
	char root[] = "/tmp/dataman-root-test-XXXXXX", other[] = "/tmp/dataman-other-test-XXXXXX";
	char payload[512], *copy;
	CHECK(mkdtemp(root) && mkdtemp(other));
	snprintf(payload, sizeof(payload), "%s|", root);
	CHECK(request(payload, 0) == 4);
	copy = session_root_copy(live_id); CHECK(copy && !strcmp(copy, root)); free(copy);
	CHECK(request(payload, 0) == 4);
	snprintf(payload, sizeof(payload), "%s/./|", root);
	CHECK(request(payload, 0) == 4);
	snprintf(payload, sizeof(payload), "%s|", other);
	CHECK(request(payload, 0) == EINVMSG);
	snprintf(payload, sizeof(payload), "%s/files/work|", other);
	CHECK(request(payload, 1) == EINVMSG && work_calls == 0);
	snprintf(payload, sizeof(payload), "%s/files/work|", root);
	CHECK(request(payload, 1) == work_result && work_calls == 1);
	CHECK(request("|", 0) == EINVMSG);
	CHECK(request("/missing-dataman-test-root|", 0) == ENOFILE);
	CHECK(request("/tmp|extra|", 0) == EINVMSG);
	CHECK(request("/tmp", 0) == EINVMSG);

	/* A new IPC generation must not inherit metadata for a reused PID. */
	live_id = 101;
	CHECK(session_root_copy(100) == NULL);
	work_result = ENOWFILE;
	snprintf(payload, sizeof(payload), "%s/files/work|", other);
	CHECK(request(payload, 1) == ENOWFILE);
	CHECK(session_root_copy(live_id) == NULL);
	work_result = 12;
	CHECK(request(payload, 1) == 12);
	copy = session_root_copy(live_id); CHECK(copy && !strcmp(copy, other)); free(copy);
	CHECK(request("/tmp/not-files/work|", 1) == EINVMSG);

	/* An index builder may start a fresh connection directly with MKIDX. */
	live_id = 102;
	snprintf(payload, sizeof(payload), "20|test-index|%s|1|work|", root);
	index_result = EIDXCREAT;
	CHECK(request(payload, 2) == EIDXCREAT && index_calls == 1);
	CHECK(session_root_copy(live_id) == NULL);
	index_result = 12;
	CHECK(request(payload, 2) == 12 && index_calls == 2);
	copy = session_root_copy(live_id); CHECK(copy && !strcmp(copy, root)); free(copy);
	snprintf(payload, sizeof(payload), "20|test-index|%s|1|work|", other);
	CHECK(request(payload, 2) == EINVMSG && index_calls == 2);
	CHECK(request("20|test-index||1|work|", 2) == EINVMSG);
	CHECK(request("20|test-index", 2) == EINVMSG);
	/* Explicit close aborts before forgetting metadata; failed abort retains it. */
	abort_result = EROLLBACK;
	CHECK(session_root_close(live_id) == EROLLBACK);
	copy = session_root_copy(live_id); CHECK(copy); free(copy);
	abort_result = 0;
	CHECK(session_root_close(live_id) == 0);
	CHECK(session_root_copy(live_id) == NULL);
	snprintf(payload, sizeof(payload), "%s|", root);
	CHECK(request(payload, 0) == 4);
	int before = abort_calls;
	attached = 0;
	CHECK(session_root_reap() == 0 && abort_calls == before + 1);
	CHECK(session_root_copy(live_id) == NULL);
	attached = 1;
	CHECK(request(payload, 0) == 4);
	before = abort_calls; dead = 1;
	CHECK(session_root_reap() == 0 && abort_calls == before + 1);
	dead = 0;
	/* Neither command works without an established connection IPC segment. */
	live_id = -1;
	CHECK(request(payload, 1) == ENOSHM);
	CHECK(session_root_copy(102) == NULL);
	CHECK(rmdir(root) == 0 && rmdir(other) == 0);
	return 0;
}
