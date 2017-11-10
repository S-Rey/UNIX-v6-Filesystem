#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "mount.h"
#include "error.h"
#include "direntv6.h"
#include "inode.h"
#include "sha.h"


#define SHELL_CMD_SIZE 255

enum shell_error_codes {
        ERR_FIRST_SHELL = 1, // not an actual error but to set the first error number
        ERR_INVALID_CMD,
        ERR_WRONG_NMB_ARG,
        ERR_DISK_NOT_MOUNT,
        ERR_CAT_ON_DIRECTORY,
        ERR_IO_SHELL,
        ERR_LAST_SHELL, // not an actual error but to have e.g. the total number of errors
        EXIT_SHELL
};

const char * const ERR_MESSAGES_SHELL[] = {
        "", // no error
        "invalid command",
        "wrong number of arguments",
        "mount the FS before the operation",
        "cat on a directory is not defined",
        "IO error: No such file or directory"
};

static struct unix_filesystem u;

typedef int (*shell_fct)(const char**);


int do_help (const char** args);
int do_exit (const char** args);
int do_mkfs (const char** args);
int do_mount (const char** args);
int do_mkdir (const char** args);
int do_lsall (const char** args);
int do_add (const char** args);
int do_cat (const char** args);
int do_istat (const char** args);
int do_inode (const char** args);
int do_sha (const char** args);
int do_psb (const char** args);


struct shell_map {
        const char* name;    // nom de la commande
        shell_fct fct;      // fonction réalisant la commande
        const char* help;   // description de la commande
        size_t argc;        // nombre d'arguments de la commande
        const char* args;   // description des arguments de la commande
};


#define NUMBER_OF_CMD 13
static const struct shell_map shell_cmds[] = {
        { "help", do_help, "display this help", 0, ""},
        { "exit", do_exit, "exit shell", 0, ""},
        { "quit", do_exit, "exit shell", 0, ""},
        { "mkfs", do_mkfs, "create a new filesystem", 3, "<diskname> <#inodes> <#blocks>"},
        { "mount", do_mount, "mount the provided filesystem", 1, "<diskname>"},
        { "mkdir", do_mkdir, "create a new directory", 1, "<dirname>"},
        { "lsall", do_lsall, "list all directories and files contained in the currently mounted filesystem", 0, ""},
        { "add", do_add, "add a new file", 2, "<src-fullpath> <dst>"},
        { "cat", do_cat, "display the content of a file", 1, "<pathname>"},
        { "istat", do_istat, "display information about the provided inode", 1, "<inode_nr>"},
        { "inode", do_inode, "display the inode number of a file", 1, "<pathname>"},
        { "sha", do_sha, "display the SHA of a file", 1, "<pathname>"},
        { "psb", do_psb, "Print SuperBlock of the currently mounted filesystem", 0, ""}
};

int nmb_commands() {
        return NUMBER_OF_CMD;
}

/**
 * @brief       test if the number of argument is correct for a shell_map
 * @param sh    the correct shell_map
 * @param args  the arguments
 * @return      0 if OK, > 0 if error
 */
int correct_args_numb(const struct shell_map* sh, const char** args) {
        if (sh == NULL || args == NULL) {
                return ERR_INVALID_CMD;
        }
        int n = 0;
        while (args[n] != NULL && n < nmb_commands()) {
                ++n;
        }
        if (n - 1 == sh->argc) {
                return 0;
        } else {
                return ERR_WRONG_NMB_ARG;
        }
}

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
int fill_inode(const char* entry, struct inode* i, size_t* inr) {
        M_REQUIRE_NON_NULL(i);
        M_REQUIRE_NON_NULL(inr);

        if(is_mounted(&u)) {
                // Find inode number
                int i_numb = direntv6_dirlookup(&u, ROOT_INUMBER, entry);
                if (i_numb < 0) {
                        return i_numb;
                }

                int return_code = inode_read(&u, i_numb, i);
                if (return_code < 0) {
                        return return_code;
                }
                *inr = i_numb;
                return 0;
        }
        return ERR_DISK_NOT_MOUNT;
}



int do_help (const char** args)
{
        /* cannot use size of struct shell_cmd as it is a table
         * => need of some macros as NUMBER_OF_CMD
         */
        for (int i = 0; i < nmb_commands(); ++i) {
                printf(" - %s", shell_cmds[i].name);
                if (shell_cmds[i].argc != 0) {
                        printf(" %s", shell_cmds[i].args);
                }
                printf(": %s.\n", shell_cmds[i].help);
        }
        return 0;
}

int do_exit (const char** args)
{
        umountv6(&u);
        return EXIT_SHELL;
}

int do_mkfs (const char** args)
{
        int num_blocks = atoi(args[2]);
        int num_inodes = atoi(args[3]);

	// Test if valid values
        if (num_blocks < 0 || num_inodes < 0 ||
            (uint16_t)num_blocks > INT16_MAX || (uint16_t)num_inodes > INT16_MAX) {
                return ERR_BAD_PARAMETER;

        }

        int return_code = mountv6_mkfs(args[1],
                                       (uint16_t)num_inodes, (uint16_t)num_blocks);
        return return_code;
}

int do_mount (const char** args)
{
        umountv6(&u);
        return mountv6(args[1], &u);
}

int do_mkdir(const char** args)
{
        return 0;
}

int do_lsall (const char** args)
{
        // Test that u has been mounted
        if(is_mounted(&u)) {
            return direntv6_print_tree(&u, ROOT_INUMBER, "");
        }
        return ERR_DISK_NOT_MOUNT;
}

int do_add(const char** args)
{
        return 0;
}

int do_cat(const char** args)
{
        size_t inr;
        struct inode i;

        int error = fill_inode(args[1], &i, &inr);//correcteur : dirlookup was enough
        if(error) {
                return error;
        }
        if ((i.i_mode & IFMT) != IFDIR) {
                struct filev6 fs;
  	        memset(&fs, 255, sizeof(fs));

                int error = filev6_open(&u, inr, &fs);
                if(error) {
                        return error;
                }
                //inode_print(&inr);
                char data[inode_getsectorsize(&i)];
                int read_bytes = filev6_readblock(&fs, data);
                while(read_bytes == SECTOR_SIZE) { //correcteur : coherent with your readblock but please refactor
                        data[SECTOR_SIZE] = '\0';
                        printf("%s", data);
                        memset(data, 0, SECTOR_SIZE);
                        read_bytes = filev6_readblock(&fs, data);
                }
                return read_bytes;
        } else {
                return ERR_CAT_ON_DIRECTORY;
        }
}

int do_inode(const char** args)
{

        int i_numb = direntv6_dirlookup(&u, ROOT_INUMBER, args[1]);
        if(i_numb < 0) {
                return i_numb;
        }
        printf("inode: %d\n", i_numb);
        return 0;
}

int do_sha(const char** args)
{
        size_t inr;
        struct inode i;

        int error = fill_inode(args[1], &i, &inr);
        if (error) {
                return error;
        }

        if ((i.i_mode & IFMT) != IFDIR) {
                print_sha_inode(&u, i, inr);
                return 0;
        } else {
                printf("SHA inode %u: no SHA for directories.\n", inr);
                return 0;
        }
}

int do_istat (const char** args)
{
	int inr = atoi(args[1]);
	// Test the validity of the command
	if (inr <= 0 || inr > UINT16_MAX || (strlen(args[1]) > 5)) {
		return ERR_INVALID_CMD;
	}
	// If disk u is mounted, then print inode
	if(is_mounted(&u)){
		struct inode i;
		int error = inode_read(&u, (uint16_t)inr, &i);
		inode_print(&i);
                return 0;
	}
        return ERR_DISK_NOT_MOUNT;
}

int do_psb (const char** args)
{
        if(is_mounted(&u)) {
                mountv6_print_superblock(&u);
                return 0;
        }
        return ERR_DISK_NOT_MOUNT;
}








/**
 * @brief          tokenize an input into a table of pointer to char
 * @param input    the line we want to split
 * @param tokens   the table of arguments
 * @return         0 on success, > 0 Shell error, < 0 file error
 */
int tokenize_input(char* input, char*** tokens)
{
        if(input == NULL){
                return ERR_INVALID_CMD;
        }

        int index = 0;
        (*tokens) = calloc(SHELL_CMD_SIZE, sizeof(char*));//correcteur : avoid dynamic allocation when not needed
        char* token;
        // if cannot allocate the memory for the tokens
        if(!*tokens) {
                return ERR_IO;
        }

        input[strcspn(input, "\n")] = '\0';


        token = strtok(input," \n\t\r\a");
        while(token != NULL) {
                (*tokens)[index] = token;
                ++index;

                // if there is more than SHELL_CMD_SIZE tokens => bug somewhere
                if(index >= SHELL_CMD_SIZE) {
                        return ERR_INVALID_CMD;
                }
                token = strtok(NULL, " ");
        }
        (*tokens)[index] == NULL;
        return 0;
}



/**
 * @brief       execute the command passed as parameter
 * @param args  the command
 * @return      0 if success, > 0 if Shell error, < 0 if file error
 */
int shell_execute(char** args)
{
        // Check if args NULL or no command (args[0])
        if (args == NULL || args[0] == NULL) {
                return ERR_INVALID_CMD;
        }

        // Search through the existing command if any match
        for(int i = 0; i < nmb_commands(); ++i) {
                // in case of a match, run the function
                if(strcmp(args[0], shell_cmds[i].name) == 0) {
                        int error = correct_args_numb(&(shell_cmds[i]), args);
                        if (error) {
                                return error;
                        }
                        return shell_cmds[i].fct(args);
                }
        }

        return ERR_INVALID_CMD;
}




void shell_loop()
{
        int error = 0;
        char** args;
        char* input;
        while (!feof(stdin) && !ferror(stdin) && error != EXIT_SHELL) {
                error = 0;
		// Calloc to clean input => some bug later read too much of it
                input = (char*)calloc(SHELL_CMD_SIZE, sizeof(char)); //correcteur : no need for dynamic allocation here
                if(!input) {
                        return; //correcteur : ?
                }

                // if error while fgets
                if (fgets(input, SHELL_CMD_SIZE, stdin) == NULL) {
                        error = ERR_INVALID_CMD;
                }

                // tokenize the input
                if (error == 0) {
                        error == tokenize_input(input, &args);
                }



                if (error == 0) {
                        error = shell_execute(args);
                }

                /*
                 * Print error
                 * error > 0 && error < ERR_LAST_SHELL: Shell error
                 * error < 0 : FS error
                 */
                if (error > 0 && error < ERR_LAST_SHELL) {
                        printf("ERROR SHELL: %s\n", ERR_MESSAGES_SHELL[error - ERR_FIRST_SHELL]);
                } else if (error < 0) {
                        printf("ERROR FS: %s\n", ERR_MESSAGES[error- ERR_FIRST]);
                }

                int i = 0;
                while(args[i] != NULL) {
                        args[i] = NULL;
                        ++i;
                }

                free(args);
                args=NULL;

                free(input);
        }       input = NULL;

}

int main(int argc, char *argv[])
{
        shell_loop();
        return 0;
}


