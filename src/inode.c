#include <inttypes.h>
#include <string.h>
#include "inode.h"
#include "sector.h"
#include "error.h"

// Size above which the file is considered an extra large file
#define EXTRA_LARGE_FILE 7*256*SECTOR_SIZE

// Helper function used to print one inode
void inode_print_one(const struct inode *inode, size_t number)
{
	// test null pointer
	if (inode == NULL) {
		return;
	}

	// test if inode is allocated
	if (inode->i_mode & IALLOC) {
		// print the number of inode
		printf("inode\t%zd", number);
		// test if inode is a folder
		if(inode->i_mode & IFDIR) {
			printf(" (%s) ", SHORT_DIR_NAME);
		} else {
			printf(" (%s) ", SHORT_FIL_NAME);
		}
		printf("len   %hu\n", inode_getsize(inode));
	}
}

int inode_scan_print(const struct unix_filesystem *u)
{
	// test if the pointer are non null
	M_REQUIRE_NON_NULL(u);


	// create a table containing the sector
	struct inode sector[INODES_PER_SECTOR];

	for(uint16_t i = 0; i < u->s.s_isize; ++i) {
		// read the sector, and keep the error if any
		int error = sector_read(u->f,(uint32_t) u->s.s_inode_start + i, sector);
		if (error != 0) {
			return error;
		}

		for (size_t j = 0; j < INODES_PER_SECTOR; ++j) {
			// print inode
			inode_print_one(&(sector[j]), i*INODES_PER_SECTOR + j);
		}
	}
	return 0;
}



void inode_print (const struct inode *inode)
{
	printf("**********FS INODE START**********\n");
	// test if the inode is NULL
	if (inode == NULL) {
		printf("NULL ptr\n");
	} else {
		printf("i_mode: %" PRIu16 "\n", inode->i_mode);
		printf("i_nlink: %" PRIu8 "\n", inode->i_nlink);
		printf("i_uid: %" PRIu8 "\n", inode->i_uid);
		printf("i_gid: %" PRIu8 "\n", inode->i_uid);
		printf("i_size0: %" PRIu8 "\n", inode->i_size0);
		printf("i_size1: %" PRIu16 "\n", inode->i_size1);
		printf("size: %" PRIu16 "\n", inode->i_size1);

	}

	printf("**********FS INODE END**********\n");
}


int inode_read(const struct unix_filesystem *u, uint16_t inr, struct inode *inode)
{
	// test if unix filesystem and inode are not null
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(inode);

	// inode table to store the sector
	struct inode sector[INODES_PER_SECTOR];
	// inr divided INODES_PER_SECTOR to find the sector
	uint16_t n_sector = inr / INODES_PER_SECTOR;
	// inr modulo INODES_PER_SECTOR to find the inode
	uint16_t n_inode  = inr % INODES_PER_SECTOR;

	// if n_sector is bigger than the number of sector containing inodes
	// then return an error
	if(n_sector > u->s.s_isize) {//correcteur : also test id < ROOT_INUMBER
		return ERR_INODE_OUTOF_RANGE;
	}

	// load corresponding sector and check for error
	int error = sector_read(u->f, u->s.s_inode_start + n_sector, sector);
	if (error) {
		return error;
	}

	// check if inode used, otherwise send and error
	if (sector[n_inode].i_mode & IALLOC) {
		//debug_print("read_inode\n", NULL);
			*inode = sector[n_inode];
		return 0;
	} else {
		return ERR_UNALLOCATED_INODE;
	}
}



 int inode_findsector(const struct unix_filesystem *u, const struct inode *i,
	int32_t file_sec_off)
{
	// test if unix filesystem and inode are not null
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(i);

	// if the offset is not correct
	if (file_sec_off < 0) {
		debug_print(" ",ERR_BAD_PARAMETER);
		return ERR_BAD_PARAMETER;
	}

	// if the inode is not allocated
	if ((i->i_mode & IALLOC) == 0) {
		debug_print("error :%d", ERR_UNALLOCATED_INODE);
		return ERR_UNALLOCATED_INODE;
	}

	// retrive the size of the inode
	int32_t inode_size = inode_getsize(i);
	uint32_t n_sector_used = inode_size/ SECTOR_SIZE;

	// if inode size is negative
	if (inode_size < 0) { //Correcteur : is it possible?
		debug_print("error :%d", ERR_BAD_PARAMETER);
		return ERR_BAD_PARAMETER;
	}

	// if inode is bigger than disk
	if (inode_size > EXTRA_LARGE_FILE) {
		debug_print("error :%d", ERR_FILE_TOO_LARGE);
		return ERR_FILE_TOO_LARGE;
	}

	// if the size is lass than 8 sector, all i_addr are file
	if (inode_size < (8*SECTOR_SIZE)) {
		if (file_sec_off > (n_sector_used)) {
			debug_print("error offset:%d\n", ERR_OFFSET_OUT_OF_RANGE);
			return ERR_OFFSET_OUT_OF_RANGE;
		} else {
			debug_print("[OK] findsector smaller than 8\n", NULL);
			return i->i_addr[file_sec_off];
		}
	} else {

		// go throug the inode table until finding the good address
		uint16_t addr_sect = file_sec_off / ADDRESSES_PER_SECTOR;
		// test if addr_sect is not bigger than the table size
		if (addr_sect >= ADDR_SMALL_LENGTH) {
			return ERR_OFFSET_OUT_OF_RANGE;
		}
		uint16_t addr_sect_off = file_sec_off % ADDRESSES_PER_SECTOR;
		// the sector filles with address
		uint16_t sector[ADDRESSES_PER_SECTOR];

		// read the sector containing the other sector address
		int error = sector_read(u->f, (uint32_t)(i->i_addr[addr_sect]),sector);
		if(error) {
			debug_print("%d",error);
			return error;
		}
		// return the correspond addr stored in the sector of address
		debug_print("[OK] findsector bigger than 8\n", NULL);
		return sector[addr_sect_off];
	}
}



int inode_alloc(struct unix_filesystem *u)
{
	M_REQUIRE_NON_NULL(u);
	int next = bm_find_next(u->ibm);
	if (next < 0) {
		return ERR_NOMEM;
	}
	bm_set(u->ibm, (uint64_t)next);
	return next;
}


int inode_write(struct unix_filesystem *u, uint16_t inr, struct inode *inode)
{
	// test if unix filesystem and inode are not null
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(inode);


	// inode table to store the sector
	struct inode sector[INODES_PER_SECTOR];
	// inr divided INODES_PER_SECTOR to find the sector
	uint16_t n_sector = inr / INODES_PER_SECTOR;
	// inr modulo INODES_PER_SECTOR to find the inode
	uint16_t n_inode  = inr % INODES_PER_SECTOR;

	// if n_sector is bigger than the number of sector containing inodes
	// then return an error
	if(n_sector > u->s.s_isize) {//correcteur : also test id < ROOT_INUMBER
		return ERR_INODE_OUTOF_RANGE;
	}

	// load corresponding sector and check for error
	int error = sector_read(u->f, u->s.s_inode_start + n_sector, &sector);
	if (error) {
		return error;
	}

	// change the inode
	sector[n_inode] = *inode;
	return sector_write(u->f, u->s.s_inode_start + n_sector, &sector);
}




