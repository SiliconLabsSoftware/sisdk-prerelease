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
#include "memfs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "em_device.h"

#if defined(__GNUC__)
#include <fcntl.h>
#include <unistd.h>
#endif
#if defined(__ICCARM__)
#include <yfuns.h>
#define O_RDONLY  _LLIO_RDONLY
#define O_WRONLY  _LLIO_WRONLY
#define O_RDWR    _LLIO_RDWR
#define O_APPEND  _LLIO_APPEND
#define O_TRUNC   _LLIO_TRUNC
#define O_CREAT   _LLIO_CREAT
#define O_EXCL    _LLIO_EXCL
#define O_BINARY  _LLIO_BINARY
#define O_TEXT    _LLIO_TEXT
#define STDIN_FILENO   _LLIO_STDIN
#define STDOUT_FILENO  _LLIO_STDOUT
#define STDERR_FILENO  _LLIO_STDERR
#endif

#if !defined(MEMFS_FILE_TABLE_SIZE)
#define MEMFS_FILE_TABLE_SIZE        (2)
#endif

// Set to 3 to avoid overlap with the standard file descriptors STDIN_FILENO,
// STDOUT_FILENO and STDERR_FILENO.
#define MEMFS_FILE_HANDLE_BEGIN      (3)

#define MEMFS_FILE_READ_ONLY   (1 << 31UL)

typedef struct {
  const char*  filename;
  uint8_t*     filebuf;
  uint8_t*     fileptr;
  unsigned int filelen;
  uint32_t     mode;
} memfs_file_entry_t;

static memfs_file_entry_t memfs_file_table[MEMFS_FILE_TABLE_SIZE];
static int memfs_file_table_head = 0;

static int memfs_find_file(const char * filename)
{
  int handle;
  for (handle = 0; handle < memfs_file_table_head; handle++) {
    if (memfs_file_table[handle].filename == NULL) {
      continue;
    }
    if (0 == strcmp(memfs_file_table[handle].filename, filename)) {
      return handle;
    }
  }
  return -1;
}

/***************************************************************************//**
 * @brief
 *   Insert a file buffer in memfs.
 *
 * @details
 *   This function inserts a file entry in the file table of memfs which will
 *   read and optionally write data to the buffer pointed to by @p buf.
 *
 * @param[in] filename
 *   Name of file entry. Filenames (strings) residing in flash can be stored by
 *   reference instead of allocated.
 *
 * @param[in] readonly
 *   File buffer is read only. E.g. buffer is in read-only memory (eeprom).
 *   This file should be opened with mode flag O_RDONLY when calling open.
 *
 * @param[in] buf
 *   Pointer to file buffer which holds the data.
 *
 * @param[in] len
 *   The length (in bytes) of the file buffer.
 *
 * @return 0 if successful, or -1 if error.
 ******************************************************************************/
int memfs_insert_file(const char * filename, bool readonly, uint8_t * buf,
                      unsigned int len)
{
  if (memfs_file_table_head >= MEMFS_FILE_TABLE_SIZE) {
    return -1;
  }
  if (memfs_find_file(filename) >= 0) {
    return -1;
  }
  if (strstr(filename, "no_such_dir")) {
    return -1;
  }

  if ((uint32_t) filename < (SRAM_BASE+SRAM_SIZE) && (uint32_t)filename >= SRAM_BASE) {
    char * buffer = (char *)malloc(strlen(filename) + 1);
    if (buffer == NULL) {
      return -1;
    }
    memcpy(buffer, filename, strlen(filename) + 1);
    memfs_file_table[memfs_file_table_head].filename = buffer;
  } else {
    memfs_file_table[memfs_file_table_head].filename = filename;
  }
  memfs_file_table[memfs_file_table_head].filebuf = buf;
  memfs_file_table[memfs_file_table_head].fileptr = buf;
  memfs_file_table[memfs_file_table_head].filelen = len;
  memfs_file_table[memfs_file_table_head].mode    =
    readonly ? MEMFS_FILE_READ_ONLY : 0;
  return memfs_file_table_head++;
}

/***************************************************************************//**
 * @brief
 *   Remove a file buffer in memfs.
 *
 * @details
 *   This function removes a file from memfs by freeing
 *   the associated file entry from the file table.
 *
 * @param[in] filename
 *   Name of file to remove.
 *
 * @return 0 if successful, or -1 if error.
 ******************************************************************************/
int memfs_remove(const char * filename)
{
  int handle;
  /*
   * Find the file in the file table
   */
  handle = memfs_find_file(filename);
  if (handle < 0) {
    return -1;
  }

  if ((uint32_t) memfs_file_table[handle].filename < (SRAM_BASE+SRAM_SIZE) && (uint32_t)memfs_file_table[handle].filename >= SRAM_BASE) {
    free((void*)memfs_file_table[handle].filename);
  }

  memfs_file_table[handle].filename = NULL;
  return 0;
}

/***************************************************************************//**
 * @brief
 *   Open a file in memfs.
 *
 * @details
 *   This function opens a file named @p filename in memfs.
 *
 * @param[in] filename
 *   Name of file entry.
 *
 * @param[in] mode
 *   Mode flags for open operation.
 *
 * @return File handle if successful, or -1 if error.
 ******************************************************************************/
int memfs_open(const char * filename, int mode)
{
  int handle;
  memfs_file_entry_t* f;

  /*
   * Find the file in the file table
   */
  handle = memfs_find_file(filename);

  if (mode & O_CREAT) {
    /* Create a file if it doesn't exists. */
    if (handle < 0) {
      handle =  memfs_insert_file(filename, false, NULL, 0);
      if (handle < 0) {
        return -1;
      }
    }
  } else {
    if (handle < 0) {
      return -1;
    }
    switch ( mode & (O_RDONLY | O_WRONLY | O_RDWR) ) {
      case O_RDONLY:
        /* The file should be opened for read only.
           Return error if it does not exist.*/
        if (handle < 0) {
          return -1;
        }
        break;

      case O_WRONLY:
        /* The file should be opened for write only. */
        if (memfs_file_table[handle].mode & (uint32_t)MEMFS_FILE_READ_ONLY) {
          return -1;
        }
        break;

      case O_RDWR:
        /* The file should be opened for both reads and writes. */
        if (memfs_file_table[handle].mode & (uint32_t)MEMFS_FILE_READ_ONLY) {
          return -1;
        }
        break;

      default:
        return -1;
    }
  }

  f = &memfs_file_table[handle];

  /* Check what we should do with it if it exists. */
  if (mode & O_APPEND) {
    /* Append to the existing file. */
    f->fileptr = f->filebuf + f->filelen;
  }

  if (mode & O_TRUNC) {
    /* Truncate the existsing file. */
    f->fileptr = f->filebuf;
  }

  f->mode = mode;

  return handle + MEMFS_FILE_HANDLE_BEGIN;
}

/***************************************************************************//**
 * @brief
 *   Close a file in memfs.
 *
 * @details
 *   This function closes a currently open file in memfs.
 *
 * @param[in] handle
 *   File handle of file.
 *
 * @return 0 if successful, or -1 if error.
 ******************************************************************************/
int memfs_close(int handle)
{
  memfs_file_entry_t* f;
  if (handle > MEMFS_FILE_HANDLE_BEGIN + memfs_file_table_head) {
    return -1;
  }
  if (handle - MEMFS_FILE_HANDLE_BEGIN > MEMFS_FILE_TABLE_SIZE) {
    return -1;
  }
  f = &memfs_file_table[handle - MEMFS_FILE_HANDLE_BEGIN];
  f->fileptr = f->filebuf;
  return 0;
}

/***************************************************************************//**
 * @brief
 *   Read data from a file in memfs.
 *
 * @details
 *   This function reads the number of bytes specfied by @p size into the
 *   @p buffer from the file associated with @p handle.
 *
 * @param[in] handle
 *   File handle of file.
 *
 * @param[in] buffer
 *   Pointer to buffer where to write data.
 *
 * @param[in] size
 *   Size in bytes to read from file.
 *
 * @return The number of bytes successfully read, or -1 if error.
 ******************************************************************************/
int memfs_read(int handle, unsigned char * buffer, size_t size)
{
  int cnt = 0;
  memfs_file_entry_t* f;
  if (handle > MEMFS_FILE_HANDLE_BEGIN + memfs_file_table_head) {
    return -1;
  }
  if (handle - MEMFS_FILE_HANDLE_BEGIN > MEMFS_FILE_TABLE_SIZE) {
    return -1;
  }
  f = &memfs_file_table[handle - MEMFS_FILE_HANDLE_BEGIN];

  for (/* Empty */; size > 0; --size) {
    int c;
    if ((unsigned int)(f->fileptr - f->filebuf) >= f->filelen) {
      c = -1;
    } else {
      c = *(f->fileptr)++;
    }

    if (c < 0) {
      break;
    }

    *buffer++ = c;
    ++cnt;
  }

  return cnt;
}

/***************************************************************************//**
 * @brief
 *   Write data to a file in memfs.
 *
 * @details
 *   This function writes the number of bytes specfied by @p size from the
 *   @p buffer to the file associated with @p handle.
 *
 * @param[in] handle
 *   File handle of file.
 *
 * @param[in] buffer
 *   Pointer to buffer where data is located.
 *
 * @param[in] size
 *   Size in bytes to write to file.
 *
 * @return The number of bytes successfully written, or -1 if error.
 ******************************************************************************/
int memfs_write(int handle, const unsigned char * buffer, size_t size)
{
  memfs_file_entry_t* f;
  unsigned int data_size;
  unsigned int new_data_size;

  /* Return ERROR for writes to standard in. */
  if (handle == STDIN_FILENO) {
    return -1;
  }

#if 0
  /* Write to standard console for standard out/err. */
  if ( handle == STDOUT_FILENO || handle == STDERR_FILENO ) {
    return console_write(handle, buffer, size);
  }
#endif

  if (handle > MEMFS_FILE_HANDLE_BEGIN + memfs_file_table_head) {
    return -1;
  }
  if (handle - MEMFS_FILE_HANDLE_BEGIN > MEMFS_FILE_TABLE_SIZE) {
    return -1;
  }
  f = &memfs_file_table[handle - MEMFS_FILE_HANDLE_BEGIN];

  if (buffer == 0) {
    /*
     * This means that we should flush internal buffers.  Since we
     * don't we just return.
     */
    return 0;
  }

  data_size = (unsigned int)(f->fileptr - f->filebuf);
  new_data_size = data_size + size;

  if (new_data_size > f->filelen) {
    /* Need more space */
    uint8_t* new = (uint8_t*) malloc(new_data_size);
    if (new) {
      /* First copy existing data. */
      f->fileptr = new;
      memcpy(f->fileptr, f->filebuf, data_size);
      free(f->filebuf);
      f->filebuf = new;
      f->fileptr += data_size;
      memcpy(f->fileptr, buffer, size);
      f->fileptr += size;
      f->filelen = new_data_size;
    } else {
      return -1;
    }
  } else {
    memcpy(f->fileptr, buffer, size);
    f->fileptr += size;
  }

  return size;
}

/***************************************************************************//**
 * @brief
 *   Move the file pointer of a file in memfs.
 *
 * @details
 *   This function moves the file pointer from where data is read or written
 *   in a file in memfs. The new location is specified by @p offset from the
 *   reference @p whence which must be one of SEEK_SET, SEEK_CUR and SEEK_END.
 *   If @p whence is SEEK_SET, the new location is @p offset bytes from the
 *   start of the file.
 *   If @p whence is SEEK_CUR, the new location is @p offset bytes from the
 *   current file pointer of the file.
 *   If @p whence is SEEK_END, the new location is the negative @p offset in
 *   bytes from the  end of the file.
 *
 * @param[in] handle
 *   File handle of file.
 *
 * @param[in] offset
 *   Offset to new location of file pointer.
 *
 * @param[in] whence
 *   Reference for @p offset. Must be one of SEEK_SET, SEEK_CUR and SEEK_END.
 *
 * @return The number of bytes successfully written, or -1 if error.
 ******************************************************************************/
long memfs_lseek(int handle, long offset, int whence)
{
  memfs_file_entry_t* f;
  if (handle > MEMFS_FILE_HANDLE_BEGIN + memfs_file_table_head) {
    return -1;
  }
  if (handle - MEMFS_FILE_HANDLE_BEGIN > MEMFS_FILE_TABLE_SIZE) {
    return -1;
  }
  f = &memfs_file_table[handle - MEMFS_FILE_HANDLE_BEGIN];

  switch (whence) {
    case SEEK_SET:
      if ((unsigned int)offset > f->filelen) {
        return -1;
      }
      f->fileptr = f->filebuf + offset;
      return (long) (f->fileptr - f->filebuf);
    case SEEK_CUR:
      if ((unsigned int)((f->fileptr - f->filebuf) + offset) > f->filelen) {
        return -1;
      }
      f->fileptr += offset;
      return (long) (f->fileptr - f->filebuf);
    case SEEK_END:
      if ((unsigned int)offset > f->filelen) {
        return -1;
      }
      f->fileptr = f->filebuf + f->filelen - offset;
      return (long) (f->fileptr - f->filebuf);
    default:
      return -1;
  }
}

/***************************************************************************//**
 * @brief
 *   Open a memfs directory.
 *
 * @details
 *   This function appears to open the memfs directory called @p name. However
 *   since directories are not supported by memfs yet, this function will open
 *   the _only_ base directory of memfs for any given directory @p name.
 *
 * @param[in] name
 *   Name of directory to open.
 *
 * @return Pointer to a memfs_DIR handle is successful, NULL if error.
 ******************************************************************************/
memfs_DIR *memfs_opendir(const char *name)
{
  (void) name;
  memfs_DIR * dir = malloc(sizeof(memfs_DIR));
  dir->current.fileno = -1;
  dir->current.d_name[0] = 0;
  return dir;
}

/***************************************************************************//**
 * @brief
 *   Close a memfs directory.
 *
 * @details
 *   This function closes the memfs directory associated with the
 *   @p dir handle.
 *
 * @param[in] dir
 *   Pointer to a memfs_DIR handle.
 *
 * @return 0 if successful, -1 if error.
 ******************************************************************************/
int  memfs_closedir(memfs_DIR *dir)
{
  if (NULL == dir) {
    return -1;
  }

  free(dir);
  return 0;
}

/***************************************************************************//**
 * @brief
 *   Get next memfs directory entry.
 *
 * @details
 *   This function returns a pointer to the next memfs directory entry associated
 *   with the @p dir handle.
 *
 * @param[in] dir
 *   Pointer to a memfs_DIR handle.
 *
 * @return Pointer to the next entry if successful, NULL if error.
 ******************************************************************************/
struct memfs_dirent *memfs_readdir(memfs_DIR *dir)
{
  if (NULL == dir) {
    return NULL;
  }

  if (MEMFS_FILE_TABLE_SIZE <= dir->current.fileno) {
    return NULL;
  }

  if (-1 == dir->current.fileno) {
    dir->current.fileno = 0;
  } else {
    dir->current.fileno++;
  }

  strcpy(dir->current.d_name,
         memfs_file_table[dir->current.fileno].filename);

  return &dir->current;
}
