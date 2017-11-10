#include <inttypes.h>
#include <stdlib.h>
#include "string.h"
#include "error.h"
#include "inode.h" 
#include "filev6.h"
#include "sha.h"
#include "sector.h"


int test_print_read_inode(struct filev6 *fs, char* data)
{
	// Test null pointer
	M_REQUIRE_NON_NULL(fs);
	M_REQUIRE_NON_NULL(fs->u);
	
	// load inode in fs->i_inode
	int error = inode_read(fs->u, fs->i_number, &(fs->i_node));
	if (error) {
		printf("filev6_open failed for inode #%" PRIu16 ".\n", fs->i_number);
		return error;
	}

	// print inode 
	printf("\nPrinting inode #%" PRIu16 "\n", fs->i_number);
	inode_print(&(fs->i_node));
	
	// test if inode i is a folder
	if (fs->i_node.i_mode & IFDIR) {
		printf("which is a directory\n");
	} else {
		// find sector of data for inode
		int sect = inode_findsector(fs->u, &(fs->i_node), fs->offset);
		if(sect < 0) {
			debug_print("[--] test-file inode_findsector %d\n", sect);
			return sect;
		}
		
		// read block of data
		int read_bytes = filev6_readblock(fs, data);
		if (read_bytes < 0) {
			debug_print("[--] test-file filev6_eradblock\n", NULL);
			return read_bytes;
		}
		printf("the first sector of data of which contains:\n");
		printf("%s\n", data);

	}
	return 0;
}


int test(struct unix_filesystem *u) 
{
	// create a file struct and fill it with random values
	struct filev6 fs;
  	memset(&fs, 255, sizeof(fs));

	// table containing the reading
	char data[SECTOR_SIZE + 1];
	data[SECTOR_SIZE] = '\0';

	// select inode #3
	uint16_t inr = 3;

	int error = filev6_open(u, inr, &fs);
	if (error) {
		debug_print("[--] test_file filev6_open\n",NULL);
		return error;
	}	
	test_print_read_inode(&fs, data);

	inr = 5;
	error = filev6_open(u, inr, &fs);
	if (error) {
		debug_print("[--] test_file filev6_open\n",NULL);
	}
	test_print_read_inode(&fs, data);
	
	printf("---\n\n");
	printf("Listing inodes SH:\n");

	struct inode sector[INODES_PER_SECTOR];

	for(uint16_t i = 0; i < u->s.s_isize; ++i) {
		// read the sector, and keep the error if any
		error = sector_read(u->f, (uint32_t)u->s.s_inode_start + i, sector);
		if (error != 0) {
			return error;
		}
		
		for (size_t j = 0; j < INODES_PER_SECTOR; ++j) {
			// print inode
			print_sha_inode(u, sector[j],(int)(i*INODES_PER_SECTOR + j));
			
		}
	}		


	return 0;
}
