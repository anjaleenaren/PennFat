#include <stdint.h>
#include <time.h>

// Constants and macros
#define MAX_FILENAME_LENGTH 32
#define MAX_FILES 256 // Adjust as necessary for your file system

int BLOCKS_IN_FAT, BLOCK_SIZE, FAT_SIZE, NUM_FAT_ENTRIES, TABLE_REGION_SIZE, DATA_REGION_SIZE;
uint16_t *FAT_TABLE;
uint16_t *FAT_DATA;
DirectoryEntry* ROOT;

// File Descriptor Table
int* FDT;

// Directory entry structure
typedef struct {
    char name[MAX_FILENAME_LENGTH]; // null-terminated file name
                                    // name[0] is aspecial marker
                                    //  - 0: end of directory
                                    //  - 1: deleted entry; the file is also deleted
                                    //  - 2: deleted entry; the file is still being used
    uint32_t size;                  // number of bytes in file
    uint16_t firstBlock;            // first block number of the file (undefined if size is zero)
    uint8_t type;                   // type of the file 
                                    //  0: unknown, 1: regular, 2: a directory file, 4: a symbolic link
    uint8_t perm;                   // file permissions
                                    //  0: none, 2: write-only, 4: read only,5: read and executable (shell scripts),
                                    //  6: read and write, 7: read, write, and executable
    time_t mtime;                   // creation/modification time as returned by time(2) in Linux
    char reserved[16];              // reserved for future use or extra features
} DirectoryEntry;

// File modes
#define F_WRITE  1 // Write mode
#define F_READ   2 // Read mode
#define F_APPEND 3 // Append mode

// Seek modes
#define F_SEEK_SET 0 // Seek from beginning of file
#define F_SEEK_CUR 1 // Seek from current position
#define F_SEEK_END 2 // Seek from end of file

/**
 * Initializes a new PennFAT filesystem.
 * Creates a PennFAT filesystem in the file named FS_NAME. The number of blocks in the FAT region is BLOCKS_IN_FAT (ranging from 1 through 32), and the block size is 256, 512, 1024, 2048, or 4096 bytes corresponding to the value (0 through 4) of BLOCK_SIZE_CONFIG.
 * @param fs_name Name of the file system image.
 * @param blocks_in_fat Number of blocks in the FAT region (ranging from 1 through 32).
 * @param block_size_config 0-4. block size will be 256, 512, 1024, 2048, or 4096 bytes depending on this value.
 */
void mkfs(const char *fs_name, int blocks_in_fat, int block_size_config);

/**
 * Mounts the PennFAT filesystem named FS_NAME by loading its FAT into memory.
 * @param fs_name Name of the file system image to mount.
 */
void mount(const char *fs_name);

/**
 * Unmounts the currently mounted PennFAT filesystem.
 */
void umount();

/**
 * Creates a file if it does not exist, or updates its timestamp to the current system time.
 * @param filename Name of the file to touch.
 * @return 0 on success, negative on error.
 */
int touch(const char *filename);

/**
 * Renames a file from SOURCE to DEST.
 * @param source Name of the source file.
 * @param dest Name of the destination file.
 * @return 0 on success, negative on error.
 */
int mv(const char *source, const char *dest);

/**
 * Removes a file.
 * @param filename Name of the file to remove.
 * @return 0 on success, negative on error.
 */
int rm(const char *filename);

/**
 * Concatenates files and prints them to stdout, or overwrites/creates OUTPUT_FILE.
 * @param files Array of file names to concatenate.
 * @param num_files Number of files in the array.
 * @param output_file Name of the output file, NULL if output is to stdout.
 * @param append Flag to indicate appending to the file instead of overwriting.
 * @return 0 on success, negative on error.
 */
int cat(const char **files, int num_files, const char *output_file, int append);

/**
 * Copies a file from SOURCE to DEST.
 * @param source Name of the source file.
 * @param dest Name of the destination file.
 * @param host_to_fs Flag indicating if the copy is from the host OS to the file system.
 * @return 0 on success, negative on error.
 */
int cp(const char *source, const char *dest, int host_to_fs);

/**
 * Lists all files in the current directory or details of a specific file.
 * @param filename Name of the file to list, NULL for listing all files.
 * @return 0 on success, negative on error.
 */
int ls(const char *filename);

/**
 * Opens a file in the filesystem.
 * @param fname Name of the file to open. 
 * -> allowed name: https://www.ibm.com/docs/en/zos/3.1.0?topic=locales-posix-portable-file-name-character-set
 * @param mode Mode to open the file in (F_WRITE, F_READ, F_APPEND).
 * @return File descriptor on success, negative value on error.
 */
int f_open(const char *fname, int mode);

/**
 * Reads data from a file.
 * @param fd File descriptor of the file to read from.
 * @param n Number of bytes to read.
 * @param buf Buffer to store read data.
 * @return Number of bytes read, 0 if EOF, negative on error.
 */
int f_read(int fd, int n, char *buf);

/**
 * Writes data to a file.
 * @param fd File descriptor of the file to write to.
 * @param str Data to write.
 * @param n Number of bytes to write.
 * @return Number of bytes written, negative on error.
 */
int f_write(int fd, const char *str, int n);

/**
 * Closes an open file.
 * @param fd File descriptor of the file to close.
 * @return 0 on success, negative on failure.
 */
int f_close(int fd);

/**
 * Deletes a file from the filesystem.
 * @param fname Name of the file to delete.
 * @return 0 on success, negative on error.
 */
int f_unlink(const char *fname);

/**
 * Repositions the file pointer of an open file.
 * @param fd File descriptor of the file.
 * @param offset Offset for repositioning.
 * @param whence Mode of seeking (F_SEEK_SET, F_SEEK_CUR, F_SEEK_END).
 * @return 0 on success, negative on error.
 */
int f_lseek(int fd, int offset, int whence);

/**
 * Lists files in the current directory or details of a specific file.
 * @param filename Name of the file to list or NULL for listing all files.
 */
void f_ls(const char *filename);

void f_chmod();

// Helper functions
/**
 * Mallocs an array of all the block numbers in the FAT chain of a file.
 * @param start_index Index in fat_table to begin search.
 * @return array of all the block numbers in the FAT chain of a file. Need to free.
 */
int* get_fat_chain(int start_index);

/**
 * Gets the data (stored as string) from a file and concatante it to data.
 * @param data String to concatenate to.
 * @param start_index Index in fat_table to begin search.
 */
void strcat_data(char* data, int start_index)

/**
 * Gets the directory entry of a file from its name.
 * @param filename Name of the file to get the entry of.
 * @return Directory entry of the file.
 */
DirectoryEntry* get_entry_from_root(const char *filename);

/**
 * Adds a directory entry to the root directory.
 * @param entry Directory entry to add.
 * @return 0 on success, negative on error.
 */
int add_entry_to_root(DirectoryEntry* entry);

/**
 * Finds first free block that we can use. (Search fat table to find first 0)
 * @return 0 on success, negative on error.
 */
int find_first_free_block();

/**
 * Deletes a file from PennFat Table.
 * Finds entry for filename. Then goes through PennFat Table and deletes all the relevant pointers starting at entry->firstBlock.
 * @param filename Name of the file to delete.
 * @return 0 on success, negative on error.
 */
int delete_from_penn_fat(const char *filename);