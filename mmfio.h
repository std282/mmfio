/**
 * Copyright (c) 2024 Alexander Shokhin
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

/**
 * mmfio.h - single-header simple memory-mapped I/O library in C for Windows and POSIX
 */

#ifndef INCLUDE_MMFIO_H
#define INCLUDE_MMFIO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct MMFILE_impl MMFILE;                         // Opaque file definition

MMFILE* mmfopen(const char* name, const char* mode);       // Opens a specified file, in memory-mapped fashion
void* mmfdata(MMFILE* mmf);                                // Returns a pointer to memory-mapped data
size_t mmfsize(MMFILE* mmf);                               // Returns a number of bytes available at memory-mapped file location
void mmfclose(MMFILE* mmf);                                // Closes a memory-mapped file
const char* mmferror(void);                                // Returns text description of last error happened with memory-mapped I/O

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // INCLUDE_MMFIO_H

#if defined(MMFIO_IMPLEMENTATION) && !defined(MMFIO_IMPLEMENTATION_INCLUDED)
#define MMFIO_IMPLEMENTATION_INCLUDED

#ifdef __cplusplus
#error Implementation must be compiled with C compiler.
#endif // __cplusplus

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static char mmferrorbuffer[512] = "";
static void mmfseterror(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  snprintf(mmferrorbuffer, sizeof(mmferrorbuffer), fmt, args);
  va_end(args);
}

const char* mmferror(void)
{
  return mmferrorbuffer;
}

#define OPENMODE_INVALID 0
#define OPENMODE_READONLY 1
#define OPENMODE_WRITEONLY 2
#define OPENMODE_READWRITE 3
static int decode_open_mode(const char* mode)
{
  int i, mask = OPENMODE_INVALID;
  for (i = 0; mode[i] != '\0'; i++) {
    switch (mode[i]) {
      case 'r':
        mask |= OPENMODE_READONLY;
        break;

      case 'w':
        mask |= OPENMODE_WRITEONLY;
        break;
    }
  }

  return mask;
}

#ifdef _WIN32
// ============================================================================
// Windows implementation. Uses CreateFileMapping.
// ============================================================================

#include <windows.h>

struct MMFILE_impl {
  HANDLE file;
  HANDLE map;
  void* mem;
  size_t size;
};

static const char* GetWindowsErrorString(int errcode)
{
  static char buffer[128];

  const char* ret;
  DWORD size = FormatMessage(
    FORMAT_MESSAGE_FROM_SYSTEM, 
    NULL, errcode, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
    buffer, sizeof(buffer) - 1, 
    NULL);

  if (size > 0) {
    if (size > 1 && buffer[size - 2] == '\r' && buffer[size - 1] == '\n') {
      buffer[size - 2] = '\0';
    } 
    else {
      buffer[size] = '\0';
    }

    ret = buffer;
  }
  else {
    ret = "<FormatMessage error>";
  }

  return ret;
}

#define LASTERROR GetWindowsErrorString(GetLastError())

MMFILE* mmfopen(const char* name, const char* mode)
{
  MMFILE* ret = NULL;
  MMFILE f;
  struct { DWORD file, mode, page, map; } flags = {0};
  bool openable = false;

  switch (decode_open_mode(mode)) {
    case OPENMODE_READONLY:
      flags.file = GENERIC_READ;
      flags.mode = OPEN_EXISTING;
      flags.page = PAGE_READONLY;
      flags.map = FILE_MAP_READ;
      openable = true;
      break;
  }

  if (openable) {
    MMFILE* fp = calloc(1, sizeof(*fp));
    if (fp != NULL) {
      f.file = CreateFileA(name, flags.file, 0, NULL, flags.mode, FILE_ATTRIBUTE_NORMAL, NULL);
      if (f.file != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER filesize;
        BOOL ok = GetFileSizeEx(f.file, &filesize);
        if (ok) {
          f.size = (size_t)filesize.QuadPart;
          if (f.size > 0) {
            f.map = CreateFileMappingA(f.file, NULL, flags.page, filesize.HighPart, filesize.LowPart, NULL);
            if (f.map != INVALID_HANDLE_VALUE) {
              f.mem = MapViewOfFile(f.map, flags.map, 0, 0, f.size);
              if (f.mem != NULL) {
                *fp = f;
                ret = fp;
              } else mmfseterror("could not map file: OS error: MapViewOfFile failed: %s", LASTERROR);
              if (ret == NULL) CloseHandle(f.map);
            } else mmfseterror("could not map file: OS error: CreateFileMappingA failed: %s", LASTERROR);
          } else mmfseterror("could not map file: file is empty");
        } else mmfseterror("could not get file size: OS error: GetFileSizeEx failed: %s", LASTERROR);
        if (ret == NULL) CloseHandle(f.file);
      } else mmfseterror("could not open file \"%s\": OS error: CreateFileA failed: %s", name, LASTERROR);
      if (ret == NULL) free(fp);
    } else mmfseterror("could not allocate space for MMFILE: calloc returned NULL");
  } else mmfseterror("no valid file opening mode flags were provided");

  return ret;
}

void* mmfdata(MMFILE* mmf)
{
  return mmf->mem;
}

size_t mmfsize(MMFILE* mmf)
{
  return mmf->size;
}

void mmfclose(MMFILE* mmf)
{
  CloseHandle(mmf->map);
  CloseHandle(mmf->file);
  free(mmf);
}

#else
// ============================================================================
// POSIX implementation. Uses mmap.
// ============================================================================

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

struct MMFILE_impl {
  int fd;
  void* mem;
  size_t size;
};

#define LASTERROR strerror(errno)

MMFILE* mmfopen(const char* name, const char* mode)
{
  MMFILE* ret = NULL;
  MMFILE f;
  struct { int mode, prot, map; } flags = {0};
  bool openable = false;

  switch (decode_open_mode(mode)) {
    case OPENMODE_READONLY:
      flags.mode = O_RDONLY;
      flags.prot = PROT_READ;
      flags.map = MAP_PRIVATE;
      openable = true;
      break;
  }

  if (openable) {
    MMFILE* fp = calloc(1, sizeof(*fp));
    if (fp != NULL) {
      f.fd = open(name, flags.mode);
      if (f.fd != -1) {
        struct stat fileinfo;
        int res = fstat(f.fd, &fileinfo);
        if (res == 0) {
          f.size = (size_t)fileinfo.st_size;
          if (f.size > 0) {
            f.mem = mmap(NULL, f.size, flags.prot, flags.map, f.fd, 0);
            if (f.mem != MAP_FAILED) {
              *fp = f;
              ret = fp;
            } else mmfseterror("could not map file: mmap returned MAP_FAILED: %s", LASTERROR);
          } else mmfseterror("could not map file: file is empty");
        } else mmfseterror("could not get file size: fstat returned -1: %s", LASTERROR);
        if (ret == NULL) close(f.fd);
      } else mmfseterror("could not open file \"%s\": open returned -1: %s", name, LASTERROR);
      if (ret == NULL) free(fp);
    } else mmfseterror("could not allocate space for MMFILE: calloc returned NULL");
  } else mmfseterror("no valid file opening mode flags were provided");

  return ret;
}

void* mmfdata(MMFILE* mmf)
{
  return mmf->mem;
}

size_t mmfsize(MMFILE* mmf)
{
  return mmf->size;
}

void mmfclose(MMFILE* mmf)
{
  munmap(mmf->mem, mmf->size);
  close(mmf->fd);
  free(mmf);
}

#endif

#undef LASTERROR
#undef OPENMODE_INVALID
#undef OPENMODE_READONLY
#undef OPENMODE_WRITEONLY
#undef OPENMODE_READWRITE

#endif // MMFIO_IMPLEMENTATION
