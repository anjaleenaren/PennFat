#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pennfat.h>

#define MAX_FAT_ENTRIES 65534 // Maximum for FAT16

void mkfs(const char *fs_name, int blocks_in_fat, int block_size_config) {
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
    int fat_size = block_size * blocks_in_fat;
    int num_fat_entries = (block_size * blocks_in_fat) / 2;
    int data_region_size = block_size * (num_fat_entries - 1);
    

    // Set the file system size
    if (ftruncate(fs_fd, fat_size) == -1) {
        perror("Error setting file system size");
        close(fs_fd);
        exit(1);
    }

    // TODO Imeplement ///////////////////////
    // Initialize FAT with 0s (free blocks)
    uint16_t *fat = (uint16_t *)calloc(blocks_in_fat, 2); //Todo: is calloc right?
    if (!fat) {
        perror("Failed to allocate FAT");
        close(fs_fd);
        exit(1);
    }

    // Write FAT to the file system file
    if (write(fs_fd, fat, fat_size) != fat_size) {
        perror("Failed to write FAT to file system image");
        free(fat);
        close(fs_fd);
        exit(1);
    }

    // Mmap for whole fat and data region in one call
    uint16_t *fat = mmap(NULL, fat_size+data_region_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    if (fat == MAP_FAILED) {
        perror("Error mapping file system");
        close(fs_fd);
        exit(1);
    }

    // Malloc data entry struct
    // Stores previous and next block pointers, index within fat

    // Malloc structs for root directory and first entry
    DirectoryEntry* root = malloc(sizeof(DirectoryEntry));
    DirectoryEntry* first_entry = malloc(sizeof(DirectoryEntry));

    // Write fat to memory: read number of blocks and block size

    // Case on what size should be based on LSB of first entry of FAT
    // File allocation table struct for metadata
    // Inside struct is a string called fatname (which will be used as “disk”)

    // Treat the fat name as a file and pass into regular open system call -> creates your disk file
    // Touch: open FAT name as file, seek to appropriate place, call write() to update internal structure of FAT. close file once done
    // Validate that filename is allowed first
    // Keep track of what current and previous blocks are to update root directory with the new file created
    // Root directory update: seek to the root directory position and then write() (several times)
    // Can use read and write within filesystem implementation
    // Touch calls “write_file_to_directory” (lower func we make - can use read write seek)
    // Creates new filesystem from scratch - creates the file and sets up root directory

    /////////////////////////

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

    // Memory map the file system
    uint16_t *fat = mmap(NULL, fat_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    if (fat == MAP_FAILED) {
        perror("Error mapping file system");
        close(fs_fd);
        exit(1);
    }

    // TODO: Load FAT and directory structure into memory

}