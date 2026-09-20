/* Exercise the production frame parser without a running server or sockets. */
#undef NDEBUG
#include "../serial_service.c"
#include <assert.h>

static void check(const char *frame, int length, int expected, int command,
        int prefix, int payload)
{
    int channel[2], cmd = -99, used = -1, bytes = -1;
    char buffer[MAXSIZ + 1];
    assert(pipe(channel) == 0);
    assert(write(channel[1], frame, length) == length);
    assert(close(channel[1]) == 0);
    assert(read_command_prefix(channel[0], buffer, length, &cmd, &used, &bytes) == expected);
    if (expected >= 0) {
        assert(cmd == command && used == prefix);
        if (expected == 1) assert(bytes == payload);
    }
    assert(close(channel[0]) == 0);
}

int main(void)
{
    check("24", 2, 0, DISCON, 2, 0);
    check("24|", 3, 0, DISCON, 3, 0);
    check("1|", 2, 0, GET_FIRST, 2, 0);
    check("20|7|", 5, 0, ICLOSE, 3, 0);
    check("20|-1|", 6, 0, ICLOSE, 3, 0);
    check("-1|", 3, 0, START_XACT, 3, 0);
    check("20", 2, -1, 0, 0, 0);
    check("-1", 2, -1, 0, 0, 0);
    check("24x", 3, -1, 0, 0, 0);
    check("23|0|0|32|1|3|abc", 17, 1, FLUSH, 14, 3);
    check("23|0|0|32|1|4|abc", 17, -1, 0, 0, 0);
    return 0;
}
