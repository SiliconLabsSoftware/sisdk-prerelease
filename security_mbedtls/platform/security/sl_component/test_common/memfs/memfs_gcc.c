/***************************************************************************//**
 * @brief Memory file system low level file i/o interface for GCC.
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
#include <stdio.h>
#if defined(__GNUC__)
#include <unistd.h>
#endif

#if defined(ENABLE_TEST_COVERAGE)
int memfs_use_gcov_file_io = 0;
#endif

/***************************************************************************//**
 * @brief
 *   Remove a file.
 *
 * @details
 *   This function removes the file named @p filename from memfs.
 *
 * @param[in] filename
 *   Name of file to remove.
 *
 * @return 0 if successful, or -1 if error.
 ******************************************************************************/
int _remove(const char * filename)
{
  return memfs_remove(filename);
}

/***************************************************************************//**
 * @brief
 *   Open a file
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
int _open(const char * filename, int mode)
{
#if defined(ENABLE_TEST_COVERAGE)
  if (memfs_use_gcov_file_io) {
    return sl_gcov_open(filename, mode);
  } else {
    return memfs_open(filename, mode);
  }
#else
  return memfs_open(filename, mode);
#endif
}

/***************************************************************************//**
 * @brief
 *   Close a file
 *
 * @details
 *   This function closes a currently open file in memfs.
 *
 * @param[in] handle
 *   File handle of file.
 *
 * @return 0 if successful, or -1 if error.
 ******************************************************************************/
int _close(int handle)
{
#if defined(ENABLE_TEST_COVERAGE)
  if (memfs_use_gcov_file_io) {
    return sl_gcov_close(handle);
  } else {
    return memfs_close(handle);
  }
#else
  return memfs_close(handle);
#endif
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
int _read(int handle, char * buffer, int size)
{
  return memfs_read(handle, (unsigned char*)buffer, size);
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
int _write(int handle, const char * buffer, int size)
{
#if defined(ENABLE_TEST_COVERAGE)
  if (memfs_use_gcov_file_io) {
    return sl_gcov_write(handle, (char*) buffer, size);
  } else {
    return memfs_write(handle, (unsigned char*)buffer, size);
  }
#else
  return memfs_write(handle, (unsigned char*)buffer, size);
#endif
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
long _lseek(int handle, long offset, int whence)
{
  return memfs_lseek(handle, offset, whence);
}
