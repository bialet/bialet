#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdio.h>

char* safe_malloc(size_t size);
char* string_safe_copy(const char* zSrc);
char* string_append(char* zPrior, const char* zSep, const char* zSrc);
void  trim(char* str);

// Opens [path] without following a symlink/junction in the final component.
// On POSIX every component is walked with openat(O_NOFOLLOW); on Windows the
// file is opened with FILE_FLAG_OPEN_REPARSE_POINT and reparse points are
// rejected outright. Returns NULL on failure or when the target is a link.
FILE* open_file_no_follow(const char* path);

#ifndef _WIN32
// POSIX building blocks behind open_file_no_follow, shared with the module
// loader and the static-file path. [path] must be absolute.
int   open_fd_no_follow(const char* path);
char* read_file_fd(int fd);
#endif

#endif
