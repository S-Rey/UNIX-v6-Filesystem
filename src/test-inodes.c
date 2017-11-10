#include "inode.h"
#include "filev6.h"

int test(struct unix_filesystem *u) 
{
	struct filev6 fv6;
	fv6.offset = 0;
	fv6.i_number = 4;
	fv6.u = u;
	uint16_t mode = IALLOC;

	int return_code = filev6_create(u, mode ,&fv6);
	if (return_code != 0) {
		return return_code;
	}

	inode_scan_print(u);
	return 0;
}
