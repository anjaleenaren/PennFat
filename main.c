#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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
int main(int argc, char *argv[]) {
    // Initialize or mount the file system here

    // mkfs("minfs", 1, 1);
    mkfs("maxfs", 32, 4);
    // mkfs("testfs", 1, 0);

    mount("maxfs");
    int fd = f_open("test.txt", F_WRITE);
    //print fd
    printf("fd: %d\n", fd);
    printf("\nwrite: %d\n", f_write(fd, "Hello world!\n", 5));
    int fd1 = f_open("other.txt", F_WRITE);
    printf("\nwrite: %d\n", f_write(fd1, "Hello world OTHER!\n", 50));
    // print fd
    printf("fd1: %d\n", fd1);
    // f_write(fd, "Hello world!\n", 25);
    f_close(fd1);
    f_close(fd);    
    mv("test.txt", "new.txt");
    unmount();

    return 0;
}