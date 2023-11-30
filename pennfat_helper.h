/* Macros, Constants, Globals, and Helper Functions for pennfat*/
#include <stdint.h>
#include <time.h>
#include <stdbool.h>

// Constants and macros
#define MAX_FILENAME_LENGTH 32
#define MAX_FILES 256 // Adjust as necessary for your file system

int BLOCKS_IN_FAT, BLOCK_SIZE, FAT_SIZE, NUM_FAT_ENTRIES, TABLE_REGION_SIZE, DATA_REGION_SIZE;
uint16_t *FAT_TABLE;
uint16_t *FAT_DATA;
char* FS_NAME;
// uint16_t *FAT_DATA;

// File Descriptor Table
typedef struct {
    char name[MAX_FILENAME_LENGTH]; // null-terminated file name (matches with DirectoryEntry)
    int mode; // mode file is opened in
    int offset; // offset of file pointer
} FDTEntry;
FDTEntry** FDT;

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

DirectoryEntry* ROOT;

// File modes
#define F_WRITE  1 // Write mode
#define F_READ   2 // Read mode
#define F_APPEND 3 // Append mode

// Seek modes
#define F_SEEK_SET 0 // Seek from beginning of file
#define F_SEEK_CUR 1 // Seek from current position
#define F_SEEK_END 2 // Seek from end of file


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
void strcat_data(char* data, int start_index);

/**
 * Gets the directory entry of a file from its name.
 * @param filename Name of the file to get the entry of.
 * @param update_first_block Flag to indicate if we should update the first block of the entry
 *  (set to true whenever you are writing)
 * @param rename_to Name to rename the file to (NULL if no rename)
 * @return Directory entry of the file.
 */
DirectoryEntry* get_entry_from_root(const char *filename, bool update_first_block, char* rename_to);

/**
 * Adds a directory entry to the root directory.
 * @param entry Directory entry to add.
 * @return 0 on success, negative on error.
 */
int add_entry_to_root(DirectoryEntry* entry);

DirectoryEntry* delete_entry_from_root(const char *filename);

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

/**
 * Appends to a file in PennFat Table that starts at block_no (needs to have a DirectoryEntry already).
 * @param data string to append to file.
 * @param block_no block number to append to (typically entry->firstBlock)
 * @param n max number of bytes to append (will be ignored if greater than block size)
 * @return 0 on success, negative on error.
 */
int append_to_penn_fat(char* data, int block_no, int n);

int cp_from_h(const char *source, const char *dest);

int cp_helper(const char *source, const char *dest);

int cp_to_h(const char *source, const char *dest);