/***************************************************************************//**
 * @brief Memory file system low level file i/o interface
 *******************************************************************************
 * # License
 * <b>Copyright 2018 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/
#if !defined __MEMFS_H_
#define __MEMFS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define MEMFS_FILENAME_MAXSIZE      (50)

#define COV_FILE_HANDLE 1000

#if defined(ENABLE_TEST_COVERAGE)
int  sl_gcov_open      (const char *ptr, int mode);
int  sl_gcov_close     (int file);
int  sl_gcov_write     (int file, char *pch, int len);

extern int memfs_use_gcov_file_io;
#endif

int  memfs_insert_file (const char * filename, bool readonly, uint8_t * buf,
                        unsigned int len);
int  memfs_remove      (const char * filename);
int  memfs_open        (const char * filename, int mode);
int  memfs_close       (int handle);
int  memfs_read        (int handle, unsigned char * buffer, size_t size);
int  memfs_write       (int handle, const unsigned char * buffer, size_t size);
long memfs_lseek       (int handle, long offset, int whence);

/* POSIX-like directory entry functions */

typedef struct memfs_dirent{
  char           d_name[MEMFS_FILENAME_MAXSIZE]; /* filename */
  int            fileno;
} memfs_dirent;

typedef struct memfs_DIR{
  memfs_dirent   current;                /* current entry */
} memfs_DIR;

memfs_DIR *memfs_opendir(const char *name);
int  memfs_closedir(memfs_DIR *);
struct memfs_dirent *memfs_readdir(memfs_DIR *);

#endif /* __MEMFS_H_ */
