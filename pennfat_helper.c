#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdbool.h>
#include "pennfat.h"
#include "pennfat_helper.h"

// adds data to end of file, given a block number in the file
int append_to_penn_fat(char* data, int block_no, int n) {
    // Find last block in file (assumes delete_from_penn_fat removes all old blocks)
    printf("[DEBUG] append_to_penn_fat - block_no: %d, str: %s\n", block_no, data);
    int last_block = block_no;
    int next_block = last_block;
    int fs_fd = open(FS_NAME, O_RDWR);
    while (next_block != 0XFFFF) {
        last_block = next_block;
        next_block = FAT_TABLE[next_block];
    }
    // Iterate through data block by block (each block is block_size bytes)
    char* cur_data_block;
    int offset = 0;
    while (offset < sizeof(char) * strlen(data)) {
        n = n - offset  >= 1 ? n - offset : 0;
        int max_size = n < BLOCK_SIZE ? n : BLOCK_SIZE;
        cur_data_block = strndup(&data[offset], max_size);
        if (!cur_data_block) {
            perror("cat - Error copying data block with strndup");
            return -1;
        }
        offset += sizeof(char) * strlen(cur_data_block);
        // Add new block to FAT chain
        int new_final_block = find_first_free_block();
        FAT_TABLE[last_block] = new_final_block;
        FAT_TABLE[new_final_block] = 0XFFFF;
        last_block = new_final_block;
        // Write data to new block
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (new_final_block - 1)), SEEK_SET);
        write(fs_fd, cur_data_block, sizeof(char) * strlen(cur_data_block));
        free(cur_data_block);

        if (n - strlen(cur_data_block) < 1) {
            // Add null terminator and break
            printf("[DEBUG] append_to_penn_fat - adding null terminator\n");
            write(fs_fd, "\0", sizeof(char));
            return offset;
        }
    }
    return strlen(data);
}

char* read_file_to_string(int fd) {
    // Seek to the end of the file to determine its size
    off_t file_size = lseek(fd, 0, SEEK_END);
    if (file_size == -1) {
        perror("Error seeking to end of file");
        return NULL;
    }
    // Allocate a buffer to hold the file content
    char* buffer = (char*)malloc(file_size + 1);  // +1 for null terminator
    if (buffer == NULL) {
        perror("Error allocating memory for file content");
        return NULL;
    }
    // Seek back to the beginning of the file
    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("Error seeking to beginning of file");
        free(buffer);
        return NULL;
    }
    // Read the entire file content into the buffer
    ssize_t bytes_read = read(fd, buffer, file_size);
    if (bytes_read == -1) {
        perror("Error reading file content");
        free(buffer);
        return NULL;
    }
    // Null-terminate the string
    buffer[bytes_read] = '\0';
    return buffer;
}


// TODO: parse args in shell
int cp(const char *source, const char *dest, int s_host, int d_host) {
    
    if (d_host) { // dest is in host
        cp_to_h(source, dest);
    } else if (s_host) { // source is in host
        cp_from_h(source, dest);
    } else { // both are in FAT
        cp_helper(source, dest);
    }
    return 0;
}

int cp_helper(const char *source, const char *dest) {
    int fs_fd = open(FS_NAME, O_RDWR);

    // both in fat
    DirectoryEntry* d_entry = get_entry_from_root(dest, true, NULL);
    if (d_entry) {
        delete_entry_from_root(dest);
    }
    // create new file with name
    touch(dest);

    // find source file
    // open file
    free(d_entry);
    DirectoryEntry* entry = get_entry_from_root(source, true, NULL);

    int* chain = get_fat_chain(entry->firstBlock);

    for (int i = 0; i < NUM_FAT_ENTRIES; i++) {
        if (!chain[i]) {
            break;
        }
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (chain[i] - 1)), SEEK_SET);
        char* txt = malloc(sizeof(char) * BLOCK_SIZE);
        read(fs_fd, txt, sizeof(char) * BLOCK_SIZE);
        // write to file
        append_to_penn_fat(txt, d_entry->firstBlock, BLOCK_SIZE);
    }
    return 0;
}

int cp_from_h(const char *source, const char *dest) {
    // copy from host file

    // open host file to read
    int h_fd = open(source, O_RDONLY);
    if (h_fd == -1) {
        perror("Error opening file");
        exit(1);
    }

    // check if file exists in fat, remove if it does
    DirectoryEntry* entry = get_entry_from_root(dest, true, NULL);
    if (entry) {
        delete_entry_from_root(dest);
    }
    // create new file with name
    touch(dest);
    // TODO: update directory entry with the first block of file
    char* txt = read_file_to_string(h_fd);
    int i = find_first_free_block();
    append_to_penn_fat(txt, i, BLOCK_SIZE);
    return 0;
}

// copying from fat to host
int cp_to_h(const char *source, const char *dest) {
    // open fat binary
    int fs_fd = open(FS_NAME, O_RDWR);
    if (fs_fd == -1) {
        perror("Error opening file system image");
        exit(1);
    }

    // get directory entry for file
    DirectoryEntry* entry = get_entry_from_root(source, true, NULL);
    if (!entry) {
        perror("Error: source file does not exist");
        return -1;
    }

    // file chain
    int* chain = get_fat_chain(entry->firstBlock);

    // open host file
    int h_fd = open(dest, O_WRONLY);

    for (int i = 0; i < NUM_FAT_ENTRIES; i++) {
        if (!chain[i]) {
            break;
        }
        // copy block into host file
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (chain[i] - 1)), SEEK_SET);
        char* txt = malloc(sizeof(char) * BLOCK_SIZE);
        read(fs_fd, txt, sizeof(char) * BLOCK_SIZE);
        write(h_fd, txt, sizeof(char) * strlen(txt));
    }
    free(chain);
    close(fs_fd);
    close(h_fd);
    return 0;
}