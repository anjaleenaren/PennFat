#include <stdint.h>
#include <time.h>
#include <stdbool.h>
/**
 * Initializes a new PennFAT filesystem.
 * Creates a PennFAT filesystem in the file named FS_NAME. The number of blocks in the FAT region is BLOCKS_IN_FAT (ranging from 1 through 32), and the block size is 256, 512, 1024, 2048, or 4096 bytes corresponding to the value (0 through 4) of BLOCK_SIZE_CONFIG.
 * @param fs_name Name of the file system image.
 * @param blocks_in_fat Number of blocks in the FAT region (ranging from 1 through 32).
 * @param block_size_config 0-4. block size will be 256, 512, 1024, 2048, or 4096 bytes depending on this value.
 */
void mkfs(char *fs_name, int blocks_in_fat, int block_size_config);

/**
 * Mounts the PennFAT filesystem named FS_NAME by loading its FAT into memory.
 * @param fs_name Name of the file system image to mount.
 */
void mount(const char *fs_name);

/**
 * Unmounts the currently mounted PennFAT filesystem.
 */
void unmount();

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
int cp(const char *source, const char *dest, int s_host, int d_host);
// int cp(const char *source, const char *dest, int host_to_fs);


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
int f_open(char *fname, int mode);

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
