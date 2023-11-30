#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdbool.h>
#include "pennfat.h"

#define MAX_FAT_ENTRIES 65534 // Maximum for FAT16

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
        default:
            BLOCK_SIZE = 4096;
            break;
    }

    FS_NAME = fs_name; // save name

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

    // Initialize the file descriptor table (FDT)
    FDT = malloc(sizeof(FDTEntry*) * NUM_FAT_ENTRIES);

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

    // Initialize the root directory
    FAT_TABLE[1] = 0xFFFF; // First block of root directory is FFFF to signal it's the end

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
    // FAT_DATA = mmap(NULL, DATA_REGION_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, TABLE_REGION_SIZE);
    // if (FAT_DATA == MAP_FAILED) {
    //     perror("Error mapping file system");
    //     close(fs_fd);
    //     exit(1);
    // }

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
    int fs_fd = open(FS_NAME, O_RDONLY);
    int start_block = FAT_TABLE[start_index];
    int next_block = start_block;
    while (next_block != 0xFFFF && next_block != 0) {
        char* cur_data = malloc(sizeof(char)*BLOCK_SIZE);
        read(fs_fd, cur_data, sizeof(char)*BLOCK_SIZE);
        if (cur_data) {
            strcat(data, cur_data);
        }
        
        next_block = FAT_TABLE[next_block];
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

int delete_from_penn_fat(const char *filename) {
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
    return 0;
}

DirectoryEntry* get_entry_from_root(const char *filename) {
    int start_block = FAT_TABLE[1];
    int next_block = start_block;
    FILE * file_ptr = fopen(FS_NAME, "r");
    int fs_fd = open(FS_NAME, O_RDONLY);
    // Iterate through all the root blocks

    int num_entries = BLOCK_SIZE / sizeof(DirectoryEntry);

    while (next_block != 0XFFFF && next_block != 0) {
        // Check if file exists in current block (go through current root block's entries)
        int block_to_check = next_block;
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (block_to_check - 1)), SEEK_SET);
        // DirectoryEntry** listEntries = FAT_DATA[block_to_check]; 
        
        for (int i = 0; i < num_entries; i++) {
            DirectoryEntry* read_struct = malloc(sizeof(DirectoryEntry));
            fread(&read_struct, sizeof(read_struct), 1, file_ptr);
            if (read_struct && strcmp(read_struct->name, filename) == 0) {
                return read_struct;
            }
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

    return NULL;
}

DirectoryEntry* delete_entry_from_root(const char *filename) {
    int start_block = FAT_TABLE[1];
    int next_block = start_block;
    FILE * file_ptr = fopen(FS_NAME, "w+");
    int num_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
    int found = 0;
    int fs_fd = open(FS_NAME, O_RDWR);

    // Iterate through all the root blocks
    while (next_block != 0XFFFF && next_block != 0) {
        // Check if file exists in current block (go through current root block's entries)
        int block_to_check = next_block;
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (block_to_check - 1)), SEEK_SET);
        for (int i = 0; i < num_entries; i++) {
            DirectoryEntry* read_struct;
            fread(&read_struct, sizeof(read_struct), 1, file_ptr);
            if (read_struct && strcmp(read_struct->name, filename) == 0) {
                // overwrite with spaces
                for (long long i = 0; i < sizeof(DirectoryEntry); ++i) {
                    fputc(' ', file_ptr);
                    found = 1;
                }
                break;
            }
        }
        if (found) {
            break;
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
    int start_block = FAT_TABLE[1];
    int last_block = start_block;
    int next_block = start_block;
    FILE * file_ptr = fopen(FS_NAME, "w+");
    int num_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
    bool found = false;
    int fs_fd = open(FS_NAME, O_RDWR);

    // Iterate through all the root blocks until we find final one
    while (next_block != 0XFFFF) {
        last_block = next_block;
        next_block = FAT_TABLE[next_block];
    }
    
    // Check if there is space in the last root block
    int block_to_check = last_block;
    lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (block_to_check - 1)), SEEK_SET);

    for (int i = 0; i < num_entries; i++) {
        DirectoryEntry* read_struct;
        fread(&read_struct, sizeof(read_struct), 1, file_ptr);
        if (read_struct->name[0] != '\0') { // how to check if it's ' '
            // empty space found
            fwrite(&entry, sizeof(DirectoryEntry), 1, file_ptr);
            break;
            found = 1;
        }
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
    FAT_TABLE[last_block] = new_final_block;
    FAT_TABLE[new_final_block] = 0XFFFF;
    lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (new_final_block - 1)), SEEK_SET);
    fwrite(&entry, sizeof(DirectoryEntry), 1, file_ptr);

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
    entry = malloc(sizeof(DirectoryEntry));
    strcpy(entry->name, filename);
    entry->size = 0;
    entry->firstBlock = -1; // firstBlock is undefined (null) when size = 0
    entry->type = 1; // TODO: is how do we set type
    entry->perm = 7; // TODO: is how do we set perm
    entry->mtime = time(NULL); // set time to now TODO: is this correct funciton call?
    
    // Save pointer at the end of root entries block
    if (add_entry_to_root(entry) == -1) {
        perror("Error: no empty entries in root directory");
        return -1;
    }
    return 0;
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
    return 0;
}

// adds data to end of file, given a block number in the file
int append_to_penn_fat(char* data, int block_no) {
    // Find last block in file (assumes delete_from_penn_fat removes all old blocks)
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
        cur_data_block = strndup(&data[offset], BLOCK_SIZE);
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
    }
    return 0;
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
    DirectoryEntry* d_entry = get_entry_from_root(dest);
    if (d_entry) {
        delete_entry_from_root(dest);
    }
    // create new file with name
    touch(dest);

    // find source file
    // open file
    free(d_entry);
    DirectoryEntry* entry = get_entry_from_root(source);

    int* chain = get_fat_chain(entry->firstBlock);

    for (int i = 0; i < NUM_FAT_ENTRIES; i++) {
        if (!chain[i]) {
            break;
        }
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE * (chain[i] - 1)), SEEK_SET);
        char* txt = malloc(sizeof(char) * BLOCK_SIZE);
        read(fs_fd, txt, sizeof(char) * BLOCK_SIZE);
        // write to file
        append_to_penn_fat(txt, d_entry->firstBlock);
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
    DirectoryEntry* entry = get_entry_from_root(dest);
    if (entry) {
        delete_entry_from_root(dest);
    }
    // create new file with name
    touch(dest);
    // TODO: update directory entry with the first block of file
    char* txt = read_file_to_string(h_fd);
    int i = find_first_free_block();
    append_to_penn_fat(txt, i);
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

int f_lseek(int fd, int offset, int whence) {

    off_t new_position;

    // TODO: Store the current position, need to keep track of position somehow
    off_t current_position = 0;
    // fcb->position;
    int blocks = offset / BLOCK_SIZE;
    int rem = offset % BLOCK_SIZE;

    // find entry name
    FDTEntry* fdtEntry = FDT[fd];
    DirectoryEntry* entry = get_entry_from_root(fdtEntry->name);
    int* chain = get_fat_chain(entry->firstBlock);
    
    switch (whence) {
        case F_SEEK_SET:
            new_position = TABLE_REGION_SIZE + (BLOCK_SIZE * (chain[blocks]-1)) + rem;
            break;
        case F_SEEK_CUR:
            new_position = current_position + offset;
            break;
        case F_SEEK_END:
            // TODO: need to actually store the current position somewhere

            new_position = lseek(fd, offset, whence);
            if (new_position == -1) {
                perror("Error seeking file");
                return -1;
            }

            printf("New file offset: %ld\n", (long)new_position);
            return 0;  // Success
        default:
            fprintf(stderr, "Invalid 'whence' parameter\n");
            return -1;  // Error
    }

    // TODO: update file pointer position that's stored to = new_position

    return 0;
}

void f_ls(const char *filename) {
    // iterate through directory entries
    // print file names 

    // open file
    FILE * file_ptr = fopen(FS_NAME, "r");
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
        // position file pointer
        lseek(fs_fd, TABLE_REGION_SIZE + (BLOCK_SIZE* (root_chain[i] - 1)), SEEK_SET);
        for (int i = 0; i < num_entries; i++) {
            DirectoryEntry* read_struct;
            fread(&read_struct, sizeof(read_struct), 1, file_ptr);
            if (read_struct) {
                printf("%s", read_struct->name);
            }
        }
        // DirectoryEntry** listEntries = FAT_DATA[root_chain[i]];
        // int max_entries = BLOCK_SIZE / sizeof(DirectoryEntry);
        // if (listEntries){
        //     for (int j = 0; j < max_entries; j++) {
        //         DirectoryEntry* entry = listEntries[j];
        //         if (entry) {
        //             write(STDOUT_FILENO, entry->name, sizeof(char)*strlen(entry->name));
        //         }
        //     }
        // }
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
        if (append) {
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
        } else {
            if (entry) {
                // Delete old file from penn fat if it exists and recreate it
                if (delete_from_penn_fat(output_file) < 0) {
                    perror("cat - Error deleting file cannot write properly, exiting");
                    return -1;
                }
            }
            if (touch(output_file) < 0) {
                perror("cat - Error creating file using touch");
                return -1;
            }
        }
        entry = get_entry_from_root(output_file); // Update entry value
        append_to_penn_fat(data, entry->firstBlock);
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
    FDTEntry* fdtEntry = malloc(sizeof(FDTEntry));
    fdtEntry->mode = mode;
    fdtEntry->name = fname;
    FDT[next_descriptor] = fdtEntry;

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
    DirectoryEntry* entry = get_entry_from_root(FDT[fd]->name);
    if (!entry) {
        perror("Error: source file does not exist");
        return -1;
    }

    // Get file data
    char* data = malloc(sizeof(char) * BLOCK_SIZE * NUM_FAT_ENTRIES);
    strcat_data(buf, entry->firstBlock);

    // Copy data to buffer
    strncpy(buf, data, n);
    free(data);
    // If we reach EOF return 0
    if (strlen(buf) * sizeof(char) < n) {
        return 0;
    }
    return n;
}

int f_write(int fd, const char *str, int n) {
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
    DirectoryEntry* entry = get_entry_from_root(FDT[fd]->name);
    char* data = malloc(sizeof(char) * BLOCK_SIZE * NUM_FAT_ENTRIES);
    if (FDT[fd]->mode == F_APPEND) {
        if (!entry) {
            // Create file if it does not exist
            if (touch(FDT[fd]->name) < 0) {
                perror("f_write - Error creating file using touch");
                return -1;
            }
            entry = get_entry_from_root(FDT[fd]->name);
            if (!entry) {
                perror("f_write - Error creating file using touch (entry still null)");
                return -1;
            }
        }
        // Get file data
        strcat_data(data, entry->firstBlock);
        // Append new data to file data
        strcat(data, str);
    } else {
        if (entry) {
            // Delete old file from penn fat if it exists and recreate it
            if (delete_from_penn_fat(FDT[fd]->name) < 0) {
                perror("f_write - Error deleting file cannot write properly, exiting");
                return -1;
            }
        }
        if (touch(FDT[fd]->name) < 0) {
            perror("f_write - Error creating file using touch");
            return -1;
        }
        // Copy str into data
        strncpy(data, str, n);
    }
    // Write data to file
    entry = get_entry_from_root(FDT[fd]->name);
    if (!entry) {
        perror("f_write - Error finding file entry before append");
        return -1;
    }
    append_to_penn_fat(data, entry->firstBlock);
    free(data);
    return n;
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
    DirectoryEntry* entry = get_entry_from_root(fname);
    if (!entry) {
        perror("f_unlink - Error: source file does not have a directory entry");
        return -1;
    }

    // Delete file from fat table and root directory
    delete_from_penn_fat(fname);
    return 0;
}