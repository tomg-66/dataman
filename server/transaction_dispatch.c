/* ***************************************************************
 *
 * PROCEDURE:	transaction_dispatch.c
 *
 * PROJECT:		dataman server side
 * 
 * DATE:		Thu Sep 17 08:17:11 PM MDT 2026
 * 
 * AUTHOR:		Tom Green
 * 
 * FILES:
 ************************************************************* */
/*
 * dataman transaction processing
 */
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * The GNU General Public License is contained in the file COPYING.
 */
#include "transaction_dispatch.h"
#include "session_root.h"
#include "errors.h"
#include <stdlib.h>
#include <string.h>

typedef struct dispatch_binding {
	int shmid;
	const dm_tx_target *files;
	dm_tx_target *targets;
	size_t count;
} dispatch_binding;

static int bind_owner(const char *root, void *context)
{
	dispatch_binding *binding = context;
	size_t length = strlen(root);

	for (size_t i = 0; i < binding->count; i++) {
		const char *path = binding->files[i].path;
		const char *relative;
		if (!path || path[0] != '/' || strncmp(path, root, length))
			return EINVMSG;
		if (length == 1)
			relative = path + 1;
		else {
			if (path[length] != '/')
				return EINVMSG;
			relative = path + length + 1;
		}
		const char *part = relative;
		for (;;) {
			size_t size = strcspn(part, "/");
			if (!size || (size == 1 && part[0] == '.') || (size == 2 && !strncmp(part, "..", 2)))
				return EINVMSG;
			if (!part[size])
				break;
			part += size + 1;
		}
		binding->targets[i].fd = binding->files[i].fd;
		binding->targets[i].path = relative;
	}
	/*
	 * Pin root metadata through owner admission, but release its mutex before
	 * calling the handler. Owner admission prevents commit/abort/disconnect.
	 */
	return dm_tx_enter(binding->shmid, binding->targets, binding->count);
}

int dm_tx_dispatch(int shmid, const dm_tx_target *files, size_t count,
		int (*handler)(void *), void *context)
{
	dispatch_binding binding = {shmid, files, NULL, count};
	int result;

	if (!handler || (count && !files) || count > SIZE_MAX / sizeof(dm_tx_target))
		return EINVMSG;
	if (count) {
		binding.targets = calloc(count, sizeof(*binding.targets));
		if (!binding.targets) {
			return ENOALLOC;
		}
	}
	result = session_root_use(shmid, bind_owner, &binding);
	if (!result) {
		result = handler(context);
		dm_tx_complete(result);
	}
	free(binding.targets);
	return result;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
