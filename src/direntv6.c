#include "direntv6.h"
#include "inode.h"
#include "error.h"
#include "string.h"
#include <inttypes.h>


#define MAXPATHLEN_UV6 1024

int direntv6_opendir(const struct unix_filesystem *u, uint16_t inr,
	struct directory_reader *d)
{
	// Test the pointer
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(d);

	// initialize fv6
	struct filev6 fv6; // struct on which the func will work in case of error
	int error = filev6_open(u, inr, &fv6);
	if (error) {
		debug_print("[--] direntv6_opendir filev6_open\n", NULL);
		return error;
	}

	// if the inode is not a directory
	if ((fv6.i_node.i_mode & IFMT) != IFDIR) {
		debug_print("[--] direntv6_opendir i_node.i_mode\n", NULL);
		return ERR_INVALID_DIRECTORY_INODE;
	}

	// cur and last are initialized to zero
	d->cur 	= 0;
	d->last = 0;

	// copy fv6 in d->fv6
	d->fv6.offset = fv6.offset;
	d->fv6.i_node = fv6.i_node;
	d->fv6.i_number = fv6.i_number;
	d->fv6.u = fv6.u;

	debug_print("[OK] direntv6_opendir\n", NULL);
	return 0;
}


int direntv6_readdir(struct directory_reader *d, char *name, uint16_t *child_inr)
{
	// Test the pointers
	M_REQUIRE_NON_NULL(d);
	M_REQUIRE_NON_NULL(name);


	// check if error of parameter
	if (d->cur > d->last
		|| d->cur < 0
		|| d->last < 0
		)
	{
		debug_print("[--] direntv6_readdir d->cur > d->last\n", NULL);
		return ERR_BAD_PARAMETER;
	}


	// need to reload a block
	if (d->cur == d->last) {
		struct direntv6 dirs[DIRENTRIES_PER_SECTOR];
		int read_code = filev6_readblock(&(d->fv6), &dirs);
		if (read_code < 0) {
			debug_print("[--] direntv6_readdir filev6_readblock\n",
			NULL);
			return read_code;//Correcteur : you should return the number of blocks which is not what readblock returns
		} else if (read_code == 0) {
			debug_print("[OK] direntv6_readdir end of file\n", NULL);
			d->cur = 0;
			d->last = 0;
			return 0;
		}
		memcpy(&(d->dirs), dirs, sizeof(struct direntv6) * DIRENTRIES_PER_SECTOR);
		d->last += DIRENTRIES_PER_SECTOR;//correcteur : coherent with your readblock but please refactor
	}

	// If the inode is unallocated
	if( d->dirs[d->cur % DIRENTRIES_PER_SECTOR].d_inumber == 0) {
		return ERR_UNALLOCATED_INODE;
	}

	// copy the value to name table and check if error
	if(strncpy(name, d->dirs[d->cur % DIRENTRIES_PER_SECTOR].d_name, DIRENT_MAXLEN)
		== NULL)//correcteur : put a \0 at the end
	{
		debug_print("[--] direntv6_readdir strncpy NULL\n", NULL);
		return ERR_BAD_PARAMETER;
	}
	name[DIRENT_MAXLEN] = '\0';

	// Initialize the last value of the table name
	*child_inr = d->dirs[d->cur % DIRENTRIES_PER_SECTOR].d_inumber;

	d->cur += 1;

	debug_print("[OK] direntv6_readdir\n", NULL);
	return 1;
}


int direntv6_print_tree(const struct unix_filesystem *u, uint16_t inr, const char *prefix)
{
	// Test Pointers
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(prefix);
	if (inr == 0) {
		return ERR_BAD_PARAMETER;
	}

	struct directory_reader d;


	/**
	 * Try to open a directory to this inode.
	 * if unsuccessful, check if it is because it is a file or just a simple
	 * error
	 */
	int error = direntv6_opendir(u, inr, &d);
	if (error) {
		printf("%s %s\n", SHORT_FIL_NAME, prefix);
		return 0;
	}



	// readdir
	int return_code = 0;
	char name[DIRENT_MAXLEN + 1];
	char path[MAXPATHLEN_UV6];

	size_t len = strlen(prefix);
	printf("%s ", SHORT_DIR_NAME);
	printf("%s/\n", prefix);

	do {
		uint16_t copy_inr = inr;

		return_code = direntv6_readdir(&d, name, &inr);

		if (return_code == 1) {
			// concatenate prefix and name in path
			snprintf(path, MAXPATHLEN_UV6, "%s/%s", prefix, name);
			direntv6_print_tree(u, inr, path);
		} else if (return_code == 0) {
			return 0;
		} else if (return_code != ERR_UNALLOCATED_INODE) {
			debug_print("[--] direntv6_print_tree readdir\n", NULL);
			return return_code;
		}

		inr = copy_inr; // restore value of inr
	} while (return_code == 1);
	return 0;
}


int is_directory(const char* entry, size_t position)
{
	M_REQUIRE_NON_NULL(entry);
	// If there is not '/' after position
	if(pos_next_entry_name(entry, &position) == 1) {
		return 1;
	}
	return 0;
}

/**
 * @brief	compare the name of a file to the entry path
 * @param name
 * @param entry
 * @param position
 * @return	1 if same and end of entry (file), 0 if same (directory), -1 if different
 */
int entry_name_cmpr(const char* name, const char* entry, size_t position)
{
	size_t entry_length = strlen(entry);
	size_t name_lentgth = strlen(name);

	// Is not the same
	if (name_lentgth > strlen(entry + (char)position)) {
		return -1;
	}

	size_t i = 0;
	while ((i + position) < entry_length && i < name_lentgth) {

                if(entry[i + position] != name[i]) {
			return -1;
		}
		++i;
	}

	// Same length and equal => it isn't a folder
	if(strlen(entry + (char)position) == name_lentgth) {
		return 1;
	} else if (entry[position+i] == '/') {
		return 0;
	}
	return -1;
}


/**
 * @brief	scan until it finds the begining of a new entry name
 * @param entry the entry name
 * @param position from where it begins to scan
 * @return 1 if OK, 0 if end of entry, < 0 if error
 */
int pos_next_entry_name(const char* entry, size_t* position)
{
	// Check the paramters
	M_REQUIRE_NON_NULL(entry);
	M_REQUIRE_NON_NULL(position);
        size_t entry_length = strlen(entry);

	// align the position to the first '/'
        while (entry[*position] != '/' && (entry_length > *position)) {
		++(*position);
	}

	// align the position to the first entry name
	while (entry[*position] == '/' && (entry_length > *position)) {
		++(*position);
	}

	if(entry_length == *position) {
		return 0;
	} else {
		return 1;
	}
}

int direntv6_dirlookup_core(const struct unix_filesystem *u,uint16_t inr,
	const char *entry, size_t position)
{
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(entry);
	if (inr == 0) {
		return ERR_BAD_PARAMETER;
	}

	struct directory_reader d;

	int error = direntv6_opendir(u, inr, &d);
	if (error){
		return error;
	}

	int return_code = 0;
        int r_inode = 0;
	char name[DIRENT_MAXLEN + 1];
	do {
		uint16_t copy_inr = inr; // save inr value in case of error
		return_code = direntv6_readdir(&d, name, &inr);
                if (return_code == 1) {
			// if same name and folder
			if(entry_name_cmpr(name, entry, position) == 0) {
				pos_next_entry_name(entry, &position);
				r_inode = direntv6_dirlookup_core(u, inr, entry, position);
				if (r_inode > 0) {
					return r_inode;
				}
			} else if (entry_name_cmpr(name, entry, position) == 1) {
				return inr;
			}
		} else {
			inr = copy_inr; // restore value of inr
		}
	} while (return_code == 1);

	return ERR_INODE_OUTOF_RANGE;
}


int direntv6_dirlookup(const struct unix_filesystem *u, uint16_t inr,
                       const char *entry)
{
	// Test the arguments
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(entry);
	if(inr == 0) {
		return ERR_BAD_PARAMETER;
	}

	size_t position = 0;
        int return_code = 1;
	// check if need to align position
        if (entry[0] == '/') {
		return_code = pos_next_entry_name(entry, &position);//correcteur : do that only once in lookup core
	}
	if(return_code == 0) {
		return inr;
	} else if (return_code < 0) {
		return return_code;
	}

        return direntv6_dirlookup_core(u, inr, entry, position);
}


/**
 *  Test if the inode is available given the path
 * @param u
 * @param entry
 * @return
 */
int existence_control (struct unix_filesystem *u, const char *entry, char* rel_name, uint16_t *parent_inr)
{
	// Test arguments
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(entry);
	M_REQUIRE_NON_NULL(rel_name);

	size_t entry_length = strlen(entry);
	// Test length of entry
	if (entry_length <= strlen(ROOTDIR_NAME)) {
		return ERR_FILENAME_ALREADY_EXISTS;
	}

	// Find the beginning of the relative name
	size_t beg_rel_name = entry_length;
	while (entry[beg_rel_name] != '/' && beg_rel_name > 0) {
		-- beg_rel_name;
	}

	// If there's no '/'
	if (beg_rel_name == 0) {
		return ERR_BAD_PARAMETER;
	}
	// position after the '/'
	++ beg_rel_name;
	if (strlen(entry + beg_rel_name) > FILENAME_MAX) {
		return ERR_FILENAME_TOO_LONG;
	}

	char *parent_name = malloc(beg_rel_name -1);
	// Create parent name
	memcpy(parent_name,entry_length,beg_rel_name - 2);
	parent_name[beg_rel_name] = '\0';

	// Test if parent exists
	*parent_inr = direntv6_dirlookup(u, ROOT_INUMBER, parent_name);
	if (*parent_inr < 0) {
		return *parent_inr;
	}

	// Test if full path does not exists
	int i_numb = direntv6_dirlookup(u,ROOT_INUMBER, parent_name);
	if (i_numb > 0) {
		return ERR_FILENAME_ALREADY_EXISTS;
	}

	// Copy the relative name
	strncpy(rel_name, entry+beg_rel_name+1,FILENAME_MAX);

	return 0;
}


int direntv6_create(struct unix_filesystem *u, const char *entry, uint16_t mode)
{
	// Test arguments
	M_REQUIRE_NON_NULL(u);
	M_REQUIRE_NON_NULL(entry);

	char *rel_name = calloc(FILENAME_MAX, 1);
	uint16_t parent_inr;

	// Test if path available
	int return_code = existence_control(u, entry, rel_name, &parent_inr);
	if (return_code != 1) {
		return return_code;
	}

	int inr = inode_alloc(u);
	if (inr < 0) {
		return inr;
	}

	// Create filev6 and initialize it
	struct filev6 fv6;
	return_code = filev6_open(u, inr, &fv6);
	if (return_code < 0) {
		return  return_code;
	}
	return_code = filev6_create(u, mode, &fv6);
	if (return_code < 0) {
		return return_code;
	}

	// Create direntv6 corresponding to the inode
	struct direntv6 dir = {};
	dir.d_inumber = inr;
	strncpy(&(dir.d_name), rel_name, FILENAME_MAX);

	// filev6 containing parent
	return_code = filev6_open(u, parent_inr, &fv6);
	if (return_code < 0) {
		return return_code;
	}

	printf("direntv6_create not finished\n");

	free(rel_name);
	rel_name = NULL;

	return 0;
}
