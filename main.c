#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdbool.h>
#include "pennfat.h"

// #define MAX_FILES 32 // Maximum number of files in the root directory

// // Directory entry structure
// typedef struct {
//     char name[32];
//     uint32_t size;
//     uint16_t firstBlock;
//     uint8_t type;
//     uint8_t perm;
//     time_t mtime;
//     char reserved[16];
// } DirectoryEntry;

// // Global variables for the filesystem
// DirectoryEntry rootDirectory[MAX_FILES];
// uint16_t *FAT;
// uint16_t blockSize;
// uint32_t fatSize;
// FILE *fsFile;

// // Function to open a file in the filesystem
// int f_open(const char *filename, const char *mode) {
//     // ... implementation ...
// }

// Main program for testing
// int main(int argc, char *argv[]) {
//     // Initialize or mount the file system here

//     // mkfs("minfs", 1, 1);
//     mkfs("maxfs", 32, 4);
//     // mkfs("testfs", 1, 0);

//     mount("maxfs");
//     touch("test.txt");
//     // int fd = f_open("test.txt", F_WRITE);
//     // f_write(fd, "hey!\n", sizeof(char) * strlen("hey!\n"));
//     // int fd1 = f_open("test.txt", F_READ);
//     // char* buf = malloc(sizeof(char) * strlen("Hello world!\n hey!\n buffer space"));    
//     //print fd
//     // printf("fd: %d\n", fd);
//     // printf("ORIGINAL\n");
//     // printf("\nwrite: %d\n", f_write(fd, "hey!\n", sizeof(char) * strlen("hey!\n")));
//     // f_read(fd1, sizeof(char) * strlen("Hello world!\n hey!\n buffer space"), buf);
//     // printf("%s\n", buf);
//     // printf("READ\n");
//     // f_read(fd1, sizeof(char) * strlen("Hello world!\n hey!\n buffer space"), buf);
//     // printf("%s\n", buf);
//     // int fd1 = f_open("other.txt", F_WRITE);
//     // printf("fd1: %d\n", fd1);
//     // printf("\nwrite: %d\n", f_write(fd1, "Hello world OTHER!\n", 50));
//     // f_write(fd, "hey!\n", sizeof(char) * strlen("hey!\n"));
//     printf("LS\n");
//     f_ls(NULL);
//     // f_unlink("test.txt");
//     // printf("LS\n");
//     // f_ls(NULL);
//     // f_close(fd); 
//     // f_unlink("test.txt");
//     // printf("LS\n");
//     // f_ls(NULL);
//     // printf("CP\n");
//     // cp("dest.txt", "test.txt", 1, 0);
//     // f_ls(NULL);
//     // int fd1 = f_open("test.txt", F_READ);
//     // char* buf = malloc(sizeof(char) * strlen("Hello world!\n hey!\n buffer space"));    
//     // int dest_fd = f_open("dest.txt", F_READ);
//     // f_read(fd1, sizeof(char) * strlen("Hello world!\n hey!\n buffer space"), buf);
//     // printf("%s\n", buf);
//     // f_close(fd1);
//     // f_close(fd1); 

//     // mv("test.txt", "new.txt");
//     // f_ls(NULL);
//     // printf("REMOVING\n");
//     // // rm("new.txt");
//     // rm("other.txt");
//     // rm("new.txt");
//     unmount();

//     return 0;
// }


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
        // write(1, "111\n", sizeof(char) * strlen("111\n"));
        // // Parse the input
        // int count = sscanf(input, "%s", command);
        // char args[60][60]; // TODO: assumes max size 60 each, can be changed later to terminal code with malloc
        // count = 0;
        // token = strtok(input, " \n");
        // while (token != NULL && count < 50) {
        //     strcpy(args[count++], token);
        //     token = strtok(NULL, " \n");
        // }
        // // these args have to be null terminated pls. tok should do this, keep in mind with terminal
        // strcpy(command, args[0]);
        // strcpy(arg1, args[1]);
        // strcpy(arg2, args[2]);
        // strcpy(arg3, args[3]);
        // Process commands


        if (strcmp(token, "exit") == 0) {
            break;
        } else if (strcmp(token, "mkfs") == 0) {
            char *fs_name = strtok(NULL, " ");
            int blocks_in_fat = atoi(strtok(NULL, " "));
            int block_size_config = atoi(strtok(NULL, " "));
            mkfs(fs_name, blocks_in_fat, block_size_config);
        } else if (strcmp(token, "mount") == 0) {
            char *fs_name = strtok(NULL, " ");
            mount(fs_name);
        } else if (strcmp(token, "umount") == 0) {
            umount();
        } else if (strcmp(token, "touch") == 0) {
            while ((token = strtok(NULL, " ")) != NULL) {
                touch(token);
            }
        } else if (strcmp(token, "mv") == 0) {
            char *source = strtok(NULL, " ");
            char *dest = strtok(NULL, " ");
            mv(source, dest);
        } else if (strcmp(token, "rm") == 0) {
            while ((token = strtok(NULL, " ")) != NULL) {
                rm(token);
            }
        } else if (strcmp(token, "cat") == 0) {
            int argc = 0;
            char *argv[1024];
            while ((token = strtok(NULL, " ")) != NULL) {
                argv[argc++] = token;
            }
            
            if (strcmp(argv[0], "-w") == 0) {
                // no input files, write mode
                cat(NULL, 0, argv[1], 0);
            } else if (strcmp(argv[0], "-a") == 0) {
                // no input, append mode
                cat(NULL, 0, argv[1], 1);
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
            char * arg1 = strtok(NULL, " ");
            char * arg2 = strtok(NULL, " ");
            char * arg3 = strtok(NULL, " ");
            // if both in fat
            if (arg3 == NULL) {
                cp(arg1, arg2, 0, 0);
            } else if (strcmp(arg1, "-h") == 0) {
                cp(arg2, arg3, 1, 0);
            } else if (strcmp(arg2, "-h") == 0) {
                cp(arg1, arg3, 0, 1);
            }
        } else if (strcmp(token, "ls") == 0) {
            // write(1, "156\n", sizeof(char) * strlen("156\n"));
            f_ls(NULL);
        } else {
            continue;
        }
    }

    return 0;
}