#include <openssl/sha.h>
#include <string.h>
#include "sha.h"
#include "inode.h"
#include "sector.h"
#include "error.h"
#include "filev6.h"


static void sha_to_string(const unsigned char *SHA, char *sha_string)
{
    if ((SHA == NULL) || (sha_string == NULL)) {
        return;
    }

    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        sprintf(&sha_string[i * 2], "%02x", SHA[i]);
    }

    sha_string[2 * SHA256_DIGEST_LENGTH] = '\0';
}

void print_sha_from_content(const unsigned char *content, size_t length)
{
        // if the input is NULL, quit the function
        if (content == NULL) {
		return;
	}

        // create a table of size SHA_DIGEST_LENGTH to store the future hash
        unsigned char hash[SHA256_DIGEST_LENGTH];
        // Compute the hash and store it to hash
        SHA256(content, length, hash);

	char sha_string[SHA256_DIGEST_LENGTH * 2];

	sha_to_string(hash, sha_string);

        // Call the print_sha function
        printf("%s\n", sha_string);

}


void print_sha_inode(struct unix_filesystem *u, struct inode inode, int inr) {
        // Test argument
        if(u == NULL) {
                return;
        }

        // if inode allocated
        if(inode.i_mode & IALLOC) {
		printf("SHA inode %d: ", inr);
	} else {
		debug_print("[--] print_sha_inode inode not allocated\n", NULL);
		return;
	}

        // if a directory
        if(inode.i_mode & IFDIR) {
                printf("no SHA for directories.\n");
                return;
        }

        struct filev6 fv6;
        memset(&fv6, 255, sizeof(fv6));
        int error = filev6_open(u,(uint16_t)inr, &fv6);
        if (error) {
                return;
        }

        // Number of sector on which is the inode
        uint32_t n_sector = (uint32_t)((inode_getsize(&inode)-1) / SECTOR_SIZE) + 1;
        unsigned char content[n_sector * SECTOR_SIZE];

        // Store the content of inode in content
        size_t i = 0;
        int read_bytes = filev6_readblock(&fv6, content);
        while (read_bytes == SECTOR_SIZE) {
                ++i;
                read_bytes = filev6_readblock(&fv6, &(content[i*SECTOR_SIZE]));
        }
        // Print the sha only for the size of the inode
        print_sha_from_content(content, (size_t)inode_getsize(&inode));
}

