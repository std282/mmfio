# mmfio.h - simple single-header C library for memory-mapped I/O

This is a very simple single-header STB-style library for very simple memory-mapped I/O in C, designed to be portable across Windows and POSIX operating systems. So far it supports only reading files of arbitrary length, on 64-bit OS.

Memory-mapped I/O allows you to work with files as if you work with memory, in contrast to streams (fopen, fread, fwrite, ...). In the realm of memory-mapped I/O, file writing is writing to a pointer; file reading is reading from a pointer. But memory-mapped I/O has little to no effect on your _actual_ RAM consumption - file is merely mapped onto address space of your machine. It is very handy if you need to read the file without worrying about file stream errors, sudden EOFs and whatnot.

I have created this simple bread-and-butter library because I failed to find any out there. Perhaps most programmers do not need them, or perhaps it's absolutely trivial to implement platform-agnostic memory-mapped I/O; I am definitely not to judge; or there is a simpler way and I'm just ignorant. There are a couple libraries for C++, though; but I wanted one to be in C. I can only humbly hope that it will be of use for someone.

## Example

```c
// You probably should create a separate file to hold an implementation and
// let other files in your project just include "mmfio.h".
#define MMFIO_IMPLEMENTATION
#include "mmfio.h"

void test(void)
{
  MMFILE* f = mmfopen("hugefile.bin", "r");
  if (f != NULL) {
    const void* data = mmfdata(f);  // Pointer to the memory-mapped file data
    size_t size = mmfsize(f);       // The size of the file

    // The file is all yours at hand! No need for buffering with fread, 
    // checking with feof, etc: just read what you need from any location you
    // want, as if the whole file is loaded in the RAM.

    mmfclose(f);
  }
  else {
    // If anything went wrong you have an error string which hopefully provides
    // just enough information to figure out what went wrong
    const char* error_description = mmferror();
  }
}
```
