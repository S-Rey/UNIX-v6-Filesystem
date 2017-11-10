/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall hello.c `pkg-config fuse --cflags --libs` -o hello
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <inttypes.h>
#include "mount.h"
#include "direntv6.h"
#include "error.h"
#include "inode.h"

struct unix_filesystem fs = {0};

int is_mounted(struct unix_filesystem* u)
{
        if (u->f == NULL) {
                return 0;
        }
        return 1;
}


/**
 * @brief       fill the inode passed as parameters
 * @param entry
 * @param i
 * @return error if any, 0 if OK
 */
int fill_inode(const char* entry, struct inode* i, uint16_t * inr) {
        M_REQUIRE_NON_NULL(i);
        M_REQUIRE_NON_NULL(inr);

	debug_print("fill_inode path %s\n", entry);

        if(is_mounted(&fs)) {
                // Find inode number
                int i_numb = direntv6_dirlookup(&fs, ROOT_INUMBER, entry);

                if (i_numb < 0) {
                        return i_numb;
                }

                int return_code = inode_read(&fs, (uint16_t)i_numb, i);
                if (return_code < 0) {
                        return return_code;
                }
                *inr = (uint16_t)i_numb;
		debug_print("[OK] fill_inode inr: #%" PRIu16 "\n",*inr);
                return 0;
        }
        return ERR_IO;
}




/* From https://github.com/libfuse/libfuse/wiki/Option-Parsing.
 * This will look up into the args to search for the name of the FS.
 */
static int arg_parse(void *data, const char *filename, int key, struct fuse_args *outargs)
{
	(void) data;
	(void) outargs;
	if (key == FUSE_OPT_KEY_NONOPT && fs.f == NULL && filename != NULL) {

		// Mount fs
               	int error = mountv6(filename, &fs);
		if (error) {
			puts(ERR_MESSAGES[error - ERR_FIRST]);
			exit(1);
		}
		debug_print("[OK] mount is done\n", NULL);


		return 0;
	}
	return 1;
}


static int fs_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

       	uint16_t inr;
        struct inode i;
	debug_print("get arguments path %s\n", path);

        res = fill_inode(path, &i, &inr);
        if(res) {
		debug_print("[--] get arguments %s\n", path);
                return res;
        }

	memset(stbuf, 0, sizeof(struct stat));
	stbuf->st_mode = S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
	if ((i.i_mode & IFMT) == IFDIR) {
		stbuf->st_mode |= S_IFDIR;
	} else {
		stbuf->st_mode |= S_IFREG;
	}
	stbuf->st_nlink = i.i_nlink;
	stbuf->st_uid = i.i_uid;
	stbuf->st_gid = i.i_gid;
	stbuf->st_size = i.i_size1;
	inode_print(&i);
	debug_print("[OK] get arguments %s\n", path);
	return res;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		      off_t offset, struct fuse_file_info *fi)
{
	// Test parameters
	M_REQUIRE_NON_NULL(path);
	M_REQUIRE_NON_NULL(buf);
	M_REQUIRE_NON_NULL(fi);

	int res = 0;
	(void) offset;
	(void) fi;
	uint16_t inr;
	struct inode i;
	struct directory_reader d;
	char name[DIRENT_MAXLEN + 1];

	// read the inode number given the path
	res = fill_inode(path, &i, &inr);//Correcteur : dirlookup was enough
	if (res) {
		debug_print("[--] readdir fill_inode %s\n", path);
		return  res;
	}

	// Open the directory to read it
	res = direntv6_opendir(&fs, inr, &d);
	if (res) {
		debug_print("[--] readdir opendir %s\n", path);
		return res;
	}

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

        // list all file in the folder
        do {
		res = direntv6_readdir(&d, name, &inr);
		if (res == 1) {
			name[DIRENT_MAXLEN] = '\0';
			filler(buf, name, NULL, 0);//correcteur : check return value
		} else if (res == 0) {
			return 0;
		} else if (res == ERR_UNALLOCATED_INODE) {
			return 0;
		}

	} while (res == 1);
	debug_print("[OK] readdir %s\n", path);
        return res;
}

static int fs_open(const char *path, struct fuse_file_info *fi)
{
	return 0;
}




static int fs_read(const char *path, char *buf, size_t size, off_t offset,
		   struct fuse_file_info *fi)
{
	(void) fi;
	M_REQUIRE_NON_NULL(path);
	M_REQUIRE_NON_NULL(buf);
        M_REQUIRE_NON_NULL(fi);

	// The number of sectors that can be read
	size_t to_read_sector = size / SECTOR_SIZE;

	if (is_mounted(&fs)) {
		struct inode i;
		uint16_t inr;

		memset(buf, '\0', size * sizeof(char));

		// Get the inr of inode
		int error = fill_inode(path, &i, &inr);//correcteur : dirlookup was enough
		if (error) {
			return error;
		}

		struct filev6 fv6;
		memset(&fv6, 255, sizeof(struct filev6));

		// Open the filev6
		error = filev6_open(&fs, inr, &fv6);
		if (error) {
			return error;
		}

		// Adjust the offset of fv6
		error = filev6_lseek(&fv6, (int32_t)offset);
                if (error) {
			return 0;
			return error;
		}

		size_t j = 0;
		int return_code;
		char data[SECTOR_SIZE+1];
		// Read the data until buf size or end of file
		do {
			memset(data, '\0', (SECTOR_SIZE+1) * sizeof(char));
			return_code = filev6_readblock(&fv6, data);//orrecteur : coherent with your readblock but please refactor
			if (return_code == SECTOR_SIZE) {
				memcpy(&buf[j * SECTOR_SIZE], data, strlen(data)+1);
				++j;
			}
		}
		while(j < to_read_sector && return_code == SECTOR_SIZE);



		// End of file
		if (return_code == 0) {
			return (int)(j * SECTOR_SIZE);
		} else if (return_code == SECTOR_SIZE) {
			return (int)(j * SECTOR_SIZE);
		} else {
			return  return_code;
		}
	}

	return (int)size;
}

static struct fuse_operations available_ops = {
	.getattr	= fs_getattr,
	.readdir	= fs_readdir,
	.open		= fs_open,
	.read		= fs_read,
};

int main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	int ret = fuse_opt_parse(&args, NULL, NULL, arg_parse);
	if (ret == 0) {
		ret = fuse_main(args.argc, args.argv, &available_ops, NULL);
		(void)umountv6(&fs);
	}
	return ret;
}
