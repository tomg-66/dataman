#include "datamanError.hpp"

#define DBERROR
#include "../../server/errors.h"

using namespace Dataman;

/*
 * dataman error is facing the user.  it is a catchable exception
 * that the user can receive outside of the normal true/false
 * functions that mutate the database.
 */
datamanError makeError(int code, const char *format, ...)
{
	char message[512];
	char tmp_str[512] = {0};

	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	if (code < 0 && code >= MAXERROR) {
		sprintf(tmp_str, ": %s", db_err_strings[-code]);
	} else if (errno) {
		sprintf(tmp_str, ": %s", strerror(errno));
	}

		// make sure to reserve a null!
	strncat(message, tmp_str, 511 - strlen(message));
	return datamanError(code, message);
}

/*
 * this is just the normal message printing function. it's so
 * that the message can get be displayed, but normal operation
 * can continue
 */
void db_err(int code, const char *fmt, ...)
{
	va_list pt;
	va_start(pt, fmt);
	vfprintf(stderr, fmt, pt);
	va_end(pt);

	if (code < 0 && code >= MAXERROR)
		fprintf(stderr, ": %s\n", db_err_strings[-code]);
	else {
		if (errno != 0)
			perror("");
	}
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
