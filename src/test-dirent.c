#include <stdlib.h>
#include "error.h"
#include "filev6.h"
#include "direntv6.h"




int test(struct unix_filesystem *u) 
{
	direntv6_print_tree(u, ROOT_INUMBER, "");
	return 0;
}
