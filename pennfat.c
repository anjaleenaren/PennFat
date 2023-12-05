#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdbool.h>
#include "pennfat.h"

#define MAX_FAT_ENTRIES 65534 // Maximum for FAT16
DirectoryEntry* ROOT = NULL;
FDTEntry** FDT = NULL;
int BLOCKS_IN_FAT = 0;
int BLOCK_SIZE = 0;
int FAT_SIZE = 0;
int NUM_FAT_ENTRIES = 0; 
int TABLE_REGION_SIZE = 0;
int DATA_REGION_SIZE = 0;
int BLOCK_SIZE_CONFIG = 0;
uint16_t *FAT_TABLE = 0;
uint16_t *FAT_DATA = 0;
char* FS_NAME = NULL;

// Helper functions
int write_entry_to_root(DirectoryEntry* entry);
int cp_from_h(const char *source, const char *dest);
int cp_helper(const char *source, const char *dest);
int cp_to_h(const char *source, const char *dest);
int append_to_penn_fat(char* data, int block_no, int n, int size);
int delete_from_penn_fat(const char *filename);
int find_first_free_block();
int add_entry_to_root(DirectoryEntry* entry);
DirectoryEntry* delete_entry_from_root(const char *filename);
DirectoryEntry* get_entry_from_root(const char *filename, bool update_first_block, char* rename_to);
int strcat_data(char* data, int start_index);
int* get_fat_chain(int start_index);


void mkfs(char *fs_name, int blocks_in_fat, int block_size_config) {
    BLOCKS_IN_FAT = blocks_in_fat;
    int fs_fd = open(fs_name, O_RDWR | O_CREAT, 0666);
    if (fs_fd == -1) {
        perror("Error creating file system image");
        exit(1);
    }

    switch(block_size_config) {
        case 0:
            BLOCK_SIZE = 256;
            break;
        case 1:
            BLOCK_SIZE = 512;
            break;
        case 2:
            BLOCK_SIZE = 1024;
            break;
        case 3:
            BLOCK_SIZE = 2048;
            break;
        case 4:
            BLOCK_SIZE = 4096;
            break;
    }

    // Calculate FAT and file system size
    BLOCK_SIZE_CONFIG = block_size_config;
    FAT_SIZE = BLOCK_SIZE * blocks_in_fat;
    NUM_FAT_ENTRIES = (BLOCK_SIZE * blocks_in_fat) / 2;
    TABLE_REGION_SIZE = sizeof(uint16_t) * NUM_FAT_ENTRIES;
    if (BLOCK_SIZE == 4096 && BLOCKS_IN_FAT == 32) {
        DATA_REGION_SIZE = BLOCK_SIZE * (NUM_FAT_ENTRIES - 2);
    } else {
        DATA_REGION_SIZE = BLOCK_SIZE * (NUM_FAT_ENTRIES - 1);
    }
    

    // Set the file system size
    if (ftruncate(fs_fd, FAT_SIZE + DATA_REGION_SIZE) == -1) {
        perror("Error setting file system size");
        close(fs_fd);
        exit(1);
    }

    // Initialize the file descriptor table (FDT)
    // FDT = malloc(sizeof(FDTEntry*) * NUM_FAT_ENTRIES);

    // Mmap for FAT table region
    // FAT_TABLE = mmap(NULL, FAT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    // if (FAT_TABLE == MAP_FAILED) {
    //     perror("Error mmapping the directory entries");
    //     close(fs_fd);
    //     exit(1);
    // }

    // allocate space for fat table
    FAT_TABLE = calloc(1, FAT_SIZE);

    // Initialize the FAT_TABLE
    FAT_TABLE[0] = (BLOCKS_IN_FAT << 8) | BLOCK_SIZE_CONFIG; //MSB = blocks_in_fat, LSB = block_size_config
    // Initialize the root directory
    FAT_TABLE[1] = 0xFFFF; // First block of root directory is FFFF to signal it's the end

    for (int i = 2; i < NUM_FAT_ENTRIES; i++) {
        FAT_TABLE[i] = 0;
    }
    
    write(fs_fd, FAT_TABLE, FAT_SIZE);
    
    free(FAT_TABLE);

    close(fs_fd);
}

void mount(const char *fs_name) {
    // Open the file system file
    // int fs_fd = open(fs_name, O_RDWR);
    // if (fs_fd == -1) {
    //     perror("Error opening file system image");
    //     exit(1);
    // }
    
    int fs_fd = open(fs_name, O_RDWR);
    if (fs_fd == -1) {
        perror("Error creating file system image");
        exit(1);
    }

    uint16_t metadata = 0;
    read(fs_fd, &metadata, sizeof(uint16_t));
    BLOCKS_IN_FAT = metadata >> 8;
    BLOCK_SIZE_CONFIG = metadata & 0xFF;
    // printf("%u %u\n", BLOCKS_IN_FAT, BLOCK_SIZE_CONFIG);
    switch(BLOCK_SIZE_CONFIG) {
        case 0:
            BLOCK_SIZE = 256;
            break;
        case 1:
            BLOCK_SIZE = 512;
            break;
        case 2:
            BLOCK_SIZE = 1024;
            break;
        case 3:
            BLOCK_SIZE = 2048;
            break;
        case 4:
            BLOCK_SIZE = 4096;
            break;
    }
    FAT_SIZE = BLOCK_SIZE * BLOCKS_IN_FAT;
    NUM_FAT_ENTRIES = (BLOCK_SIZE * BLOCKS_IN_FAT) / 2;
    TABLE_REGION_SIZE = sizeof(uint16_t) * NUM_FAT_ENTRIES;

    if (BLOCK_SIZE == 4096 && BLOCKS_IN_FAT == 32) {
        DATA_REGION_SIZE = BLOCK_SIZE * (NUM_FAT_ENTRIES - 2);
    } else {
        DATA_REGION_SIZE = BLOCK_SIZE * (NUM_FAT_ENTRIES - 1);
    }

    // Initialize the file descriptor table (FDT)
    FDT = calloc(1, sizeof(FDTEntry*) * NUM_FAT_ENTRIES);

    // Mmap for FAT table region
    FAT_TABLE = mmap(NULL, FAT_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    if (FAT_TABLE == MAP_FAILED) {
        perror("Error mmapping the directory entries");
        close(fs_fd);
        exit(1);
    }

    FAT_TABLE[0] = metadata;
    
    for (int i = 1; i < NUM_FAT_ENTRIES; i++) {
        // FAT_TABLE[i] = 0;
        read(fs_fd, &FAT_TABLE[i], 2);
    }

    // Initialize the root directory
    FAT_TABLE[1] = 0xFFFF; // First block of root directory is FFFF to signal it's the end
    
    FS_NAME = malloc(sizeof(char) * strlen(fs_name));
    strcpy(FS_NAME, fs_name); // save name

    close(fs_fd);
}

// TODO: global var with fs_name
void umount() {
    // Open the file system file
    int fs_fd = open(FS_NAME, O_RDWR);
    if (fs_fd == -1) {
        perror("Error opening file system image");
        exit(1);
    }

    // Free directory entries TODO
    // 1. Start at fat table index 1, and follow pointers to get location of all data blocks
    // int* root_chain = get_fat_chain(1);
    // 2. Free all data blocks
    // for (int i = 0; i < NUM_FAT_ENTRIES; i++) {
    //     if (!root_chain[i]) {
    //         break;
    //     }
    //     fopen(FS_NAME, "r");
    //     DirectoryEntry** listEntries = FAT_DATA[root_chain[i]];
    //     int max_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
    //     if (listEntries){
    //         for (int j = 0; j < max_entries; j++) {
    //             DirectoryEntry* entry = listEntries[j];
    //             if (entry) {
    //                 free(entry);
    //             }
    //         }
    //     }
    // }
    // 3. Free root_chain
    // free(root_chain);
    free(FS_NAME);
    free(FDT);

    lseek(fs_fd, 0, SEEK_SET);
    // printf("%i\n", FAT_TABLE[1]);
    // printf("%i\n", FAT_TABLE[2]);
    // printf("%i\n", FAT_TABLE[3]);
    // printf("%i\n", FAT_TABLE[4]);
    write(fs_fd, FAT_TABLE, FAT_SIZE);

    // Unmap the memory-mapped region
    if (munmap(FAT_TABLE, TABLE_REGION_SIZE) == -1) {
        perror("Error unmapping file system - table");
        close(fs_fd);
        exit(1);
    }

    // if (munmap(FAT_DATA, DATA_REGION_SIZE) == -1) {
    //     perror("Error unmapping file system - data");
    //     close(fs_fd);
    //     exit(1);
    // }

    close(fs_fd);
}

// get chain of blocks for a file given the start index
int* get_fat_chain(int start_index) {
    // write(1, "entered chain\n", sizeof(char) * strlen("entered chain\n"));
    // malloc array for chain
    int* fat_chain = malloc(sizeof(int) * NUM_FAT_ENTRIES);
    if (fat_chain == NULL) {
        return NULL;
    }
    int i = 0;
    // first block is start_index
    int block = start_index;
    // go until END is reached
    // write(1, "enter loop\n", sizeof(char) * strlen("enter loop\n"));
    while (block != 0xFFFF && block != 0) {
        // write(1, "block\n", sizeof(char) * strlen("block\n"));
        fat_chain[i] = block;
        block = FAT_TABLE[block];
        i++;
    }
    return fat_chain;
}

int strcat_data(char* data, int start_index) {
    int fs_fd = open(FS_NAME, O_RDWR);
    // int start_block = FAT_TABLE[start_index];
    int next_block = start_index;
    int chars_read = 0;
    while (next_block != 0xFFFF && next_block != 0) {
        char* cur_data = calloc(1, sizeof(char)*BLOCK_SIZE);
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (next_block - 1)), SEEK_SET);
        // printf("%i\n", TABLE_REGION_SIZE + (BLOCK_SIZE * (next_block - 1)));
        chars_read += read(fs_fd, cur_data, sizeof(char)*BLOCK_SIZE);
        // write(1, "data is\n", sizeof(char)*strlen("data is\n"));
        // write(1, cur_data, sizeof(char)*strlen(cur_data));
        // write(STDOUT_FILENO, "strcat check: \n", sizeof(char) * strlen("strcat check: \n"));
        // write(STDOUT_FILENO, cur_data, sizeof(char) * strlen(cur_data));
        if (cur_data) {
            strcat(data, cur_data);
        }
        next_block = FAT_TABLE[next_block];
    }
    // number of chars read
    return chars_read;
}

int find_first_free_block() {
    for (int i = 1; i < NUM_FAT_ENTRIES; i++) {
        if (FAT_TABLE[i] == 0) {
            return i;
        }
    }
    return -1;
}

int delete_from_penn_fat(const char *filename) {
    // See if file currently exists by iterating through root directory
    DirectoryEntry* entry = get_entry_from_root(filename, true, NULL);
    if (!entry) {
        perror("Source file does not exist");
        return 0;
    }
    int fs_fd = open(FS_NAME, O_RDWR);
    // Delete file from fat table if it does exist
    int block = entry->firstBlock;
    while (block != 0xFFFF && block != 0) {
        int next_block = FAT_TABLE[block];
        FAT_TABLE[block] = 0;
        block = next_block;
        lseek(fs_fd, block * 2, SEEK_SET);
        write(fs_fd, &FAT_TABLE[block], 2);
    }

    // Delete entry from root directory
    delete_entry_from_root(filename);
    close(fs_fd);
    return 0;
}

DirectoryEntry* get_entry_from_root(const char *filename, bool update_first_block, char* rename_to) {
    // write(1, "getting entry\n", sizeof(char) * strlen("getting entry\n"));
    int start_block = 1;
    int next_block = start_block;
    int fs_fd = open(FS_NAME, O_RDWR);

    int num_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
    // write(1, "init vals\n", sizeof(char) * strlen("init vals\n"));

    // Iterate through all the root blocks
    while (next_block != 0xFFFF && next_block != 0) {
        // write(1, "loop iter\n", sizeof(char) * strlen("loop iter\n"));
        // Check if file exists in current block (go through current root block's entries)
        int block_to_check = next_block;
        // seg fault here since going out of bounds of file size
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (block_to_check - 1)), SEEK_SET);
        
        for (int i = 0; i < num_entries; i++) {
            // write(1, "num_entry\n", sizeof(char) * strlen("num_entry\n"));
            DirectoryEntry* read_struct = calloc(1, sizeof(DirectoryEntry));
            read(fs_fd, read_struct, sizeof(DirectoryEntry));
            
            // if wanted file has been found
            if (read_struct && strcmp(read_struct->name, filename) == 0) {
                if (update_first_block) {
                    if (read_struct->firstBlock == (uint16_t) -1) {
                        read_struct->firstBlock = find_first_free_block();
                        read_struct->mtime = time(NULL);
                        FAT_TABLE[read_struct->firstBlock] = 0xFFFF;
                        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (block_to_check - 1)) + (i * sizeof(DirectoryEntry)), SEEK_SET);
                        write(fs_fd, read_struct, sizeof(DirectoryEntry));
                    }
                }
                if (rename_to != NULL) {
                    // printf("Rename to: %s\n", rename_to);
                    strcpy(read_struct->name, rename_to);
                    // printf("New name is: %s\n", read_struct->name);
                    read_struct->mtime = time(NULL);
                    lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (block_to_check - 1)) + (i * sizeof(DirectoryEntry)), SEEK_SET);
                    write(fs_fd, read_struct, sizeof(DirectoryEntry));
                }
                // free(read_struct);
                close(fs_fd);
                return read_struct;
            }
            // free(read_struct);
        }

        // int max_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
        // if (listEntries){
        //     for (int j = 0; j < max_entries; j++) {
        //         DirectoryEntry* entry = listEntries[j];
        //         // Update timestamp to current system time if it exists
        //         if (entry && entry->name && strcmp(entry->name, filename) == 0) {
        //             return entry;
        //         }
        //     }
        // }
        
        next_block = FAT_TABLE[next_block];
    }
    close(fs_fd);

    return NULL;
}

DirectoryEntry* delete_entry_from_root(const char *filename) {
    int start_block = 1;
    int next_block = start_block;
    int num_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
    int fs_fd = open(FS_NAME, O_RDWR);

    // Iterate through all the root blocks
    while (next_block != 0xFFFF && next_block != 0) {
        // Check if file exists in current block (go through current root block's entries)
        int block_to_check = next_block;
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (block_to_check - 1)), SEEK_SET);
        for (int i = 0; i < num_entries; i++) {
            DirectoryEntry* read_struct = calloc(1, sizeof(DirectoryEntry));
            // fread(&read_struct, sizeof(read_struct), 1, file_ptr);
            read(fs_fd, read_struct, sizeof(DirectoryEntry));
            if (read_struct && strcmp(read_struct->name, filename) == 0) {
                // overwrite with spaces
                // make name empty string
                lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (block_to_check - 1)) + (sizeof(DirectoryEntry) * i), SEEK_SET);
                read_struct->name[0] = '\0';
                write(fs_fd, read_struct, sizeof(DirectoryEntry));

                return read_struct;
            }

            free(read_struct);
        }

        // DirectoryEntry** listEntries = FAT_DATA[block_to_check]; 
        // int max_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
        // if (listEntries){
        //     for (int j = 0; j < max_entries; j++) {
        //         DirectoryEntry* entry = listEntries[j];
        //         if (entry && entry->name && strcmp(entry->name, filename) == 0) {
        //             free(entry->name);
        //             listEntries[j] = NULL;
        //             return;
        //         }
                
        //     }
        // }
        
        next_block = FAT_TABLE[next_block];
    }
    return NULL;
}

int add_entry_to_root(DirectoryEntry* entry) {
    int start_block = 1;

    int last_block = start_block;
    int next_block = start_block;
    int num_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
    int fs_fd = open(FS_NAME, O_RDWR);
    bool found = false;

    // Iterate through all the root blocks until we find final one
    while (next_block != 0xFFFF) {
        last_block = next_block;
        next_block = FAT_TABLE[next_block];
    }
    
    // Check if there is space in the last root block
    int block_to_check = last_block;
    // Seek to access DIRECTORY-BLOCK in the DATA region that contains the file we are looking for
    lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (block_to_check - 1)), SEEK_SET); 

    for (int i = 0; i < num_entries; i++) {
        // Go through this DIRECTORY-BLOCK to find the first empty file space
        // char buffer[sizeof(DirectoryEntry)];
        DirectoryEntry* read_struct = (DirectoryEntry*) calloc(1, sizeof(DirectoryEntry));
        // read(fs_fd, buffer, sizeof(DirectoryEntry))
        // if (memcmp(buffer, " ", sizeof(DirectoryEntry)) == 0) { 
            
        // }
        if (read_struct == NULL) {
            perror("Error allocating memory for read_struct in add_entry_to_root");
            break;
        }

        read(fs_fd, read_struct, sizeof(DirectoryEntry)); //, 1, fs_fd);
        // printf("%d read_struct: %s\n", i, read_struct->name);
        // check if name is empty
        // if (strcmp(read_struct->name, "") == 0) {

        // empty or deleted struct space
        if (read_struct->name[0] == 0 || read_struct->name[0] == '\0') {
            // empty space found
            lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (block_to_check - 1)) + (i * sizeof(DirectoryEntry)), SEEK_SET); 
            write(fs_fd, entry, sizeof(DirectoryEntry));//, 1, fs_fd);
            found = true;
            // printf("%d read_struct: %s\n", i, read_struct->name);
            break;
        }
        
        free(read_struct);
    }

    // DirectoryEntry** listEntries = FAT_DATA[block_to_check];
    // int max_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
    // if (listEntries){
    //     for (int j = 0; j < max_entries; j++) {
    //         DirectoryEntry* entry = listEntries[j];
    //         // Update timestamp to current system time if it exists
    //         if (!entry) {
    //             listEntries[j] = entry;
    //             return 0;
    //         }
    //     }
    // }

    // If no space, add another block to the FAT chain
    if (found) {
        return 0;
    }
    int new_final_block = find_first_free_block();
    // printf("new_final_block: %d\n", new_final_block);
    FAT_TABLE[last_block] = new_final_block;
    FAT_TABLE[new_final_block] = 0xFFFF;
    lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (new_final_block - 1)), SEEK_SET);
    write(fs_fd, entry, sizeof(DirectoryEntry));

    return 0;
}

int touch(const char *filename) {
    if (strlen(filename) > MAX_FILENAME_LENGTH) {
        perror("Error: filename too long");
        return -1;
    }
    if (filename[0] == '\0') {
        perror("Error: filename cannot be empty");
        return -1;
    }
    if (filename[0] == '.' || filename[0] == '/' || filename[0] == '\\' || filename[0] == ':') {
        perror("Error: filename cannot start with ', /, \\, or :'");
        return -1;
    }
    // See if file currently exists by iterating through root directory
    DirectoryEntry* entry = get_entry_from_root(filename, false, NULL);
    if (entry) {
        // printf("Found entry for file in touch\n");
        entry->mtime = time(NULL);
        return 0;
    }

    // Create file if it does not exist
    entry = calloc(1, sizeof(DirectoryEntry));
    strcpy(entry->name, filename);
    entry->size = 0;
    entry->firstBlock = -1; // firstBlock is undefined (null) when size = 0
    entry->type = 1; // TODO: is how do we set type
    entry->perm = 7; // TODO: is how do we set perm
    entry->mtime = time(NULL); // set time to now TODO: is this correct function call?

    // Save pointer at the end of root entries block
    if (add_entry_to_root(entry) == -1) {
        perror("Error: no empty entries in root directory");
        return -1;
    }
    // printf("TOUCH %s\n", entry->name);
    // f_ls(NULL);
    free(entry);
    return 0;
}

int rm(const char *filename) {
    // See if file currently exists by iterating through root directory
    DirectoryEntry* entry = get_entry_from_root(filename, false, NULL);
    if (!entry) {
        perror("Source file does not exist");
        return 0;
    }
    // Delete file from fat table if it does exist
    int block = entry->firstBlock;
    while (block != 0xFFFF && block != 0) {
        int next_block = FAT_TABLE[block];
        FAT_TABLE[block] = 0;
        block = next_block;
    }

    // Delete entry from root directory
    delete_entry_from_root(filename);
    return 0;
}

int mv(const char *source, const char *dest) {
    // TODO: add function to validate name

    // See if file currently exists by iterating through root directory
    DirectoryEntry* entry = get_entry_from_root(source, false, NULL);
    if (!entry) {
        perror("Error: source file does not exist");
        return -1;
    }
    // delete dest file
    rm(dest);
    // rename
    entry = get_entry_from_root(source, false, (char*) dest);

    return 0;
}

// adds data to end of file, given a block number in the file
int append_to_penn_fat(char* data, int block_no, int n, int size) {
    int last_block = block_no;
    int next_block = last_block;
    int fs_fd = open(FS_NAME, O_RDWR);
    // printf("BLOCK NO %i\n", block_no);
    while (next_block != 0xFFFF && next_block != 0) {
        last_block = next_block;
        next_block = FAT_TABLE[next_block];
    }
    // printf("LAST BLOCK %i\n", last_block);
    // write(1, "Here\n", sizeof(char) * strlen("Here\n"));
    // Iterate through data block by block (each block is block_size bytes)
    char* cur_data_block = malloc(BLOCK_SIZE);
    int offset = 0;

    // read what's in last block to determine block text length
    lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (last_block - 1)), SEEK_SET);
    int read_ret = 0;
    int bytes_read = 0;
    if (size != 0) {
        read_ret = read(fs_fd, cur_data_block, BLOCK_SIZE);
        // printf("LEN OF CUR_DATA AFTER READ %d\n", strlen(cur_data_block));
        bytes_read = strlen(cur_data_block);
    }
    if (read_ret < 0) {
        perror("append_to_penn_fat - Error reading data block, bytes_read negative");
        return -1;
    }

    // If there is space remaing (char_rem > 0) in the last block
        //      => THEN write char_rem number of bytes to the block
    
    int bytes_rem = BLOCK_SIZE - bytes_read;
    if (bytes_rem > 0) {   
        int max_size = n < bytes_rem ? n : bytes_rem;
        if (max_size < strlen(data) && max_size > 0) max_size -=  1; // subtract 1 to make room for for null terminator that strndup will add
        cur_data_block = strndup(&data[offset], max_size);
        if (!cur_data_block) {
            perror("append_to_penn_fat - Error copying data block with strndup");
            return -1;
        }
        // printf("|CUR DATA BLOCK: %s|\n", cur_data_block);
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (last_block - 1)) + bytes_read, SEEK_SET);
        write(fs_fd, cur_data_block, sizeof(char) * strlen(cur_data_block));

        if (strlen(data) > bytes_rem && n > bytes_rem) {
            // If we stil have data left to write then set offset and continue with rest of program
            offset = bytes_rem;
        } else {
            // Otherwise we are done and can return
            free(cur_data_block);
            write(fs_fd, "\0", sizeof(char));
            return bytes_rem;
        }
    }
    
    
    while (offset < sizeof(char) * strlen(data)) {
        // printf("offset: %i", offset);
        n = n - offset  >= 1 ? n - offset : 0;
        int max_size = n < BLOCK_SIZE ? n : BLOCK_SIZE;
        if (max_size < strlen(data) && max_size > 0) max_size -=  1; // subtract 1 to make room for for null terminator that strndup will add
        cur_data_block = strndup(&data[offset], max_size);
        // write(1, cur_data_block, sizeof(char) * strlen(cur_data_block));
        if (!cur_data_block) {
            perror("append_to_penn_fat - Error copying data block with strndup");
            return -1;
        }
        offset += sizeof(char) * strlen(cur_data_block);
        // Add new block to FAT chain
        int new_final_block = find_first_free_block();
        FAT_TABLE[last_block] = new_final_block;
        FAT_TABLE[new_final_block] = 0xFFFF;
        last_block = new_final_block;
        // Write data to new block
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (new_final_block - 1)), SEEK_SET);
        write(fs_fd, cur_data_block, sizeof(char) * strlen(cur_data_block));

        if (n - strlen(cur_data_block) < 1) {
            // Add null terminator and break
            // printf("[DEBUG] append_to_penn_fat - adding null terminator\n");
            free(cur_data_block);
            write(fs_fd, "\0", sizeof(char));
            return offset;
        }
    }
    free(cur_data_block);
    // write(1, "while done\n", sizeof(char) * strlen("while done\n"));
    return strlen(data) * sizeof(char);
}

char** read_file_to_string(int fd) {
    // Seek to the end of the file to determine its size
    off_t file_size = lseek(fd, 0, SEEK_END);
    if (file_size == -1) {
        perror("Error seeking to end of file");
        return NULL;
    }
    // Allocate a buffer to hold the file content
    char* buffer = (char*) calloc(1, file_size + 1);  // +1 for null terminator
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
    // write(1, "FILE SIZE\n", strlen("FILE SIZE\n"));
    // printf("%jd\n", file_size);
    // write(1, &file_size, sizeof(file_size));
    if (bytes_read == -1) {
        perror("Error reading file content");
        free(buffer);
        return NULL;
    }
    // write(1, "BYTES READ\n", strlen("BYTES READ\n"));
    // printf("%zd\n", bytes_read);
    // write(1, &bytes_read, sizeof(bytes_read));
    
    // Null-terminate the string
    buffer[bytes_read] = '\0';
    // printf("STRLEN %lu", strlen(buffer));
    // write(1, "BUFFER\n", strlen("BUFFER\n"));
    // write(1, buffer, file_size + 1);
    char** array = (char**)malloc(2 * sizeof(char*));
    array[0] = buffer;
    array[1] = (char*)malloc(sizeof(int));
    sprintf(array[1], "%ld", file_size + 1);
    return array;
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

    // both in fat
    
    int fs_fd = open(FS_NAME, O_RDWR);

    // find source file
    // open file
    DirectoryEntry* entry = get_entry_from_root(source, true, NULL);

    if (entry == NULL) {
        perror("Error: source file does not exist");
        return -1;
    }

    DirectoryEntry* d_entry = get_entry_from_root(dest, true, NULL);
    if (d_entry) {
        delete_from_penn_fat(dest);
        // delete_entry_from_root(dest);
    }
    // create new file with name
    touch(dest);

    d_entry = get_entry_from_root(dest, true, NULL);

    int* chain = get_fat_chain(entry->firstBlock);
    int w_fd = f_open((char *) dest, F_WRITE);
    int chars = entry->size;
    int size = 0;
    for (int i = 0; i < NUM_FAT_ENTRIES; i++) {
        if (!chain[i]) {
            break;
        }

        if (chars <= 0) {
            break;
        }

        if (chars >= BLOCK_SIZE) {
            size = BLOCK_SIZE;
            chars -= BLOCK_SIZE;
        } else {
            size = chars;
            chars = 0;
        }

        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (chain[i] - 1)), SEEK_SET);
        char* txt = malloc(size);
        read(fs_fd, txt, size);
        // write to file
        f_write(w_fd, txt, size);

        // if (chars <= 0) {
        //     break;
        // }
        // lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (chain[i] - 1)), SEEK_SET);
        // char* txt = NULL;
        // int size = 0;
        // // either size of block or size of file text
        // if (chars >= BLOCK_SIZE) {
        //     size = BLOCK_SIZE;
        //     chars -= BLOCK_SIZE;
        // } else {
        //     size = chars;
        //     chars = 0;
        // }

        // txt = calloc(1, sizeof(char) * size);
        // read(fs_fd, txt, sizeof(char) * size);
        // if (i == 0) {
        //     f_write(w_fd, txt, sizeof(char) * size);
        //     write(1, "WRITING\n", sizeof(char) * strlen("WRITING\n"));
        // } else {
        //     append_to_penn_fat(txt, entry->firstBlock, sizeof(char) * size);
        // }
    }
    f_close(w_fd);
    // write(1, "writing to root\n", sizeof(char) * strlen("writing to root\n"));
    d_entry->size = entry->size;
    // printf("CHARS %i\n", d_entry->size);
    write_entry_to_root(d_entry);
    return 0;
}

int cp_from_h(const char *source, const char *dest) {
    // copy from host file

    // open host file to read
    int h_fd = open(source, O_RDWR);
    if (h_fd == -1) {
        perror("Error opening file");
        exit(1);
    }
    int fs_fd = open(FS_NAME, O_RDWR);

    // check if file exists in fat, remove if it does
    DirectoryEntry* entry = get_entry_from_root(dest, true, NULL);
    // write(1, "entry checked\n", sizeof(char)*strlen("entry checked\n"));
    if (entry) {
        delete_from_penn_fat(dest);
        // delete_entry_from_root(dest);
    }
    // write(1, "entry checked\n", sizeof(char)*strlen("entry checked\n"));
    // create new file with name
    touch(dest);
    entry = get_entry_from_root(dest, true, NULL);
    // int w_fd = f_open((char *) dest, F_WRITE);
    // TODO: update directory entry with the first block of file
    char** array = read_file_to_string(h_fd);
    char* txt = array[0];
    int size = atoi(array[1]);
    // write(STDOUT_FILENO, "CP\n", sizeof(char) * strlen("CP\n"));
    // write(1, txt, size);
    int rem_size = size;
    int offset = 0;
    int rem = 0;
    int block = entry->firstBlock;
    while (offset < size) {
        // if (size == BLOCK_SIZE + 1 && offset == size - 1) {
        //     break;
        // }

        // if (size == BLOCK_SIZE + 1) {
        //     rem = BLOCK_SIZE;
        // }
        if (rem_size > BLOCK_SIZE) {
            rem = BLOCK_SIZE;
        } else {
            rem = rem_size;
        }
        // printf("VALS %i %i\n", size, rem);
        // write block sized portion
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (block - 1)), SEEK_SET);
        write(fs_fd, &txt[offset], rem);
        offset += rem;
        if (rem == BLOCK_SIZE) {
            // add new block if needed
            int temp = find_first_free_block();
            FAT_TABLE[block] = temp;
            FAT_TABLE[temp] = 0xFFFF;
            block = temp;
        }
        rem_size -= rem;
    }
    entry->size = size;
    
    write_entry_to_root(entry);

    // write(STDOUT_FILENO, "CP\n", sizeof(char) * strlen("CP\n"));
    // write(1, txt, size);
    // f_write(w_fd, txt, size);
    // sizeof(char) * strlen(txt)
    // int i = find_first_free_block();
    // append_to_penn_fat(txt, i, BLOCK_SIZE);
    // f_close(w_fd);
    // entry->size = strlen(txt);
    // write_entry_to_root(entry);
    return 0;
}

// copying from fat to host
int cp_to_h(const char *source, const char *dest) {
    // write(1, "helper\n", sizeof(char) * strlen("helper\n"));
    // open fat binary
    int fs_fd = open(FS_NAME, O_RDWR);
    if (fs_fd == -1) {
        perror("Error opening file system image");
        exit(1);
    }

    // write(1, "fd got\n", sizeof(char) * strlen("fd got\n"));
    // get directory entry for file
    DirectoryEntry* entry = get_entry_from_root(source, true, NULL);
    if (!entry) {
        perror("Error: source file does not exist");
        return -1;
    }

    // write(1, "entry\n", sizeof(char) * strlen("entry\n"));

    // file chain
    int* chain = get_fat_chain(entry->firstBlock);
    // write(1, "chain\n", sizeof(char) * strlen("chain\n"));

    // open host file
    int h_fd = open(dest, O_RDWR | O_CREAT, 0666);
    int chars = entry->size;
    int size = 0;
    // write(1, "host opened\n", sizeof(char) * strlen("host opened\n"));
    for (int i = 0; i < NUM_FAT_ENTRIES; i++) {
        // write(1, chain[i], sizeof(int));
        if (!chain[i] || chain[i] == 0) {
            break;
        }
        if (chars <= 0) {
            break;
        }

        if (chars >= BLOCK_SIZE) {
            size = BLOCK_SIZE;
            chars -= BLOCK_SIZE;
        } else {
            size = chars;
            chars = 0;
        }

        // copy block into host file
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (chain[i] - 1)), SEEK_SET);
        char* txt = malloc(size);
        read(fs_fd, txt, size);
        write(h_fd, txt, size);

        // // copy block into host file
        // char* txt = NULL;
        // int size = 0;
        // // either size of block or size of file text
        // if (chars >= BLOCK_SIZE) {
        //     size = BLOCK_SIZE;
        //     chars -= BLOCK_SIZE;
        // } else {
        //     size = chars;
        //     chars = 0;
        // }

        // txt = malloc(sizeof(char) * size);
        // lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (chain[i] - 1)), SEEK_SET);
        // read(fs_fd, txt, sizeof(char) * size);
        // printf("%s\n", txt);
        // write(h_fd, txt, sizeof(char) * size);
        // printf("SIZE %i\n", size);

        
    }
    free(chain);
    close(fs_fd);
    close(h_fd);
    return 0;
}

int f_lseek(int fd, int offset, int whence) {

    off_t new_position;

    int blocks = offset / BLOCK_SIZE;
    int rem = offset % BLOCK_SIZE;

    // find entry name
    FDTEntry* fdtEntry = FDT[fd];
    int current_position = fdtEntry->offset;

    DirectoryEntry* entry = get_entry_from_root(fdtEntry->name, false, NULL);
    int* chain = get_fat_chain(entry->firstBlock);

    // current offset from beginning of root directory
    int root_offset = current_position - TABLE_REGION_SIZE;
    // block number of current file pointer
    int curr_block = root_offset / BLOCK_SIZE;
    // offset value of end of current block
    int end_block = TABLE_REGION_SIZE + ((curr_block + 1)*BLOCK_SIZE);
    // number of bytes between file pointer and end of block
    int block_leftover = end_block - current_position;
    
    int block = entry->firstBlock;
    int last_block = block;
    switch (whence) {
        case F_SEEK_SET:
            new_position = TABLE_REGION_SIZE + (BLOCK_SIZE * (chain[blocks]-1)) + rem;
            break;
        case F_SEEK_CUR:
            
            // shift is within same block
            if (offset < block_leftover) {
                new_position = current_position + offset;
            } else {
                // shift to different block
                offset -= block_leftover;
                int num_blocks = offset / BLOCK_SIZE;
                int rem_block = offset % BLOCK_SIZE;
                new_position = TABLE_REGION_SIZE + (BLOCK_SIZE * chain[num_blocks + curr_block]) + rem_block;
            }
            
            break;
        case F_SEEK_END:
            // TODO: need to actually store the current position somewhere
            while (block != 0xFFFF) {
                last_block = block;
                block = FAT_TABLE[block];
            }
            // last_block is the last file block
            new_position = TABLE_REGION_SIZE + (BLOCK_SIZE * last_block) + offset;

            // printf("New file offset: %ld\n", (long)new_position);
            break;  // Success
        default:
            fprintf(stderr, "Invalid 'whence' parameter\n");
            return -1;  // Error
    }

    // update file pointer position that's stored to = new_position
    fdtEntry->offset = new_position;
    return 0;
}

void f_ls(const char *filename) {
    // iterate through directory entries
    // print file names 

    // open file
    // FILE * file_ptr = fopen(FS_NAME, "r");
    int fs_fd = open(FS_NAME, O_RDWR);
    // if (fs_fd == -1) {
    //     perror("Error opening file system image");
    //     exit(1);
    // }

    // get root directories
    int* root_chain = get_fat_chain(1);
    // max number of directory entry structs in the block
    int num_entries = BLOCK_SIZE / sizeof(DirectoryEntry);

    for (int i = 0; i < NUM_FAT_ENTRIES; i++) {
        if (!root_chain[i]) {
            break;
        }
        // printf("%i\n", root_chain[i]);
        // position file pointer
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE* (root_chain[i] - 1)), SEEK_SET);
        for (int i = 0; i < num_entries; i++) {
            DirectoryEntry* read_struct = calloc(1, sizeof(DirectoryEntry));
            // fread(&read_struct, sizeof(read_struct), 1, file_ptr);
            read(fs_fd, read_struct, sizeof(DirectoryEntry));
            // directory entry was not deleted (non empty name)
            // if (read_struct == NULL) {
            //     printf("NULL found\n");
            //     break;
            // }
            // printf("%s", read_struct->name);
            // if (strcmp(read_struct->name, "") != 0) {
            struct tm *localTime = localtime(&read_struct->mtime);
            char formattedTime[50];
            // strftime(formattedTime, sizeof(formattedTime), "%B %d %H:%M", read_struct->mtime);
            strftime(formattedTime, sizeof(formattedTime), "%b %d %H:%M", localTime);

            // perm string
            char* perm = NULL;
            if (read_struct->perm == 0) {
                perm = "---";
            } else if (read_struct->perm == 2) {
                perm = "--w";
            } else if (read_struct->perm == 4) {
                perm = "-r-";
            } else if (read_struct->perm == 5) {
                perm = "xr-";
            } else if (read_struct->perm == 6) {
                perm = "-rw";
            } else if (read_struct->perm == 7) {
                perm = "xrw";
            }
            if (read_struct->name[0] != 0 && read_struct->name[0] != '\0') {
                printf("%hu %s %u %s %s\n", read_struct->firstBlock, perm, 
               read_struct->size, formattedTime, read_struct->name);
            }
            // (long long) read_struct->mtime
        }
    }
    free(root_chain);
    close(fs_fd);
}

int cat(const char **files, int num_files, const char *output_file, int append) {
    // Note should support:
    // cat FILE ... [ -w OUTPUT_FILE ]: (set output_file to null if stdout) Concatenates the files and prints them to stdout by default, or overwrites OUTPUT_FILE. If OUTPUT_FILE does not exist, it will be created. (Same for OUTPUT_FILE in the commands below.)
    // cat FILE ... [ -a OUTPUT_FILE ]: (set output_file to null if stdout) Concatenates the files and prints them to stdout by default, or appends to OUTPUT_FILE.
    // cat -w OUTPUT_FILE: (set num_files to 0) Reads from the terminal and overwrites OUTPUT_FILE.
    // cat -a OUTPUT_FILE: (set num_files to 0) Reads from the terminal and appends to OUTPUT_FILE.
    
    // Step 1: Get input data (as a string)
    char* data = calloc(1, sizeof(char) * BLOCK_SIZE * NUM_FAT_ENTRIES);
    int chars_added = 0;
    if (num_files > 0) {
        // Concatenate files
        for (int i = 0; i < num_files; i++) {
            // Get directory entry for file
            DirectoryEntry* entry = get_entry_from_root(files[i], true, NULL);
            if (!entry) {
                perror("Error: source file does not exist");
                return -1;
            }
            // Get new file data and append it to current data
            chars_added += strcat_data(data, entry->firstBlock);
            // write(1, "strcat called\n", sizeof(char) * strlen("strcat called\n"));
        }
    } else {
        // Read from terminal
        int num_bytes = read(STDIN_FILENO, data, sizeof(char) * BLOCK_SIZE * NUM_FAT_ENTRIES);
        if (num_bytes == -1) {
            perror("Error reading from terminal");
            return -1;
        }
        chars_added += num_bytes;
    }
    uint32_t stored_size = 0;
    // Step 2: Output data
    if (output_file) {
        // Write to file
        DirectoryEntry* entry = get_entry_from_root(output_file, true,   NULL);
        if (append) {
            if (!entry) {
                // Create file if it does not exist
                // printf(" -a start file create");
                if (touch(output_file) < 0) {
                    perror("cat - Error creating file using touch");
                    return -1;
                }
                // printf(" -a start get entry");
                entry = get_entry_from_root(output_file, true, NULL);
                if (!entry) {
                    perror("cat - Error creating file using touch (entry still null)");
                    return -1;
                }
            }
            stored_size = entry->size;
        } else {
            // printf(" -w start");
            if (entry) {
                // Delete old file from penn fat if it exists and recreate it
                // printf(" -w delete");
                if (delete_from_penn_fat(output_file) < 0) {
                    perror("cat - Error deleting file cannot write properly, exiting");
                    return -1;
                }
            }
            // printf(" -w touch");
            if (touch(output_file) < 0) {
                perror("cat - Error creating file using touch");
                return -1;
            }
        }
        entry = get_entry_from_root(output_file, true, NULL); // Update entry value
        append_to_penn_fat(data, entry->firstBlock, chars_added, entry->size);
        entry->size = stored_size + chars_added;
        write_entry_to_root(entry);
    } else {
        // Write to stdout
        int num_bytes = write(STDOUT_FILENO, data, sizeof(char) * strlen(data));
        if (num_bytes == -1) {
            perror("Error writing to stdout");
            return -1;
        }
    }
    return 0;
}

/* F_* Function definitions */
int f_open(char *fname, int mode) {
    if(strlen(fname) > MAX_FILENAME_LENGTH) {
        perror("Error: filename too long");
        return -1;
    }
    // TODO: check name meets https://www.ibm.com/docs/en/zos/3.1.0?topic=locales-posix-portable-file-name-character-set

    // Get next_descriptor that's free and add entry to FDT
    int next_descriptor = 0;
    for (int i = 0; i < NUM_FAT_ENTRIES; i++) {
        if (!FDT[i]) {
            next_descriptor = i;
            break;
        }
    }
    FDTEntry* fdtEntry = calloc(1, sizeof(FDTEntry));
    fdtEntry->mode = mode;
    strcpy(fdtEntry->name, fname);
    
    fdtEntry->offset = 0;
    FDT[next_descriptor] = fdtEntry;
    // printf("[DEBUG] Created file descriptor %d, name: %s\n", next_descriptor, FDT[next_descriptor]->name);

    return next_descriptor;
}

int f_read(int fd, int n, char *buf) {
    // Check if file descriptor is valid
    if (fd < 0 || fd >= NUM_FAT_ENTRIES) {
        perror("Error: invalid file descriptor");
        return -1;
    }
    // Check if file is open
    if (!FDT[fd]) {
        perror("Error: file is not open");
        return -1;
    }
    // Check if file is open for reading
    if (FDT[fd]->mode != F_READ) {
        perror("Error: file is not open for reading");
        return -1;
    }

    // Get directory entry for file
    DirectoryEntry* entry = get_entry_from_root(FDT[fd]->name, false, NULL);
    if (!entry) {
        perror("Error: source file does not exist");
        return -1;
    }

    // Get file data
    char* data = calloc(1, sizeof(char) * BLOCK_SIZE * NUM_FAT_ENTRIES);
    strcat_data(data, entry->firstBlock);

    // Copy data to buffer
    strncpy(buf, data, n);
    // write(STDOUT_FILENO, data, sizeof(char) * strlen(data));
    free(data);
    // If we reach EOF return 0
    if (strlen(buf) * sizeof(char) < n) {
        return 0;
    }
    return n;
}

int f_write(int fd, const char *str, int n) {
    // printf("[DEBUG] F_WRITE fd: %d, str: %s, n: %d\n", fd, str, n);
    // Check if file descriptor is valid
    if (fd < 0 || fd >= NUM_FAT_ENTRIES) {
        perror("Error: invalid file descriptor");
        return -1;
    }
    // Check if file is open
    if (!FDT[fd]) {
        perror("Error: file is not open");
        return -1;
    }
    // Check if file is open for writing
    if (FDT[fd]->mode != F_WRITE && FDT[fd]->mode != F_APPEND) {
        perror("Error: file is not open for writin or appending");
        return -1;
    }
    // Get directory entry for file and write
    DirectoryEntry* entry = get_entry_from_root(FDT[fd]->name, true, NULL);
    // printf("PARAM: %s\n", str);
    // write(STDOUT_FILENO, "PARAM\n", sizeof(char) * strlen("PARAM\n"));
    // write(1, str, sizeof(char) * strlen(str));
    char* data = calloc(1, sizeof(char) * BLOCK_SIZE * NUM_FAT_ENTRIES);
    uint32_t stored_size = 0;
    if (FDT[fd]->mode == F_APPEND) {
        if (!entry) {
            // Create file if it does not exist
            if (touch(FDT[fd]->name) < 0) {
                perror("f_write - Error creating file using touch");
                return -1;
            }
            entry = get_entry_from_root(FDT[fd]->name, true, NULL);
            if (!entry) {
                perror("f_write - Error creating file using touch (entry still null)");
                return -1;
            }
        }
        // Get file data
        strcat_data(data, entry->firstBlock);
        // Append new data to file data
        strncat(data, str, n / sizeof(char));
    } else {
        if (entry) {
            // Delete old file from penn fat if it exists and recreate it
            stored_size = entry->size;
            if (delete_from_penn_fat(FDT[fd]->name) < 0) {
                perror("f_write - Error deleting file cannot write properly, exiting");
                return -1;
            }
        }
        if (touch(FDT[fd]->name) < 0) {
            perror("f_write - Error creating file using touch");
            return -1;
        }
        // printf("f_write ls\n");
        // f_ls(NULL);
        // Copy str into data
        strncpy(data, str, n / sizeof(char));
    }
    // Write data to file
    entry = get_entry_from_root(FDT[fd]->name, true, NULL);
    if (!entry) {
        perror("f_write - Error finding file entry before append");
        return -1;
    }
    // write(STDOUT_FILENO, "got entry\n", sizeof(char) * strlen("got entry\n"));
    // Get first block
    int chars_added = append_to_penn_fat(data, entry->firstBlock, n, entry->size);
    // printf("DATA %s", data);
    // write(STDOUT_FILENO, "DATA\n", sizeof(char) * strlen("DATA\n"));
    // write(1, data, sizeof(char) * strlen(data));
    // write(STDOUT_FILENO, "chars added\n", sizeof(char) * strlen("chars added\n"));
    if (chars_added < 0) {
        perror("f_write - Error appending to penn fat");
        return -1;
    }
    FDT[fd]->offset += chars_added; // increment offset by n
    entry->size = stored_size + chars_added;
    write_entry_to_root(entry);
    // write(STDOUT_FILENO, "offset\n", sizeof(char) * strlen("offset\n"));
    free(data);
    // printf("Data freed\n");
    return chars_added;
}

// once an entry has been updated, rewrites entry to same place in root
int write_entry_to_root(DirectoryEntry* entry) {
    // find entry
    int* root_chain = get_fat_chain(1);

    int fs_fd = open(FS_NAME, O_RDWR);

    // Iterate through all the root blocks
    int num_entries = BLOCK_SIZE / sizeof(DirectoryEntry);

    for (int i = 0; i < NUM_FAT_ENTRIES; i++) {
        if (!root_chain[i]) {
            break;
        }
        int block_to_check = root_chain[i];

        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (block_to_check - 1)), SEEK_SET);

        for (int i = 0; i < num_entries; i++) {
            DirectoryEntry* read_struct = calloc(1, sizeof(DirectoryEntry));
            read(fs_fd, read_struct, sizeof(DirectoryEntry));
            // if wanted file has been found
            if (read_struct && strcmp(read_struct->name, entry->name) == 0) {
                lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (block_to_check - 1)) + (i * sizeof(DirectoryEntry)), SEEK_SET);
                write(fs_fd, entry, sizeof(DirectoryEntry));
                free(read_struct);
                return 0;
            }
        }
    }
    return 0;
}

int f_close(int fd) {
    // Check if file descriptor is valid
    if (fd < 0 || fd >= NUM_FAT_ENTRIES) {
        perror("Error: invalid file descriptor");
        return -1;
    }
    // Check if file is open
    if (!FDT[fd]) {
        perror("Error: file is not open");
        return -1;
    }
    // Free FDT entry
    free(FDT[fd]);
    FDT[fd] = NULL;
    return 0;
}

int f_unlink(const char *fname) {
    // Should should not be able to delete a file that is in use by another process.
    // Should not be able to delete a file that is open - check to see if it's open
    for (int i = 0; i < NUM_FAT_ENTRIES; i++) {
        if (FDT[i] && strcmp(FDT[i]->name, fname) == 0) {
            perror("f_unlink - Error: file is open");
            return -1;
        }
    }

    // See if file currently exists by iterating through root directory
    DirectoryEntry* entry = get_entry_from_root(fname, true, NULL);
    if (!entry) {
        perror("f_unlink - Error: source file does not have a directory entry");
        return -1;
    }

    // Delete file from fat table and root directory
    delete_from_penn_fat(fname);
    return 0;
}

int chmod(const char *fname, uint8_t new_perm) {
    // See if file currently exists by iterating through root directory
    DirectoryEntry* entry = get_entry_from_root(fname, true, NULL);
    if (!entry) {
        perror("chmod - Error: source file does not have a directory entry");
        return -1;
    }
    entry->perm = new_perm;
    entry->mtime = time(NULL);
    msync(entry, sizeof(DirectoryEntry), MS_SYNC);
    return 0;
}