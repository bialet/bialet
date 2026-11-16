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

// Fail-fast allocation. Note this exits the process, so an allocation failure
// while handling a request takes down the whole server rather than failing that
// one request. On POSIX the supervisor forks a replacement; on Windows there is
// none, so a transient OOM is an outage. Kept as-is because the handful of
// callers cannot meaningfully recover mid-Wren-call, but at least say what
// happened first: this used to exit(1) with no output at all, which is
// indistinguishable from a crash.
char* safe_malloc(size_t size) {
  char* p = (char*)malloc(size);
  if(p == NULL) {
    fprintf(stderr, "bialet: out of memory allocating %zu bytes\n", size);
    exit(EXIT_FAILURE);
  }
  return p;
}

// Hex-escapes every control byte in a wire-derived value before it reaches the
// log. Request lines, URIs and multipart filenames are attacker-supplied and
// may carry a bare LF, CR, TAB or ESC: written verbatim they let one request
// open a second log record and emit terminal/OSC sequences into the operator's
// log or journal. Two hex digits per byte keeps the value readable while making
// every control byte inert.
//
// C1 controls (U+0080..U+009F, the range that carries CSI, OSC, ST and NEL) are
// escaped too, including their two-byte UTF-8 form C2 80..C2 9F: a value that
// arrived as valid UTF-8 used to reach the log raw while this function's own
// promise said every control byte was inert. Other UTF-8 is passed through
// unchanged, so an accented or CJK log message stays readable.
//
// Returns a heap string (caller frees) or NULL on OOM.
char* log_escape(const char* s) {
  if(s == NULL)
    return NULL;
  size_t len = strlen(s);
  char*  out = malloc(len * 4 + 1);
  if(out == NULL)
    return NULL;
  static const char hex[] = "0123456789abcdef";
  size_t            n = 0;
  size_t            i = 0;
  while(i < len) {
    unsigned char b = (unsigned char)s[i];
    size_t        seq = 1;
    unsigned long cp = b;

    if((b & 0xE0) == 0xC0) {
      seq = 2;
      cp = b & 0x1F;
    } else if((b & 0xF0) == 0xE0) {
      seq = 3;
      cp = b & 0x0F;
    } else if((b & 0xF8) == 0xF0) {
      seq = 4;
      cp = b & 0x07;
    } else if(b >= 0x80) {
      seq = 0; /* bare continuation byte or invalid lead */
    }

    int valid = seq > 0 && i + seq <= len;
    if(valid) {
      for(size_t k = 1; k < seq; k++) {
        unsigned char cont = (unsigned char)s[i + k];
        if((cont & 0xC0) != 0x80) {
          valid = 0;
          break;
        }
        cp = (cp << 6) | (cont & 0x3F);
      }
      /* Overlong forms are not valid UTF-8, and a consumer that accepts them
       * would still decode a C1 control from C0 9B / E0 80 80. Surrogates and
       * code points past U+10FFFF are invalid too. Treat all of them as
       * invalid so each byte is escaped. */
      if(valid) {
        if((seq == 2 && cp < 0x80) || (seq == 3 && cp < 0x800) ||
           (seq == 4 && cp < 0x10000) || cp > 0x10FFFF ||
           (cp >= 0xD800 && cp <= 0xDFFF))
          valid = 0;
      }
    }

    int escape = 0;
    if(b < 0x20 || b == 0x7f) {
      escape = 1; /* C0 and DEL */
    } else if(valid && cp >= 0x80 && cp <= 0x9f) {
      escape = 1; /* C1, including its UTF-8 form */
    } else if(!valid) {
      escape = 1; /* not a well-formed sequence: make the bytes inert */
    }

    if(escape) {
      if(seq > 1 && valid) {
        /* Escape the whole sequence, so neither byte is left raw. */
        for(size_t k = 0; k < seq; k++) {
          unsigned char e = (unsigned char)s[i + k];
          out[n++] = '\\';
          out[n++] = 'x';
          out[n++] = hex[e >> 4];
          out[n++] = hex[e & 15];
        }
        i += seq;
      } else {
        out[n++] = '\\';
        out[n++] = 'x';
        out[n++] = hex[b >> 4];
        out[n++] = hex[b & 15];
        i++;
      }
    } else {
      memcpy(out + n, s + i, seq);
      n += seq;
      i += seq;
    }
  }
  out[n] = '\0';
  return out;
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
//
// This used to be copy-pasted in three places (here, server.c and
// bialet_wren.c); it lives here now and the others call it.
int open_fd_no_follow(const char* path) {
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
    // Fail rather than truncate. Silently shortening an over-long component
    // would open a *different* entry than the caller asked for -- a name that
    // the containment check above never validated.
    if(comp_len >= sizeof(comp)) {
      close(dirfd);
      return -1;
    }
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

// Reads the whole file behind [fd] into a NUL-terminated heap buffer, closing
// [fd] either way. Accepts fd < 0 so callers can chain it onto
// open_fd_no_follow() directly.
char* read_file_fd(int fd) {
  if(fd < 0)
    return NULL;
  struct stat st;
  if(fstat(fd, &st) != 0 || st.st_size < 0) {
    close(fd);
    return NULL;
  }
  size_t len = (size_t)st.st_size;
  char*  buffer = (char*)malloc(len + 1);
  if(buffer == NULL) {
    close(fd);
    return NULL;
  }
  size_t got = 0;
  while(got < len) {
    ssize_t n = read(fd, buffer + got, len - got);
    if(n <= 0)
      break;
    got += (size_t)n;
  }
  close(fd);
  buffer[got] = '\0';
  return buffer;
}
#endif

// Returns 1 when any path component of [uri] starts with '_' or '.', closing
// the private-file boundary for "//_db.sqlite3", "/sub/.env" and
// "/./..." style requests that the old prefix/strstr checks missed.
//
// RFC 8615 carves out a single exception: when the *first* segment is exactly
// ".well-known", the whole subtree below it is allowed. That namespace is
// public by design (ACME, OAuth/OIDC, WebFinger, security.txt); every other
// dotfile/dotfolder keeps the 403. Matching is case-sensitive per the RFC, so
// ".Well-Known" is not exempt, and it is a path-prefix rule, not per-segment:
// "/foo/.well-known/x" stays forbidden.
int has_forbidden_uri_component(const char* uri) {
  if(!uri)
    return 0;
  const char* c = uri;
  while(*c == '/' || *c == '\\')
    c++;
  if(strncmp(c, ".well-known", 11) == 0 &&
     (c[11] == '\0' || c[11] == '/' || c[11] == '\\'))
    return 0;
  return has_private_uri_component(c);
}

// Same rule without the RFC 8615 exception. Startup guards use this: a log or
// database file placed under .well-known would be served publicly (that
// namespace is exempt from the ignore list too), which defeats the guard.
int has_private_uri_component(const char* uri) {
  if(!uri)
    return 0;
  const char* c = uri;
  while(*c) {
    while(*c == '/' || *c == '\\')
      c++;
    if(!*c)
      break;
    if(*c == '_' || *c == '.')
      return 1;
    while(*c && *c != '/' && *c != '\\')
      c++;
  }
  return 0;
}

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
