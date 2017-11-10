
#include <inttypes.h>
#include <string.h>
#include "mount.h"
#include "sector.h"
#include "error.h"
#include "bmblock.h"
#include "inode.h"


/**
 * Fill ibm and fbm of a unix_filesystem struct
 * @param OUT u
 */
void fill_ibm(struct unix_filesystem * u)
{
	// Test if u is NULL
	if (!u) {
		return;;
	}

	// create a table containing the sector
	struct inode sector[INODES_PER_SECTOR];

	for(uint16_t i = 0; i < u->s.s_isize; ++i) {
		// read the sector, and keep the error if any
		int error = sector_read(u->f,(uint32_t) u->s.s_inode_start + i, sector);
		if (error != 0) {
			return;
		}

		for (size_t j = 0; j < 16; ++j) {
			// If current inode is allocated
			if(sector[j].i_mode & IALLOC) {
				bm_set(u->ibm, i*INODES_PER_SECTOR + j);
			}
		}
	}
	return;
}


void fill_fbm(struct unix_filesystem *u)
{
	// Test if u is NULL
	if (!u) {
		return;
	}


	// create a table containing the sector
	struct inode sector[INODES_PER_SECTOR];

	for(uint16_t i = 0; i < u->s.s_isize; ++i) {
		// read the sector, and keep the error if any
		int error = sector_read(u->f,(uint32_t) u->s.s_inode_start + i, sector);
		if (error != 0) {
			return;
		}

		for (size_t j = 0; j < 16; ++j) {
			// If current inode is allocated
			if(sector[j].i_mode & IALLOC) {
				int index = 0, n_sect = 0;
				do {
					n_sect = inode_findsector(u, &(sector[j]), index);
					if (n_sect > 0) {
						bm_set(u->fbm, (uint64_t) n_sect);
					}
					++ index;
				} while (n_sect > 0);
			}
		}
	}

}


int mountv6(const char *filename, struct unix_filesystem *u)
{
	// Test arguments
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(filename);

	// Set to 0 all the unix_filesystem structure
	memset(u, 0, sizeof(*u));
	u->fbm = NULL;
	u->ibm = NULL;

	// open the file u->f
	u->f = fopen(filename, "r+");
	if(u->f == NULL) {
		return ERR_IO;
	}

	// boot_sector will recieve the sector 512 bytes
	uint8_t boot_sector[SECTOR_SIZE];
	// load the first sector into boot_sector and return the error if any
	int error = sector_read(u->f, BOOTBLOCK_SECTOR, boot_sector);
	if( error != 0) {
		return error;
	}

	// test if the magic number has the right value
	if(boot_sector[BOOTBLOCK_MAGIC_NUM_OFFSET] != BOOTBLOCK_MAGIC_NUM) {
		return ERR_BADBOOTSECTOR;
	}

	// read the superblock
	error = sector_read(u->f, SUPERBLOCK_SECTOR, &(u->s));
	if (error) {
		return error;
	}

	// allocate ibm and fbm
	u->ibm = bm_alloc(u->s.s_inode_start,
			  (uint64_t)(u->s.s_isize * INODES_PER_SECTOR) - 1);
	u->fbm = bm_alloc(u->s.s_block_start + 1, u->s.s_fsize - 1);

	// if the allocation failed
	if (u->ibm == NULL ||
	    u->fbm == NULL) {
		return ERR_IO;
	}

	fill_ibm(u);
	fill_fbm(u);

	// return sector_read code
	return error;
}


void mountv6_print_superblock(const struct unix_filesystem *u)
{
	// if pointer is NULL
	if(u == NULL) {
		return;
	}

	printf("\n**********FS SUPERBLOCK START**********\n");

	printf("s_isize\t\t\t: %" PRIu16 "\n", u->s.s_isize);
	printf("s_fsize\t\t\t: %" PRIu16 "\n", u->s.s_fsize);
	printf("s_fbmsize\t\t: %" PRIu16 "\n", u->s.s_fbmsize);
	printf("s_ibmsize\t\t: %" PRIu16 "\n", u->s.s_ibmsize);
	printf("s_inonde_start\t\t: %" PRIu16 "\n", u->s.s_inode_start);
	printf("s_block_start\t\t: %" PRIu16 "\n", u->s.s_block_start);
	printf("s_fbm_start\t\t: %" PRIu16 "\n", u->s.s_fbm_start);
	printf("s_ibm_start\t\t: %" PRIu16 "\n", u->s.s_ibm_start);

	printf("s_flock\t\t\t: %" PRIu8 "\n", u->s.s_flock);
	printf("s_ilock\t\t\t: %" PRIu8 "\n", u->s.s_ilock);
	printf("s_fmod\t\t\t: %" PRIu8 "\n", u->s.s_fmod);
	printf("s_ronly\t\t\t: %" PRIu8 "\n", u->s.s_ronly);
	printf("s_time\t\t\t: [");
	printf("%" PRIu16 "", u->s.s_time[0]);
	printf("] %" PRIu16 "\n", u->s.s_time[1]);

	printf("**********FS SUPERBLOCK END***********\n");
}

int umountv6(struct unix_filesystem *u)
{
	// Test arguments
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(u->f);

	// if error during closing return ERR_IO, else 0
	int error = fclose(u->f);
	if (error) {
		return ERR_IO;
	}

	// free fbm and ibm
	free(u->fbm);
	free(u->ibm);

	u->fbm = NULL;
	u->ibm = NULL;

	return 0;
}



int mountv6_mkfs(const char *filename, uint16_t num_blocks, uint16_t num_inodes)
{
	// Test argument
	M_REQUIRE_NON_NULL(filename);

	// Create superblock and initilaize its values
	struct superblock s = {};
	s.s_isize = num_inodes / INODES_PER_SECTOR;
	s.s_fsize = num_blocks;
	if (s.s_fsize < (s.s_isize + num_inodes)) {
		return ERR_NOT_ENOUGH_BLOCS;
	}
	s.s_inode_start = SUPERBLOCK_SECTOR + 1;
	s.s_block_start = s.s_inode_start + s.s_isize;

	// Open file on disk and test if f has NULL value
	FILE* f = fopen(filename ,"w");
	if (!f) {
		return ERR_IO;
	}

	// create boot_sector with its magick number
	uint8_t boot_sector[SECTOR_SIZE] = {};
	boot_sector[BOOTBLOCK_MAGIC_NUM_OFFSET] = BOOTBLOCK_MAGIC_NUM;

	int return_code = fwrite(&boot_sector, SECTOR_SIZE, 1, f);
	if (return_code != 1) {
		return ERR_IO;
	}

	return_code = fwrite(&s, SECTOR_SIZE, 1, f);
	if (return_code != 1) {
		return  ERR_IO;
	}


	struct inode inodes[INODES_PER_SECTOR] = {};
	inodes[ROOT_INUMBER].i_mode = IALLOC | IFDIR;

	return_code = fwrite(&inodes, SECTOR_SIZE, 1, f);
	size_t index = 0;
	size_t tot_inodes = s.s_inode_start + s.s_block_start;
	// number of sector needed (counting inonde 0 and root inode => +2)
	size_t n_inode_sector = (tot_inodes+2) / INODES_PER_SECTOR;

	memset(&inodes, 0, SECTOR_SIZE);
	while (return_code == 1 && index < n_inode_sector) {
		return_code = fwrite(inodes, SECTOR_SIZE, 1, f);
		++ index;
	}

	fclose(f);
	f = NULL;

	return 0;
}




