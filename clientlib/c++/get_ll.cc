/*
 * this is like get_long only for long longs.  Switching to 64 bit addressing
 * for dataman will require this
 */

#ifdef __gnu_linux__
#include <byteswap.h>
#include <endian.h>
#else
#include "misc.h"
#endif
#include <stdint.h>

int64_t get_ll(const void *ptr)
{

	int64_t tmp;
	tmp = *(int64_t *)ptr;

#ifdef __gnu_linux__
	if (__BYTE_ORDER == __LITTLE_ENDIAN)
		return(bswap_64(tmp));
#else
	if (d_endian == LITTLE_ENDIAN)
		return((((tmp) & 0xff00000000000000ull) >> 56)	\
      		| (((tmp) & 0x00ff000000000000ull) >> 40)		\
      		| (((tmp) & 0x0000ff0000000000ull) >> 24)		\
      		| (((tmp) & 0x000000ff00000000ull) >> 8)		\
      		| (((tmp) & 0x00000000ff000000ull) << 8)		\
      		| (((tmp) & 0x0000000000ff0000ull) << 24)		\
      		| (((tmp) & 0x000000000000ff00ull) << 40)		\
      		| (((tmp) & 0x00000000000000ffull) << 56));
#endif
	else
		return(*(int64_t *)ptr);
}

int64_t get_ll(const char *ptr)
{
	return(get_ll((const void *)ptr));
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim: set noet sw=4 sts=4 ts=4 fdm=marker:
 */
