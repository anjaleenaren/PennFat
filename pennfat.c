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

    // Mmap for FAT table region
    FAT_TABLE = mmap(NULL, FAT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    if (FAT_TABLE == MAP_FAILED) {
        perror("Error mmapping the directory entries");
        close(fs_fd);
        exit(1);
    }

    // Initialize the FAT_TABLE
    FAT_TABLE[0] = (blocks_in_fat << 8) | block_size_config; //MSB = blocks_in_fat, LSB = block_size_config
    for (int i = 1; i < NUM_FAT_ENTRIES; i++) {
        DirectoryEntry *entry = malloc(sizeof(DirectoryEntry));
        entry->firstBlock = 0;
        FAT_TABLE[i] = entry;
    }

    close(fs_fd);
}

void mount(const char *fs_name) {
    // Open the file system file
    int fs_fd = open(fs_name, O_RDWR);
    if (fs_fd == -1) {
        perror("Error opening file system image");
        exit(1);
    }

    // Mmap for data region
    FAT_DATA = mmap(NULL, DATA_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, FAT_SIZE);
    if (FAT_DATA == MAP_FAILED) {
        perror("Error mapping file system");
        close(fs_fd);
        exit(1);
    }

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
    for (int i = 1; i < NUM_FAT_ENTRIES; i++) {
        DirectoryEntry *entry = FAT_TABLE[i];
        free(entry->name);
        free(entry);
    }

    // Unmap the memory-mapped region
    if (munmap(FAT_TABLE, FAT_SIZE) == -1) {
        perror("Error unmapping file system - table");
        close(fs_fd);
        exit(1);
    }
    if (munmap(FAT_DATA, FAT_SIZE) == -1) {
        perror("Error unmapping file system - data");
        close(fs_fd);
        exit(1);
    }

    close(fs_fd);
}