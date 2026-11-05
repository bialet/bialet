#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdio.h>

// Hex-escapes every control byte in a wire-derived value before it reaches the
// log. Returns a heap string (caller frees) or NULL on OOM.
char* log_escape(const char* s);

char* safe_malloc(size_t size);
char* string_safe_copy(const char* zSrc);
char* string_append(char* zPrior, const char* zSep, const char* zSrc);
void  trim(char* str);

// Opens [path] without following a symlink/junction in the final component.
// On POSIX every component is walked with openat(O_NOFOLLOW); on Windows the
// file is opened with FILE_FLAG_OPEN_REPARSE_POINT and reparse points are
// rejected outright. Returns NULL on failure or when the target is a link.
FILE* open_file_no_follow(const char* path);

// Returns 1 when any path component of [uri] starts with '_' or '.', i.e. when
// it names a private file (a database, .env, a dotfile). Shared by the HTTP
// path and the Wren file API so both enforce the same boundary. The single
// exception is an RFC 8615 ".well-known" first segment.
int has_forbidden_uri_component(const char* uri);

#ifndef _WIN32
// POSIX building blocks behind open_file_no_follow, shared with the module
// loader and the static-file path. [path] must be absolute.
int   open_fd_no_follow(const char* path);
char* read_file_fd(int fd);
#endif

#endif
