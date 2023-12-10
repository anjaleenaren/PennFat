#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdbool.h>
#include "pennfat.h"

int main() {
    char input[1024];

    while (1) {
        write(STDOUT_FILENO, "pennfat# ", sizeof(char) * strlen("pennfat# "));
        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("Error handling input");
            return -1;
        }
        // write(1, "103\n", sizeof(char) * strlen("103\n"));
        // Remove newline character
        input[strcspn(input, "\n")] = '\0';

        char *token = strtok(input, " ");
        if (token == NULL) {
            continue;  // Empty line
        }

        if (strcmp(token, "exit") == 0) {
            if (FS_NAME != NULL) {
                umount();
            }
            break;
        // } else if (strcmp(token, "del") == 0) { // take this out later
        //     delete_from_penn_fat("f1");
        } else if (strcmp(token, "mkfs") == 0) {
            char *fs_name = strtok(NULL, " ");
            int blocks_in_fat = atoi(strtok(NULL, " "));
            int block_size_config = atoi(strtok(NULL, " "));
            mkfs(fs_name, blocks_in_fat, block_size_config);
        } else if (strcmp(token, "mount") == 0) {
            char *fs_name = strtok(NULL, " ");
            mount(fs_name);
        } else if (strcmp(token, "umount") == 0) {
            if (FS_NAME == NULL) {
                continue;
            }
            umount();
        } else if (strcmp(token, "touch") == 0) {
            if (FS_NAME == NULL) {
                continue;
            }
            while ((token = strtok(NULL, " ")) != NULL) {
                touch(token);
            }
        } else if (strcmp(token, "mv") == 0) {
            if (FS_NAME == NULL) {
                continue;
            }
            char *source = strtok(NULL, " ");
            char *dest = strtok(NULL, " ");
            mv(source, dest);
        } else if (strcmp(token, "rm") == 0) {
            if (FS_NAME == NULL) {
                continue;
            }
            while ((token = strtok(NULL, " ")) != NULL) {
                rm(token);
            }
        } else if (strcmp(token, "cat") == 0) {
            if (FS_NAME == NULL) {
                continue;
            }
            int argc = 0;
            char *argv[1024];
            while ((token = strtok(NULL, " ")) != NULL) {
                argv[argc++] = token;
            }

            if (argc == 0) {
                continue;
            } else if (strcmp(argv[0], "-w") == 0) {
                // no input files, write mode
                cat(NULL, 0, argv[1], 0);
            } else if (strcmp(argv[0], "-a") == 0) {
                // no input, append mode
                cat(NULL, 0, argv[1], 1);
            } else if (argc == 1) {
                // input, std out
                cat(argv, argc, NULL, 0);
            } else if (strcmp(argv[argc-2], "-w") == 0) {
                // input, output, write
                cat(argv, argc-2, argv[argc-1], 0);
            } else if (strcmp(argv[argc-2], "-a") == 0) {
                // input, output, append
                cat(argv, argc-2, argv[argc-1], 1);
            } else if (argc > 0) {
                // input, no output file
                cat(argv, argc, NULL, 0);
            }

        } else if (strcmp(token, "cp") == 0) {
            if (FS_NAME == NULL) {
                continue;
            }
            // write(1, "cp\n", sizeof(char) * strlen("cp\n"));
            char * arg1 = strtok(NULL, " ");
            char * arg2 = strtok(NULL, " ");
            char * arg3 = strtok(NULL, " ");
            // if both in fat
            if (arg3 == NULL) {
                cp(arg1, arg2, 0, 0);
            } else if (strcmp(arg1, "-h") == 0) {
                // source in host
                cp(arg2, arg3, 1, 0);
            } else if (strcmp(arg2, "-h") == 0) {
                // dest in host
                // write(1, "dest host\n", sizeof(char) * strlen("dest host\n"));
                cp(arg1, arg3, 0, 1);
            }
        } else if (strcmp(token, "ls") == 0) {
            if (FS_NAME == NULL) {
                continue;
            }
            // write(1, "156\n", sizeof(char) * strlen("156\n"));
            f_ls(NULL);
        } else if (strcmp(token, "chmod") == 0) {
            if (FS_NAME == NULL) {
                continue;
            }
            char *filename = strtok(NULL, " ");
            char *perm_str = strtok(NULL, " ");
            if (filename == NULL || perm_str == NULL) {
                write(STDOUT_FILENO, "Usage: chmod <filename> <permissions>\n", strlen("Usage: chmod <filename> <permissions>\n"));
                continue;
            }
            uint8_t permissions = (uint8_t)strtol(perm_str, NULL, 8);
            if (chmod(filename, permissions) < 0) {
                perror("chmod failed");
            }
        } else {
            continue;
        }
    }

    return 0;
}