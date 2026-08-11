#include "utils.h"

#include "bialet.h"
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef NAME_MAX
#define NAME_MAX 255
#endif
#else
#include <fcntl.h>
#include <io.h>
#endif

char* safe_malloc(size_t size) {
  char* p;

  p = (char*)malloc(size);
  if(p == 0) {
    exit(1);
  }
  return p;
}

char* string_safe_copy(const char* zSrc) {
  char*  zDest;
  size_t size;

  if(zSrc == 0)
    return 0;
  size = strlen(zSrc) + 1;
  zDest = (char*)safe_malloc(size);
  strcpy(zDest, zSrc);
  return zDest;
}

char* string_append(char* zPrior, const char* zSep, const char* zSrc) {
  char*  zDest;
  size_t size;
  size_t n0, n1, n2;

  if(zSrc == 0)
    return 0;
  if(zPrior == 0)
    return string_safe_copy(zSrc);
  n0 = strlen(zPrior);
  n1 = strlen(zSep);
  n2 = strlen(zSrc);
  size = n0 + n1 + n2 + 1;
  zDest = (char*)safe_malloc(size);
  memcpy(zDest, zPrior, n0);
  free(zPrior);
  memcpy(&zDest[n0], zSep, n1);
  memcpy(&zDest[n0 + n1], zSrc, n2 + 1);
  return zDest;
}

void trim(char* str) {
  char* start = str;
  char* end;

  while(isspace((unsigned char)*start))
    start++;

  if(*start == 0) {
    *str = '\0';
    return;
  }

  end = start + strlen(start) - 1;
  while(end > start && isspace((unsigned char)*end))
    end--;
  end[1] = '\0';

  if(start != str)
    memmove(str, start, strlen(start) + 1);
}

#ifndef _WIN32
// Walks [path] component by component with openat(O_NOFOLLOW) so a symlink
// swap on any component between a realpath() containment check and this open
// fails with ELOOP instead of being followed out of the root.
static int open_fd_no_follow(const char* path) {
  if(path == NULL || path[0] != '/')
    return -1;
  int dirfd = open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if(dirfd < 0)
    return -1;
  const char* p = path + 1;
  while(*p) {
    while(*p == '/')
      p++;
    if(*p == '\0')
      break;
    const char* next = strchr(p, '/');
    size_t      comp_len = next ? (size_t)(next - p) : strlen(p);
    char        comp[NAME_MAX + 1];
    if(comp_len >= sizeof(comp))
      comp_len = sizeof(comp) - 1;
    memcpy(comp, p, comp_len);
    comp[comp_len] = '\0';
    int next_fd = openat(dirfd, comp, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    close(dirfd);
    if(next_fd < 0)
      return -1;
    dirfd = next_fd;
    p += comp_len;
  }
  return dirfd;
}
#endif

FILE* open_file_no_follow(const char* path) {
  if(path == NULL)
    return NULL;
#ifdef _WIN32
  wchar_t wide[PATH_MAX];
  if(MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, PATH_MAX) == 0)
    return NULL;
  // FILE_FLAG_OPEN_REPARSE_POINT opens the link/junction itself instead of its
  // target, so a planted reparse point is never followed; we then reject it.
  HANDLE h = CreateFileW(wide, GENERIC_READ,
                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
  if(h == INVALID_HANDLE_VALUE)
    return NULL;
  BY_HANDLE_FILE_INFORMATION info;
  if(!GetFileInformationByHandle(h, &info)) {
    CloseHandle(h);
    return NULL;
  }
  if(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
    CloseHandle(h);
    return NULL;
  }
  int fd = _open_osfhandle((intptr_t)h, _O_RDONLY);
  if(fd < 0) {
    CloseHandle(h);
    return NULL;
  }
  return fdopen(fd, "rb");
#else
  int fd = open_fd_no_follow(path);
  if(fd < 0)
    return NULL;
  FILE* f = fdopen(fd, "rb");
  if(f == NULL)
    close(fd);
  return f;
#endif
}
