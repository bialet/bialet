/*
 * This file is part of Bialet, which is licensed under the
 * MIT License.
 *
 * Copyright (c) 2023-2026 Rodrigo Arce
 *
 * SPDX-License-Identifier: MIT
 *
 * For full license text, see LICENSE.md.
 */
#include "server.h"

#include "bialet.h"
#include "bialet_wren.h"
#include "favicon.h"
#include "livereload.h"
#include "messages.h"
#include "utils.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#define bialet_socket_t SOCKET
#define BIALET_INVALID_SOCKET INVALID_SOCKET
#define socket_close(s) closesocket(s)
#define setsockopt_val(v) ((const char*)(v))
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#if IS_MAC
#include <mach/mach_time.h>
#endif
typedef int bialet_socket_t;
#define BIALET_INVALID_SOCKET (-1)
#define socket_close(s) close(s)
#define setsockopt_val(v) (v)
#endif

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef NAME_MAX
#define NAME_MAX 255
#endif

#define BUFFER_SIZE (BUFSIZ * 4)
#define PATH_SIZE (1024 * 2)

// Idle/read/write timeout for accepted client sockets. The server is
// single-threaded, so a stalled peer must not be able to park it forever.
// Kept well below the old 15s so a slowloris peer needs to reconnect far more
// often to hold the accept/handle loop, and any single stalled request is
// bounded to a few seconds.
#define BIALET_SOCKET_TIMEOUT_MS (5000)

// Total wall-clock budget for reading a request body. The per-recv SO_RCVTIMEO
// only bounds a single recv() call, so a peer that dribbles bytes just under
// that timeout could otherwise hold the single-threaded accept loop forever.
#define BIALET_BODY_READ_DEADLINE_MS (30000)

bialet_socket_t            server_fd = BIALET_INVALID_SOCKET;
static struct BialetConfig bialet_config;

// Portable case-insensitive compare of exactly [n] bytes.
static int ci_ncmp(const char* a, const char* b, size_t n) {
  for(size_t i = 0; i < n; i++) {
    int ca = tolower((unsigned char)a[i]);
    int cb = tolower((unsigned char)b[i]);
    if(ca != cb)
      return ca - cb;
  }
  return 0;
}

// Bounded search for the CRLFCRLF terminating the header block. Returns the
// offset of the first body byte, or 0 when the terminator has not arrived yet.
// The request buffer holds attacker-controlled bytes that may contain NULs
// (file uploads), so every scan here is length-bounded rather than str*().
static size_t header_block_len(const char* buf, size_t len) {
  if(len < 4)
    return 0;
  for(size_t i = 0; i + 4 <= len; i++) {
    if(memcmp(buf + i, "\r\n\r\n", 4) == 0)
      return i + 4;
  }
  return 0;
}

// Returns a pointer to the value of header [name] within the header block, or
// NULL. Matching is anchored to the start of a header line and the request
// line is skipped: an unanchored search (the old stristr over the whole
// buffer) accepted "Content-Length:" appearing in the request line or body,
// so "GET /?x=Content-Length:50000" made the single-threaded server wait out
// the full body deadline for bytes that were never coming. Leading whitespace
// before the field name is not accepted either, which is what stops request
// smuggling via " Content-Length:".
static const char* find_header(const char* buf, size_t hdr_len, const char* name,
                               size_t name_len) {
  const char* p = buf;
  const char* end = buf + hdr_len;

  while(p + 1 < end && !(p[0] == '\r' && p[1] == '\n'))
    p++;
  if(p + 1 >= end)
    return NULL;
  p += 2;

  while(p < end) {
    const char* eol = p;
    while(eol + 1 < end && !(eol[0] == '\r' && eol[1] == '\n'))
      eol++;
    size_t line_len = (size_t)(eol - p);
    if(line_len > name_len && p[name_len] == ':' && ci_ncmp(p, name, name_len) == 0)
      return p + name_len + 1;
    if(eol + 1 >= end)
      break;
    p = eol + 2;
  }
  return NULL;
}

// Strict unsigned decimal parse of a Content-Length value. Returns 0 on
// success, 1 when well-formed but above [max], -1 when malformed. strtol()
// previously accepted leading signs and stopped at the first non-digit, so
// "Content-Length:50000junk" parsed as 50000.
static int parse_content_length(const char* v, size_t max, size_t* out) {
  while(*v == ' ' || *v == '\t')
    v++;
  if(*v < '0' || *v > '9')
    return -1;
  size_t n = 0;
  while(*v >= '0' && *v <= '9') {
    size_t digit = (size_t)(*v - '0');
    if(n > (SIZE_MAX - digit) / 10)
      return -1;
    n = n * 10 + digit;
    v++;
  }
  while(*v == ' ' || *v == '\t')
    v++;
  if(*v != '\r' && *v != '\n' && *v != '\0')
    return -1;
  if(n > max)
    return 1;
  *out = n;
  return 0;
}

// Returns 1 when any path component of [uri] starts with '_' or '.', closing
// the private-file boundary for "//_db.sqlite3", "/sub/_route.wren" and
// "/./..." style requests that the old prefix/strstr checks missed.
static int has_forbidden_uri_component(const char* uri) {
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

// Returns a pointer to the final component of [path], honoring both POSIX and
// Windows separators. Never returns a pointer to a separator itself.
static const char* path_basename(const char* path) {
  const char* last = path;
  for(const char* p = path; *p; p++) {
    if(*p == '/' || *p == '\\')
      last = p + 1;
  }
  return last;
}

// Returns 1 when [path] is a regular file without following a symlink in the
// final component. On POSIX lstat reports the link itself; on Windows stat()
// may still follow a junction, so the resolved-path basename check remains the
// real boundary there.
static int is_regular_file_no_follow(const char* path, struct stat* st) {
#ifdef _WIN32
  return stat(path, st) == 0 && S_ISREG(st->st_mode);
#else
  return lstat(path, st) == 0 && S_ISREG(st->st_mode);
#endif
}

static ssize_t send_all(bialet_socket_t fd, const void* buf, size_t count) {
  size_t      sent = 0;
  const char* p = (const char*)buf;
  while(sent < count) {
    ssize_t n = send(fd, p + sent, count - sent, 0);
    if(n < 0)
      return n; // error (including SO_SNDTIMEO expiry)
    if(n == 0)
      break; // peer closed
    sent += (size_t)n;
  }
  return (ssize_t)sent;
}

// Applies receive/send timeouts so a half-open or stalling connection is
// dropped after BIALET_SOCKET_TIMEOUT_MS instead of blocking the
// single-threaded accept/handle loop forever.
static void set_socket_timeout(bialet_socket_t fd) {
#ifdef _WIN32
  DWORD timeout_ms = BIALET_SOCKET_TIMEOUT_MS;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, setsockopt_val(&timeout_ms),
             sizeof(timeout_ms));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, setsockopt_val(&timeout_ms),
             sizeof(timeout_ms));
#else
  struct timeval tv;
  tv.tv_sec = BIALET_SOCKET_TIMEOUT_MS / 1000;
  tv.tv_usec = (BIALET_SOCKET_TIMEOUT_MS % 1000) * 1000;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
}

static long long monotonic_ms(void) {
#ifdef _WIN32
  return (long long)GetTickCount64();
#elif IS_MAC
  static mach_timebase_info_data_t timebase;
  if(timebase.denom == 0)
    mach_timebase_info(&timebase);
  return (long long)(mach_absolute_time() * timebase.numer / timebase.denom /
                     1000000);
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

// Returns 1 when [fd] becomes readable within timeout_ms, 0 on timeout or
// error. Used to enforce the total body-read deadline below.
static int wait_readable(bialet_socket_t fd, int timeout_ms) {
#ifdef _WIN32
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(fd, &readfds);
  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  int ret = select(0, &readfds, NULL, NULL, &tv);
  return ret > 0 && FD_ISSET(fd, &readfds);
#else
  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = POLLIN;
  int ret = poll(&pfd, 1, timeout_ms);
  return ret > 0 && (pfd.revents & POLLIN);
#endif
}

// Consumes the client's request body after an early rejection (e.g. HTTP 413
// for an oversized body). Closing the socket while unread bytes sit in the
// receive buffer makes the kernel emit RST, which discards the response we
// already sent -- clients would see an empty body. Read and discard the
// remaining bytes within a bounded deadline so a slow-drip peer cannot stall
// the single-threaded server.
static void drain_request_body(bialet_socket_t fd, size_t bytes_remaining) {
  long long deadline = monotonic_ms() + BIALET_BODY_READ_DEADLINE_MS;
  char      drain_buf[BUFFER_SIZE];
  while(bytes_remaining > 0) {
    long long remaining = deadline - monotonic_ms();
    if(remaining <= 0)
      break;
    if(!wait_readable(fd, (int)remaining))
      break;
    size_t want =
        bytes_remaining < sizeof(drain_buf) ? bytes_remaining : sizeof(drain_buf);
    ssize_t n = recv(fd, drain_buf, want, 0);
    if(n <= 0)
      break;
    bytes_remaining -= (size_t)n;
  }
}

void handle_client(bialet_socket_t client_socket);

int start_server(struct BialetConfig* config) {
#ifdef _WIN32
  WSADATA wsaData;
  if(WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    fprintf(stderr, "WSAStartup failed\n");
    exit(EXIT_FAILURE);
  }
#endif
  bialet_config = *config;
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(server_fd == BIALET_INVALID_SOCKET) {
    perror("Failed to create socket");
    exit(EXIT_FAILURE);
  }
  // Enable SO_REUSEADDR option
  int opt = 1;
  if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, setsockopt_val(&opt),
                sizeof(opt)) == -1) {
    perror("Failed to set SO_REUSEADDR");
    socket_close(server_fd);
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in server_addr = {
      .sin_family = AF_INET,
      .sin_addr.s_addr = inet_addr(config->host),
      .sin_port = htons(config->port),
  };
  int port;
  int initial_port = config->port < 0 ? BIALET_DEFAULT_PORT : config->port;
  int max_retries = config->port < 0 ? 10 : 1;

  for(int retries = 0; retries < max_retries; retries++) {
    port = initial_port + retries;
    server_addr.sin_port = htons(port);
    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
      continue;
    }
    if(listen(server_fd, 10) == -1) {
      continue;
    }
    return port;
  }
  if(max_retries == 1) {
    char port_str[10];
    snprintf(port_str, sizeof(port_str), "%d", port);
    message(red("Could not bind port"), magenta(port_str),
            "- Check if the port is already in use.");
  } else {
    message(red("Could not open server"));
    char initial_port_str[10];
    snprintf(initial_port_str, sizeof(initial_port_str), "%d", config->port);
    char last_port_str[10];
    snprintf(last_port_str, sizeof(last_port_str), "%d", port);
    message("Ports from", magenta(initial_port_str), "to", magenta(last_port_str),
            "tried");
  }
  socket_close(server_fd);
  exit(EXIT_FAILURE);
  return -1;
}

void stop_server() {
  if(server_fd != BIALET_INVALID_SOCKET) {
    socket_close(server_fd);
    server_fd = BIALET_INVALID_SOCKET;
    message(magenta("Server stopped"));
  }
}

struct String create_string(const char* str, size_t len) {
  struct String s;
  s.len = len;
  s.str = (char*)malloc(len + 1);
  if(s.str == NULL) {
    perror("Failed to allocate memory for string");
    exit(EXIT_FAILURE);
  }
  memcpy(s.str, str, len);
  s.str[len] = '\0';
  return s;
}

void clean_http_message(struct HttpMessage* hm) {
  if(!hm)
    return;
  free(hm->message.str);
  free(hm->method.str);
  free(hm->uri.str);
  free(hm->routes.str);
  free(hm);
  hm = NULL;
}

const char* get_http_status_description(int status_code) {
  switch(status_code) {
    case 100:
      return "Continue";
    case 101:
      return "Switching Protocols";
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 202:
      return "Accepted";
    case 203:
      return "Non-Authoritative Information";
    case 204:
      return "No Content";
    case 205:
      return "Reset Content";
    case 206:
      return "Partial Content";
    case 300:
      return "Multiple Choices";
    case 301:
      return "Moved Permanently";
    case 302:
      return "Found";
    case 303:
      return "See Other";
    case 304:
      return "Not Modified";
    case 307:
      return "Temporary Redirect";
    case 308:
      return "Permanent Redirect";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 413:
      return "Payload Too Large";
    case 429:
      return "Too Many Requests";
    case 405:
      return "Method Not Allowed";
    case 406:
      return "Not Acceptable";
    case 407:
      return "Proxy Authentication Required";
    case 408:
      return "Request Timeout";
    case 409:
      return "Conflict";
    case 410:
      return "Gone";
    case 500:
      return "Internal Server Error";
    case 501:
      return "Not Implemented";
    case 502:
      return "Bad Gateway";
    case 503:
      return "Service Unavailable";
    case 504:
      return "Gateway Timeout";
    case 505:
      return "HTTP Version Not Supported";
    default:
      return "Unknown Status";
  }
}

char* get_content_type(const char* path) {
  const char* ext = strrchr(path, '.');
  if(!ext) {
    return (char*)"Content-Type: application/octet-stream\r\n";
  }
  if(strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) {
    return BIALET_HEADERS;
  } else if(strcmp(ext, ".css") == 0) {
    return (char*)"Content-Type: text/css\r\n";
  } else if(strcmp(ext, ".js") == 0) {
    return (char*)"Content-Type: application/javascript\r\n";
  } else if(strcmp(ext, ".json") == 0) {
    return (char*)"Content-Type: application/json\r\n";
  } else if(strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
    return (char*)"Content-Type: image/jpeg\r\n";
  } else if(strcmp(ext, ".png") == 0) {
    return (char*)"Content-Type: image/png\r\n";
  } else if(strcmp(ext, ".gif") == 0) {
    return (char*)"Content-Type: image/gif\r\n";
  } else if(strcmp(ext, ".svg") == 0) {
    return (char*)"Content-Type: image/svg+xml\r\n";
  } else if(strcmp(ext, ".txt") == 0) {
    return (char*)"Content-Type: text/plain\r\n";
  } else if(strcmp(ext, ".xml") == 0) {
    return (char*)"Content-Type: application/xml\r\n";
  } else if(strcmp(ext, ".pdf") == 0) {
    return (char*)"Content-Type: application/pdf\r\n";
  }
  return (char*)"Content-Type: application/octet-stream\r\n";
}

// Zeroed so that `headers` -- declared in struct HttpMessage but not populated
// on this path -- is never left indeterminate. Returns NULL on allocation
// failure so the caller can fail the single request instead of exiting the
// whole server.
struct HttpMessage* parse_request(char* request, size_t length) {
  struct HttpMessage* hm =
      (struct HttpMessage*)calloc(1, sizeof(struct HttpMessage));
  if(hm == NULL) {
    perror("Failed to allocate memory for HttpMessage");
    return NULL;
  }

  char   first_line[BUFFER_SIZE];
  char*  line_end = strstr(request, "\r\n");
  size_t line_len = line_end ? (size_t)(line_end - request) : strlen(request);
  if(line_len >= sizeof(first_line))
    line_len = sizeof(first_line) - 1;
  memcpy(first_line, request, line_len);
  first_line[line_len] = '\0';

  hm->message = create_string(request, length);

  // Tokenize the request line from the bounded copy above.
  char* saveptr = NULL;
  char* method = strtok_r(first_line, " ", &saveptr);
  if(!method)
    method = (char*)"GET";
  hm->method = create_string(method, strlen(method));

  char* url = strtok_r(NULL, " ", &saveptr);
  if(!url)
    url = (char*)"/";
  hm->uri = create_string(url, strlen(url));

  hm->routes = create_string("", 0);

  return hm;
}

void write_response(int client_socket, struct BialetResponse* response) {
  if(!response->status) {
    custom_error(404, response);
  }

  // `length` is authoritative. Falling back to strlen() is only safe for a
  // body this struct owns, which is always NUL-terminated by construction; an
  // opaque caller-owned buffer may be a zero-length or unterminated allocation.
  size_t body_len = response->length;
  if(body_len == 0 && response->body != NULL && response->body_owned) {
    body_len = strlen(response->body);
  }

  const char* desc = get_http_status_description(response->status);
  const char* hdr = response->header ? response->header : "";

  // %zu rather than %lu: unsigned long is 32-bit under Windows LLP64.
  int needed = snprintf(NULL, 0,
                        "HTTP/1.1 %d %s\r\n"
                        "%s"
                        "Content-Length: %zu\r\n\r\n",
                        response->status, desc, hdr, body_len);
  if(needed < 0) {
    perror("Failed to format HTTP response");
    socket_close(client_socket); // was leaked on this path
    return;
  }

  char* message = (char*)malloc((size_t)needed + 1);
  if(message == NULL) {
    perror("Failed to allocate memory for HTTP response");
    socket_close(client_socket);
    return;
  }
  snprintf(message, (size_t)needed + 1,
           "HTTP/1.1 %d %s\r\n"
           "%s"
           "Content-Length: %zu\r\n\r\n",
           response->status, desc, hdr, body_len);

  (void)send_all(client_socket, message, (size_t)needed);
  free(message);

  if(response->body && body_len > 0) {
    (void)send_all(client_socket, response->body, body_len);
  }

  socket_close(client_socket);
}

// Frees the heap-allocated body/header of a response when the ownership flags
// indicate they belong to the struct (static strings and buffers owned by
// other code, e.g. file_content, are left untouched).
static void free_response_owned(struct BialetResponse* response) {
  if(response->body_owned) {
    free(response->body);
    response->body = NULL;
    response->body_owned = 0;
  }
  if(response->header_owned) {
    free(response->header);
    response->header = NULL;
    response->header_owned = 0;
  }
}

// Opens an absolute, root-contained path with O_NOFOLLOW applied to every
// component. A single O_NOFOLLOW on the final component (as open() alone
// provides) leaves a TOCTOU window: a directory component swapped for a
// symlink between the realpath() containment check and this open would escape
// the root. Walking the path component-by-component with openat(2) closes that
// window -- any symlink swap now fails with ELOOP instead of being followed.
// Returns an open file descriptor or -1.
#ifndef _WIN32
static int open_fd_without_follow(const char* path) {
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

// Opens [path] without following a symlink as any component. On POSIX the
// caller is expected to have already validated [path] with realpath() for root
// containment; O_NOFOLLOW per component closes the TOCTOU window where the
// file or an intermediate directory is swapped for an out-of-root symlink
// between the check and the open. On Windows the shared helper opens with
// FILE_FLAG_OPEN_REPARSE_POINT and rejects reparse points (junctions/symlinks)
// instead of following them.
static FILE* open_file_within_root(const char* path) {
#ifdef _WIN32
  return open_file_no_follow(path);
#else
  int fd = open_fd_without_follow(path);
  if(fd < 0)
    return NULL;
  FILE* f = fdopen(fd, "rb");
  if(f == NULL)
    close(fd);
  return f;
#endif
}

// Resolves [path] to an absolute path inside the application root, writing the
// result into [resolved] (PATH_SIZE). Returns 1 on success, 0 when the path
// cannot be resolved or escapes the root (symlinked outside it).
static int resolve_within_root(const char* path, char* resolved) {
  if(realpath_n(path, resolved, PATH_SIZE) == NULL)
    return 0;
  size_t root_len = strlen(bialet_config.full_root_dir);
  if(strncmp(resolved, bialet_config.full_root_dir, root_len) != 0 ||
     (resolved[root_len] != '/' && resolved[root_len] != '\\' &&
      resolved[root_len] != '\0')) {
    return 0;
  }
  return 1;
}

void handle_client(bialet_socket_t client_socket) {
  char   buffer[BUFFER_SIZE];
  size_t total_read = 0;
  size_t hdr_len = 0;

  // Read until the header block is complete. Taking whatever the first recv()
  // returned meant headers split across two TCP segments -- routine for any
  // real client -- were parsed as a truncated request. Bounded by the same
  // wall-clock deadline used for bodies so a slow-drip peer cannot park the
  // accept loop.
  long long deadline = monotonic_ms() + BIALET_BODY_READ_DEADLINE_MS;
  for(;;) {
    long long remaining = deadline - monotonic_ms();
    if(remaining <= 0 || total_read >= sizeof(buffer) - 1)
      break;
    if(!wait_readable(client_socket, (int)remaining))
      break;
    ssize_t n =
        recv(client_socket, buffer + total_read, sizeof(buffer) - 1 - total_read, 0);
    if(n <= 0)
      break;
    total_read += (size_t)n;
    hdr_len = header_block_len(buffer, total_read);
    if(hdr_len != 0)
      break;
  }
  if(total_read == 0) {
    // A peer that simply closed sets no errno, so perror() here only printed a
    // stale, misleading message.
    socket_close(client_socket);
    return;
  }
  buffer[total_read] = '\0';

  char*  full_request = buffer;
  size_t content_length = 0;
  int    should_free_request = 0;

  // Content-Length is read from the header block only, anchored to a line
  // start (see find_header).
  if(hdr_len != 0) {
    static const char kContentLength[] = "Content-Length:";
    const char*       cl =
        find_header(buffer, hdr_len, kContentLength, sizeof(kContentLength) - 1);
    if(cl != NULL) {
      int rc =
          parse_content_length(cl, bialet_config.max_post_size, &content_length);
      if(rc != 0) {
        // Reject oversized or malformed bodies with a proper HTTP 413 instead
        // of dropping the connection: reading the body into Wren would blow
        // the memory limit. Drain the already-declared body first so the
        // socket closes with a clean FIN and the client actually receives the
        // 413 page.
        if(rc > 0) {
          size_t already = total_read - hdr_len;
          size_t declared = 0;
          if(parse_content_length(cl, SIZE_MAX, &declared) == 0 &&
             already < declared) {
            drain_request_body(client_socket, declared - already);
          }
        }
        struct BialetResponse too_large = {0};
        custom_error(413, &too_large);
        write_response(client_socket, &too_large);
        free_response_owned(&too_large);
        return;
      }
    }
  }

  // Read the remainder of the body, if any.
  if(content_length > 0 && hdr_len != 0) {
    size_t full_size = hdr_len + content_length;
    if(total_read < full_size) {
      full_request = (char*)malloc(full_size + 1);
      if(!full_request) {
        perror("Failed to allocate memory for large request");
        socket_close(client_socket);
        return;
      }
      should_free_request = 1;
      memcpy(full_request, buffer, total_read);

      while(total_read < full_size) {
        long long remaining = deadline - monotonic_ms();
        if(remaining <= 0)
          break;
        if(!wait_readable(client_socket, (int)remaining))
          break;
        ssize_t n = recv(client_socket, full_request + total_read,
                         full_size - total_read, 0);
        if(n <= 0)
          break;
        total_read += (size_t)n;
      }

      // The body did not fully arrive (peer closed early or the deadline
      // expired): reject the request instead of processing a truncated one.
      if(total_read < full_size) {
        free(full_request);
        socket_close(client_socket);
        return;
      }
      full_request[total_read] = '\0';
    }
  }

  struct HttpMessage* hm = parse_request(full_request, total_read);
  if(hm == NULL) {
    if(should_free_request)
      free(full_request);
    socket_close(client_socket);
    return;
  }
  if(!livereload_is_poll(hm->uri.str))
    message(magenta("Request"), hm->method.str, hm->uri.str);

  if(strcmp("/favicon.ico", hm->uri.str) == 0) {
    (void)send_all(client_socket, FAVICON_RESPONSE, strlen(FAVICON_RESPONSE));
    (void)send_all(client_socket, favicon_data, FAVICON_SIZE);
    clean_http_message(hm);
    if(should_free_request)
      free(full_request);
    socket_close(client_socket);
    return;
  }

  {
    struct BialetResponse lr_response = {0, "", "", 0, 0, 0};
    if(livereload_try_handle(hm->uri.str, &lr_response)) {
      write_response(client_socket, &lr_response);
      clean_http_message(hm);
      if(should_free_request)
        free(full_request);
      return;
    }
  }

  struct BialetResponse response = {0, "", "", 0, 0, 0};
  char                  path[PATH_SIZE];
  char                  wren_path[PATH_SIZE + 5];
  struct stat           file_stat;
  int                   private_path_internal = 0;

  // Reject any URI whose decoded path component starts with '_' or '.'
  // (private files such as _db.sqlite3, _route.wren, .env). This runs before
  // any stat/fopen so "/sub/_route.wren" and "//_db.sqlite3" cannot bypass it.
  if(has_forbidden_uri_component(hm->uri.str)) {
    clean_http_message(hm);
    custom_error(403, &response);
    write_response(client_socket, &response);
    free_response_owned(&response);
    if(should_free_request)
      free(full_request);
    return;
  }

  snprintf(path, PATH_SIZE, "%s%s", bialet_config.root_dir, hm->uri.str);
  // Remove query parameters if present
  char* query_start = strchr(path, '?');
  if(query_start) {
    *query_start = '\0'; // Truncate at '?'
  }

  // Check for directory traversal patterns in the URI before processing
  if(strstr(hm->uri.str, "..") != NULL) {
    message(red("Security Error"), "Path traversal attempt blocked", hm->uri.str);
    clean_http_message(hm);
    custom_error(403, &response);
    write_response(client_socket, &response);
    free_response_owned(&response);
    if(should_free_request)
      free(full_request);
    return;
  }

  // Handle routes ending with "/" or without
  size_t pathlen = strlen(path);
  if(pathlen > 0 && path[pathlen - 1] == '/') {
    path[pathlen - 1] = '\0';
  }

  if(strlen(path) + 5 < PATH_SIZE) { // 5 accounts for ".wren" and null terminator
    snprintf(wren_path, PATH_SIZE + 5, "%s.wren", path);
    if(stat(wren_path, &file_stat) == 0) {
      strncpy(path, wren_path, PATH_SIZE - 1);
      path[PATH_SIZE - 1] = '\0'; // Ensure null termination
    }
  } else {
    perror("Path too long to append .wren suffix");
  }

  if(stat(path, &file_stat) == 0 && S_ISDIR(file_stat.st_mode)) {
    // Serve index.html or index.wren
    strncat(path, "/index.wren", PATH_SIZE - strlen(path) - 1);
    if(stat(path, &file_stat) != 0) {
      // Reemplazar sufijo ".wren" por ".html"
      size_t L = strlen(path);
      if(L >= 5) {
        strncpy(path + L - 5, ".html", 6);
      }
    }
  }

  if(stat(path, &file_stat) != 0) {
    // Search for _route.wren
    char* url_copy = strdup(hm->uri.str);
    if(!url_copy) {
      perror("strdup");
      clean_http_message(hm);
      if(should_free_request)
        free(full_request);
      socket_close(client_socket);
      return;
    }
    while(1) {
      snprintf(path, PATH_SIZE, "%s%s/_route.wren", bialet_config.root_dir,
               url_copy);
      // lstat/no-follow: a planted sub/_route.wren -> ../_db.sqlite3 must not
      // be accepted as a route file, otherwise realpath resolves it to the
      // database and private_path_internal waives the private-file check.
      if(is_regular_file_no_follow(path, &file_stat)) {
        hm->routes = create_string(url_copy, strlen(url_copy));
        private_path_internal = 1;
        break;
      }
      char* last_slash = strrchr(url_copy, '/');
      if(!last_slash) { // Stop if root is reached
        free(url_copy);
        // If no index.wren or index.html or _route.wren in root
        // serve welcome page
        if(strncmp(hm->uri.str, "/", 2) == 0) {
          response.status = 200;
          response.body = BIALET_WELCOME_PAGE;
          response.length = strlen(BIALET_WELCOME_PAGE);
          response.header = BIALET_HEADERS;
        }
        (void)livereload_inject_response(&response);
        clean_http_message(hm);
        write_response(client_socket, &response);
        free_response_owned(&response);
        if(should_free_request)
          free(full_request);
        return;
      }
      *last_slash = '\0'; // Truncate to parent directory
    }
    free(url_copy);
  }

  // Validate final path is within root_dir before opening file
  char resolved_path[PATH_SIZE];
  if(realpath_n(path, resolved_path, sizeof(resolved_path)) != NULL) {
    size_t root_len = strlen(bialet_config.full_root_dir);
    if(strncmp(resolved_path, bialet_config.full_root_dir, root_len) != 0 ||
       (resolved_path[root_len] != '/' && resolved_path[root_len] != '\\' &&
        resolved_path[root_len] != '\0')) {
      message(red("Security Error"), "Path traversal blocked", path);
      clean_http_message(hm);
      custom_error(403, &response);
      write_response(client_socket, &response);
      free_response_owned(&response);
      if(should_free_request)
        free(full_request);
      return;
    }
    // Re-apply the private-file rule to the resolved target. A symlink named
    // "x" -> "_db.sqlite3" passes the URI check, so the canonical path must
    // be validated too. Only the root-relative portion is checked so an app
    // hosted under a "_"/"."-named directory is not blocked. The framework's
    // own _route.wren (found only through the lstat-based route search) is
    // exempt, and only when the resolved basename is exactly "_route.wren" so
    // a planted sub/_route.wren -> ../_db.sqlite3 cannot waive the boundary.
    int route_wren_waived = private_path_internal &&
                            strcmp(path_basename(resolved_path), "_route.wren") == 0;
    if(!route_wren_waived && has_forbidden_uri_component(resolved_path + root_len)) {
      message(red("Security Error"), "Private file access blocked", path);
      clean_http_message(hm);
      custom_error(403, &response);
      write_response(client_socket, &response);
      free_response_owned(&response);
      if(should_free_request)
        free(full_request);
      return;
    }
    // Use the verified resolved path
    strncpy(path, resolved_path, PATH_SIZE - 1);
    path[PATH_SIZE - 1] = '\0';
  }

  // Open file and read content
  FILE* file = open_file_within_root(path);
  if(file == NULL) {
    perror("Error opening file");
    clean_http_message(hm);
    if(should_free_request)
      free(full_request);
    socket_close(client_socket);
    return;
  }
  if(fseek(file, 0, SEEK_END) != 0) {
    perror("fseek");
    fclose(file);
    clean_http_message(hm);
    if(should_free_request)
      free(full_request);
    socket_close(client_socket);
    return;
  }
  long file_size_l = ftell(file);
  if(file_size_l < 0) {
    perror("ftell");
    fclose(file);
    clean_http_message(hm);
    if(should_free_request)
      free(full_request);
    socket_close(client_socket);
    return;
  }
  size_t file_size = (size_t)file_size_l;
  rewind(file);

  unsigned char is_wren_file = 0;
  if(strstr(path, ".wren") != NULL) {
    is_wren_file = 1;
  }

  // Always reserve room for a terminator, for wren and non-wren files alike.
  // A zero-byte static file previously produced malloc(0), and the
  // `length == 0 -> strlen(body)` fallback in write_response then read out of
  // bounds to compute Content-Length and sent whatever heap bytes followed.
  size_t alloc_size = file_size + 1;
  char*  file_content = malloc(alloc_size);
  if(file_content == NULL) {
    perror("Error allocating memory for file content");
    fclose(file);
    clean_http_message(hm);
    if(should_free_request)
      free(full_request);
    socket_close(client_socket);
    return;
  }
  size_t read_bytes = fread(file_content, 1, file_size, file);
  if(read_bytes != file_size) {
    perror("Error reading file");
    free(file_content);
    fclose(file);
    clean_http_message(hm);
    if(should_free_request)
      free(full_request);
    socket_close(client_socket);
    return;
  }
  fclose(file);
  file_content[read_bytes] = '\0';

  if(is_wren_file) {
    response = bialet_run(path, file_content, hm);
    if(response.length == 0 && response.body) {
      response.length = strlen(response.body);
    }
  } else {
    response.status = 200;
    response.body = file_content;
    response.length = read_bytes;
    response.header = get_content_type(path);
  }

  (void)livereload_inject_response(&response);
  clean_http_message(hm);
  write_response(client_socket, &response);
  free(file_content);
  free_response_owned(&response);

  // Free allocated memory if we had to read a large request
  if(should_free_request)
    free(full_request);
}

int server_poll(int delay) {
#ifdef _WIN32
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(server_fd, &readfds);
  struct timeval tv;
  tv.tv_sec = delay / 1000;
  tv.tv_usec = (delay % 1000) * 1000;
  int ret = select(0, &readfds, NULL, NULL, delay >= 0 ? &tv : NULL);
  if(ret < 0) {
    if(server_fd != BIALET_INVALID_SOCKET) {
      perror("Select error");
    }
    return -1;
  } else if(ret == 0) {
    return 0;
  }
#else
  struct pollfd fds[1];
  fds[0].fd = server_fd;
  fds[0].events = POLLIN;

  int poll_result = poll(fds, 1, delay);
  if(poll_result < 0) {
    if(server_fd != BIALET_INVALID_SOCKET) {
      perror("Poll error");
    }
    return -1;
  } else if(poll_result == 0) {
    return 0;
  }

  if(!(fds[0].revents & POLLIN)) {
    return 0;
  }
#endif

  bialet_socket_t client_socket = accept(server_fd, NULL, NULL);
  if(client_socket == BIALET_INVALID_SOCKET) {
    perror("Failed to accept connection");
    return -1;
  }
  set_socket_timeout(client_socket);
  handle_client(client_socket);

  return 0;
}

static int custom_error_recursing = 0;

void custom_error(int status, struct BialetResponse* response) {
  // The caller may already own a heap body/header (e.g. the
  // Response.useErrorFallback path in bialet_run swaps a real response for a
  // fallback page); release them before overwriting so they are not leaked.
  free_response_owned(response);
  response->header = BIALET_HEADERS;
  response->header_owned = 0;
  response->status = status;
  char        path[PATH_SIZE];
  char        resolved_path[PATH_SIZE];
  struct stat file_stat;

  if(!custom_error_recursing) {
    snprintf(path, PATH_SIZE, "%s/%d.wren", bialet_config.root_dir, status);
    if(resolve_within_root(path, resolved_path) &&
       stat(resolved_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
      FILE* file = open_file_within_root(resolved_path);
      if(file != NULL) {
        if(fseek(file, 0, SEEK_END) == 0) {
          long file_size = ftell(file);
          if(file_size >= 0) {
            rewind(file);
            char* file_content = (char*)malloc((size_t)file_size + 1);
            if(file_content != NULL) {
              size_t read_bytes = fread(file_content, 1, (size_t)file_size, file);
              fclose(file);
              file_content[read_bytes] = '\0';
              custom_error_recursing = 1;
              struct BialetResponse wren_response =
                  bialet_run(resolved_path, file_content, NULL);
              custom_error_recursing = 0;
              free(file_content);
              if(wren_response.body && wren_response.length == 0)
                wren_response.length = strlen(wren_response.body);
              if(wren_response.status != 0) {
                *response = wren_response;
                if(!response->header) {
                  response->header = BIALET_HEADERS;
                  response->header_owned = 0;
                }
                return;
              } else {
                if(wren_response.body_owned)
                  free(wren_response.body);
                if(wren_response.header_owned)
                  free(wren_response.header);
              }
            } else {
              fclose(file);
            }
          } else {
            fclose(file);
          }
        } else {
          fclose(file);
        }
      }
    }
  }

  snprintf(path, PATH_SIZE, "%s/%d.html", bialet_config.root_dir, status);
  if(resolve_within_root(path, resolved_path) &&
     stat(resolved_path, &file_stat) == 0) {
    FILE* file = open_file_within_root(resolved_path);
    if(file != NULL) {
      if(fseek(file, 0, SEEK_END) == 0) {
        long file_size = ftell(file);
        if(file_size >= 0) {
          rewind(file);
          // +1 for the trailing NUL: an empty or short-read {status}.html
          // would otherwise leave the body unterminated and the strlen
          // fallback in write_response would over-read the heap.
          char* file_content = (char*)malloc((size_t)file_size + 1);
          if(file_content != NULL) {
            size_t read_bytes = fread(file_content, 1, (size_t)file_size, file);
            fclose(file);
            file_content[read_bytes] = '\0';
            response->body = file_content;
            response->body_owned = 1;
            response->length = read_bytes;
            return;
          }
        }
      }
      fclose(file);
    }
  }
  if(status == 404) {
    response->body = BIALET_NOT_FOUND_PAGE;
    response->body_owned = 0;
    response->length = strlen(BIALET_NOT_FOUND_PAGE);
  } else if(status == 403) {
    response->body = BIALET_FORBIDDEN_PAGE;
    response->body_owned = 0;
    response->length = strlen(BIALET_FORBIDDEN_PAGE);
  } else if(status == 413) {
    response->body = BIALET_PAYLOAD_TOO_LARGE_PAGE;
    response->body_owned = 0;
    response->length = strlen(BIALET_PAYLOAD_TOO_LARGE_PAGE);
  } else if(status == 429) {
    response->body = BIALET_TOO_MANY_REQUESTS_PAGE;
    response->body_owned = 0;
    response->length = strlen(BIALET_TOO_MANY_REQUESTS_PAGE);
  } else if(status == 500) {
    response->body = BIALET_ERROR_PAGE;
    response->body_owned = 0;
    response->length = strlen(BIALET_ERROR_PAGE);
  } else {
    response->body = (char*)"";
    response->body_owned = 0;
    response->length = 0;
  }
}
