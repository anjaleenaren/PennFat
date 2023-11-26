#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pennfat.h>

#define MAX_FAT_ENTRIES 65534 // Maximum for FAT16

void mkfs(const char *fs_name, int blocks_in_fat, int block_size_config) {
    BLOCKS_IN_FAT = blocks_in_fat;
    int fs_fd = open(fs_name, O_RDWR | O_CREAT, 0666);
    if (fs_fd == -1) {
        perror("Error creating file system image");
        exit(1);
    }

    int block_size;
    switch(block_size_config) {
        case 0:
            block_size = 256;
            break;
        case 1:
            block_size = 512;
            break;
        case 2:
            block_size = 1024;
            break;
        case 3:
            block_size = 2048;
            break;
        default:
            block_size = 4096;
            break;
    }

    // Calculate FAT and file system size
    FAT_SIZE = block_size * blocks_in_fat;
    NUM_FAT_ENTRIES = (block_size * blocks_in_fat) / 2;
    DATA_REGION_SIZE = block_size * (NUM_FAT_ENTRIES - 1);

    // Set the file system size
    if (ftruncate(fs_fd, FAT_SIZE) == -1) {
        perror("Error setting file system size");
        close(fs_fd);
        exit(1);
    }

    uint16_t *fat = (uint16_t *)calloc(BLOCKS_IN_FAT, 2); //Todo: is calloc right?
    if (!fat) {
        perror("Failed to allocate FAT");
        close(fs_fd);
        exit(1);
    }

    // Write FAT to the file system file
    if (write(fs_fd, fat, FAT_SIZE) != FAT_SIZE) {
        perror("Failed to write FAT to file system image");
        free(fat);
        close(fs_fd);
        exit(1);
    }

    free(fat);
    close(fs_fd);
}

void mount(const char *fs_name) {
    // Open the file system file
    int fs_fd = open(fs_name, O_RDWR);
    if (fs_fd == -1) {
        perror("Error opening file system image");
        exit(1);
    }

    // TODO Imeplement ///////////////////////

    // Mmap for whole fat and data region in one call
    FAT_MAP = mmap(NULL, FAT_SIZE+DATA_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    if (FAT_MAP == MAP_FAILED) {
        perror("Error mapping file system");
        close(fs_fd);
        exit(1);
    }

    // Malloc data entry struct
    // Stores previous and next block pointers, index within fat
    // Malloc structs for root directory and first entry
    ROOT = malloc(sizeof(DirectoryEntry));

    close(fs_fd);
}

void umount(const char *fs_name) {
    // Open the file system file
    int fs_fd = open(fs_name, O_RDWR);
    if (fs_fd == -1) {
        perror("Error opening file system image");
        exit(1);
    }

    // Free directory entries
    free(ROOT);

    // Unmap the memory-mapped region
    if (munmap(FAT_MAP, FAT_SIZE) == -1) {
        perror("Error unmapping file system");
        close(fs_fd);
        exit(1);
    }

    // TODO: how to keep track of root and first entry in order to free them??
    // Free dynamically allocated memory
    // free(root);
    // free(first_entry);

    close(fs_fd);
}