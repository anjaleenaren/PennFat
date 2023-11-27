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
        default:
            BLOCK_SIZE = 4096;
            break;
    }

    // Calculate FAT and file system size
    FAT_SIZE = BLOCK_SIZE * blocks_in_fat;
    NUM_FAT_ENTRIES = (BLOCK_SIZE * blocks_in_fat) / 2;
    TABLE_REGION_SIZE = sizeof(uint16_t) * NUM_FAT_ENTRIES;
    DATA_REGION_SIZE = BLOCK_SIZE * (NUM_FAT_ENTRIES - 1);

    // Set the file system size
    if (ftruncate(fs_fd, FAT_SIZE) == -1) {
        perror("Error setting file system size");
        close(fs_fd);
        exit(1);
    }

    // Mmap for FAT table region
    FAT_TABLE = mmap(NULL, TABLE_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    if (FAT_TABLE == MAP_FAILED) {
        perror("Error mmapping the directory entries");
        close(fs_fd);
        exit(1);
    }

    // Initialize the FAT_TABLE
    FAT_TABLE[0] = (blocks_in_fat << 8) | block_size_config; //MSB = blocks_in_fat, LSB = block_size_config
    for (int i = 1; i < NUM_FAT_ENTRIES; i++) {
        FAT_TABLE[i] = 0;
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
    FAT_DATA = mmap(NULL, DATA_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, TABLE_REGION_SIZE);
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

    // Free directory entries TODO
    // 1. Start at fat table index 1, and follow pointers to get location of all data blocks
    int* root_chain = get_fat_chain(1);
    // 2. Free all data blocks
    for (int i = 0; i < NUM_FAT_ENTRIES; i++) {
        if (!root_chain[i]) {
            break;
        }
        DirectoryEntry** listEntries = FAT_DATA[root_chain[i]];
        int max_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
        if (listEntries){
            for (int j = 0; j < max_entries; j++) {
                DirectoryEntry* entry = listEntries[j];
                if (entry) {
                    free(entry);
                }
            }
        }
    }
    // 3. Free root_chain
    free(root_chain);

    // Unmap the memory-mapped region
    if (munmap(FAT_TABLE, TABLE_REGION_SIZE) == -1) {
        perror("Error unmapping file system - table");
        close(fs_fd);
        exit(1);
    }
    if (munmap(FAT_DATA, DATA_REGION_SIZE) == -1) {
        perror("Error unmapping file system - data");
        close(fs_fd);
        exit(1);
    }

    close(fs_fd);
}

int* get_fat_chain(int start_index) {
    int* fat_chain = malloc(sizeof(int) * NUM_FAT_ENTRIES);
    int i = 0;
    int start_block = FAT_TABLE[start_index];
    int next_block = start_block;
    while (next_block != 0XFFFF) {
        fat_chain[i] = next_block;
        next_block = FAT_TABLE[next_block];
        i++;
    }
    return fat_chain;
}

void strcat_data(char* data, int start_index) {
    int i = 0;
    int start_block = FAT_TABLE[start_index];
    int next_block = start_block;
    while (next_block != 0xFFFF && next_block != 0) {
        char* cur_data = FAT_DATA[next_block];
        if (cur_data) {
            strcat(data, cur_data);
        }
        
        next_block = FAT_TABLE[next_block];
        i++;
    }
}

int find_first_free_block() {
    for (int i = 1; i < NUM_FAT_ENTRIES; i++) {
        if (FAT_TABLE[i] == 0) {
            return i;
        }
    }
    return -1;
}

DirectoryEntry* get_entry_from_root(const char *filename) {
    int i = 0;
    int start_block = FAT_TABLE[1];
    int next_block = start_block;
    // Iterate through all the root blocks
    while (next_block != 0XFFFF && next_block != 0) {
        { // Check if file exists in current block (go through current root block's entries)
            int block_to_check = next_block;
            DirectoryEntry** listEntries = FAT_DATA[block_to_check]; 
            int max_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
            if (listEntries){
                for (int j = 0; j < max_entries; j++) {
                    DirectoryEntry* entry = listEntries[j];
                    // Update timestamp to current system time if it exists
                    if (entry && entry->name && strcmp(entry->name, filename) == 0) {
                        return entry;
                    }
                }
            }
        }
        next_block = FAT_TABLE[next_block];
        i++;
    }

    return NULL;
}

DirectoryEntry* delete_entry_from_root(const char *filename) {
    int i = 0;
    int start_block = FAT_TABLE[1];
    int next_block = start_block;

    // Iterate through all the root blocks
    while (next_block != 0XFFFF && next_block != 0) {
        { // Check if file exists in current block (go through current root block's entries)
            int block_to_check = next_block;
            DirectoryEntry** listEntries = FAT_DATA[block_to_check]; 
            int max_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
            if (listEntries){
                for (int j = 0; j < max_entries; j++) {
                    DirectoryEntry* entry = listEntries[j];
                    if (entry && entry->name && strcmp(entry->name, filename) == 0) {
                        free(entry->name);
                        listEntries[j] = NULL;
                        return;
                    }
                    
                }
            }
        }
        next_block = FAT_TABLE[next_block];
        i++;
    }
    return NULL;
}

int add_entry_to_root(DirectoryEntry* entry) {
    int i = 0;
    int start_block = FAT_TABLE[1];
    int last_block = start_block;
    int next_block = start_block;
    // Iterate through all the root blocks until we find final one
    while (next_block != 0XFFFF) {
        last_block = next_block;
        next_block = FAT_TABLE[next_block];
        i++;
    }
    
    // Check if there is space in the last root block
    int block_to_check = last_block;
    DirectoryEntry** listEntries = FAT_DATA[block_to_check];
    int max_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
    if (listEntries){
        for (int j = 0; j < max_entries; j++) {
            DirectoryEntry* entry = listEntries[j];
            // Update timestamp to current system time if it exists
            if (!entry) {
                listEntries[j] = entry;
                return 0;
            }
        }
    }

    // If no space, add another block to the FAT chain
    int new_final_block = find_first_free_block();
    FAT_TABLE[last_block] = new_final_block;
    FAT_TABLE[new_final_block] = 0XFFFF;
    DirectoryEntry** listEntries = FAT_DATA[new_final_block];
    listEntries[0] = entry;

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
    DirectoryEntry* entry = get_entry_from_root(filename);
    if (entry) {
        entry->mtime = time(NULL);
        return 0;
    }

    // Create file if it does not exist
    DirectoryEntry *entry = malloc(sizeof(DirectoryEntry));
    strcpy(entry->name, filename);
    entry->size = 0;
    entry->firstBlock = NULL; // firstBlock is undefined (null) when size = 0
    entry->type = 1; // TODO: is how do we set type
    entry->perm = 7; // TODO: is how do we set perm
    entry->mtime = time(NULL); // set time to now TODO: is this correct funciton call?
    
    // Save pointer at the end of root entries block
    if (add_entry_to_root(entry) == -1) {
        perror("Error: no empty entries in root directory");
        return -1;
    }
}

int mv(const char *source, const char *dest) {
    // TODO: add function to validate name

    // See if file currently exists by iterating through root directory
    DirectoryEntry* entry = get_entry_from_root(source);
    if (!entry) {
        perror("Error: source file does not exist");
        return -1;
    }

    // Rename file if it does exist
    strcpy(entry->name, dest);
    entry->mtime = time(NULL);
    return 0;
}

int rm(const char *filename) {
    // See if file currently exists by iterating through root directory
    DirectoryEntry* entry = get_entry_from_root(filename);
    if (!entry) {
        perror("Error: source file does not exist");
        return -1;
    }

    // Delete file from fat table if it does exist
    int block = entry->firstBlock;
    while (block != 0XFFFF && block != 0) {
        int next_block = FAT_TABLE[block];
        FAT_TABLE[block] = 0;
        block = next_block;
    }

    // Delete entry from root directory
    delete_entry_from_root(filename);
}

// TODO: parse args in shell
int cp(const char *source, const char *dest, int s_host, int d_host) {
    // if d_host, cp_to_h
    if (d_host) {
        cp_to_h(source, dest);
    } else if (s_host) {
        cp_from_h(source, dest);
    } else {
        cp_helper(source, dest);
    }
}

int cp_helper(const char *source, const char *dest) {

    // both in fat
    DirectoryEntry* entry = get_entry_from_root(dest);
    if (entry) {
        delete_entry_from_root(dest);
    }
    // create new file with name
    touch(dest);

    // find source file
    // open file
    free(entry);
    DirectoryEntry* entry = get_entry_from_root(source);

    int* chain = get_fat_chain(entry->firstBlock);

    for (int i = 0; i < NUM_FAT_ENTRIES; i++) {
        if (!chain[i]) {
            break;
        }
        char* txt = FAT_DATA[chain[i]];
        // TODO: write to file
    }
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
    DirectoryEntry* entry = get_entry_from_root(dest);
    if (entry) {
        delete_entry_from_root(dest);
    }
    // create new file with name
    touch(dest);
    // TODO: write to file  
}

// copying from fat to host
int cp_to_h(const char *source, const char *dest) {
    // open fat binary
    int fs_fd = open(fs_name, O_RDWR);
    if (fs_fd == -1) {
        perror("Error opening file system image");
        exit(1);
    }

    // get directory entry for file
    DirectoryEntry* entry = get_entry_from_root(source);
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
        char* txt = FAT_DATA[chain[i]];
        write(h_fd, txt, sizeof(char) * strlen(txt));
    }
    free(chain);
    close(fs_fd);
    close(h_fd);
}

void f_ls(const char *filename) {
    // iterate through directory entries
    // print file names 

    // open file
    int fs_fd = open(fs_name, O_RDWR);
    if (fs_fd == -1) {
        perror("Error opening file system image");
        exit(1);
    }

    // get root directories
    int* root_chain = get_fat_chain(1);

    for (int i = 0; i < NUM_FAT_ENTRIES; i++) {
        if (!root_chain[i]) {
            break;
        }
        DirectoryEntry** listEntries = FAT_DATA[root_chain[i]];
        int max_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
        if (listEntries){
            for (int j = 0; j < max_entries; j++) {
                DirectoryEntry* entry = listEntries[j];
                if (entry) {
                    write(STDOUT_FILENO, entry->name, sizeof(char)*strlen(entry->name));
                }
            }
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
    char* data = malloc(sizeof(char) * BLOCK_SIZE * NUM_FAT_ENTRIES);
    if (num_files > 0) {
        // Concatenate files
        for (int i = 0; i < num_files; i++) {
            // Get directory entry for file
            DirectoryEntry* entry = get_entry_from_root(files[i]);
            if (!entry) {
                perror("Error: source file does not exist");
                return -1;
            }
            // Get new file data and append it to current data
            strcat_data(data, entry->firstBlock);
        }
    } else {
        // Read from terminal
        int num_bytes = read(STDIN_FILENO, data, sizeof(char) * BLOCK_SIZE * NUM_FAT_ENTRIES);
        if (num_bytes == -1) {
            perror("Error reading from terminal");
            return -1;
        }
    }

    // Step 2: Output data
    if (output_file) {
        // Write to file
        DirectoryEntry* entry = get_entry_from_root(output_file);
        if (!entry) {
            // Create file if it does not exist
            if (touch(output_file) < 0) {
                perror("cat - Error creating file using touch");
                return -1;
            }
            entry = get_entry_from_root(output_file);
            if (!entry) {
                perror("cat - Error creating file using touch (entry still null)");
                return -1;
            }
        }

        if (!append && entry) { // Delete old file from penn fat if it exists and we are trying to overwrite
            if (delete_from_penn_fat(output_file) < 0) {
                perror("cat - Error deleting file cannot write properly, exiting");
                return -1;
            }
        }
        // Find last block in file (assumes delete_from_penn_fat removes all old blocks)
        int last_block = entry->firstBlock;
        int next_block = last_block;
        while (next_block != 0XFFFF) {
            last_block = next_block;
            next_block = FAT_TABLE[next_block];
        }
        // Iterate through data block by block (each block is block_size bytes)
        char* cur_data_block;
        int offset = 0;
        while (offset < sizeof(char) * strlen(data)) {
            cur_data_block = strndup(data[offset], BLOCK_SIZE);
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
            FAT_DATA[new_final_block] = cur_data_block; 
            free(cur_data_block);
        }
    } else {
        // Write to stdout
        int num_bytes = write(STDOUT_FILENO, data, sizeof(char) * strlen(data));
        if (num_bytes == -1) {
            perror("Error writing to stdout");
            return -1;
        }
    }
}
