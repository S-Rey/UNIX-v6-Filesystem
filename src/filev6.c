#include "error.h"
#include "filev6.h"
#include "inode.h"
#include "sector.h"
#include <string.h>

int filev6_open(const struct unix_filesystem *u, uint16_t inr, struct filev6 *fv6)
{
	// test the pointer parameter
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(fv6);

	// initialize the u and offset fields
	fv6->u = u;
	fv6->offset = 0;
	fv6->i_number = inr;

	// load the inode
	int error = inode_read(u, inr, &(fv6->i_node));
	if (error) {
		debug_print("[--] filev6_open, cannot read inode\n", NULL);
		return error;
	}
	// if everything is OK, return 0
	debug_print("[OK] filev6_open\n", NULL);
	return 0;	
}


int filev6_readblock(struct filev6 *fv6, void *buf)
{
	// Test for NULL pointer
	M_REQUIRE_NON_NULL(fv6);
	M_REQUIRE_NON_NULL(buf);
	M_REQUIRE_NON_NULL(fv6->u);

	// Get the size of the file and the size of the offset 
	int32_t size = inode_getsize(&(fv6->i_node));
	int32_t offset_size = fv6->offset;// * SECTOR_SIZE;
	
	// if end of file
	if(offset_size >= size) {
		fv6->offset = 0;
		debug_print("[OK] filev6_readblock end of file", NULL);	
		return 0;
	}

	// If size of inode is less than zero => bad parameter
	if(size < 0) {
		debug_print("[--] filev6_readblock inode size negative", NULL);
		return ERR_BAD_PARAMETER;
	}

	// Load the offset of the sector
	int sect = inode_findsector(fv6->u, &(fv6->i_node), (fv6->offset)/SECTOR_SIZE);
	if (sect < 0 ) {
		debug_print("[--] filev6_readblock error searching offset", NULL);
		return sect;
	}

	int error = sector_read(fv6->u->f, (uint16_t)sect, buf);
	if (error) {
		debug_print("[--] filev6_readblock error sector_read", NULL);
		return error;	
	}
	
	fv6->offset += SECTOR_SIZE;
	
	
	return SECTOR_SIZE;
}

int filev6_lseek(struct filev6 *fv6, int32_t offset)
{
	M_REQUIRE_NON_NULL(fv6);
	// Check if the offset is bigger than the size of the inode
	if (offset > inode_getsize(&fv6->i_node) || offset < 0) {
		return ERR_OFFSET_OUT_OF_RANGE;
	}
	fv6->offset = offset;
	return 0;
}



int filev6_create(struct unix_filesystem *u, uint16_t mode, struct filev6 *fv6)
{
	// Test arguments
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(fv6);

	// Create and initialize an inode
	struct inode inode;
	memset(&inode, 0, sizeof(struct inode));
	inode.i_mode = mode;

	// write inode
	int return_code = inode_write(u, fv6->i_number, &inode);
	if (return_code != 0) {
		return  return_code;
	}

	//update filev6
	fv6->i_node = inode;

	return 0;
}



int filev6_writebytes(struct unix_filesystem *u, struct filev6 *fv6, void *buf, int len)
{
	printf("filev6_writebytes not implemented\n");
	return ERR_IO;
}
