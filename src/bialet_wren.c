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
#include "bialet_wren.h"

#include "bialet.h"
#include "http_call.h"
#include "livereload.h"
#include "messages.h"
#include "server.h"
#include "show_errors.h"
#include "utils.h"
#include "wren.h"
#include "wren_vm.h"
#include <ctype.h>
#include <sqlite3.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef _WIN32
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef NAME_MAX
#define NAME_MAX 255
#endif
#endif

#define BIALET_SQLITE_ERROR 11
#define BIALET_SQLITE_BUSY_TIMEOUT 5000
#define BIALET_SQLITE_JOURNAL_SIZE "67108864" // 64 mb
#define BIALET_SQLITE_MMAP_SIZE "134217728"   // 128 mb
#define BIALET_SQLITE_CACHE_SIZE "-10000"     // It's in kb, so 10 mb
#define MAX_URL_LEN 1024
#define MAX_LINE_ERROR_LEN 100
#define MAX_COLUMNS 100
#define MAX_MODULE_LEN 256
#define HTTP_OK 200
#define HTTP_ERROR 500
#define BIALET_FILE_CHAR 26
#define BIALET_EXTERNAL_MODULE_LEN 3
#define BIALET_MODULE_GITHUB_PREFIX "gh:"
#define BIALET_REMOTE_MODULE_GITHUB_URL                                             \
  "https://raw.githubusercontent.com/%s/%s/refs/heads/%s/%s" BIALET_EXTENSION
#define BIALET_REMOTE_MODULE_DEFAULT_BRANCH "main"

#define MAIN_MODULE_NAME "main"
#define MAIN_MODULE_SOURCE "Response.init\nDate.init(\"\")"
#define CLI_MODULE_NAME "bialet_cli"

// Maximum number of file parts accepted per multipart request. Without this
// cap a 10MB body split into tens of thousands of tiny parts would force that
// many synchronous INSERT statements and unbounded WAL/disk growth.
#define MAX_UPLOAD_FILES 100

WrenConfiguration          wren_config;
static struct BialetConfig bialet_config;
sqlite3*                   db;

// Test-runner failure capture. bialet_wren_error() is the global Wren error
// callback for every VM in the process (requests, migrations, cron, and
// -T test files alike); while test_capturing is set, it records the abort
// message and originating line here instead of printing, so run_test_file()
// can report the real reason a test failed instead of just a pass/fail bit.
// Tests run one WrenVM at a time on a single thread, so plain (non-thread-
// local) statics are enough.
#define TEST_FAIL_MSG_LEN 256
static int  test_capturing = 0;
static int  test_skip_requested = 0;
static int  test_fail_line = 0;
static char test_fail_msg[TEST_FAIL_MSG_LEN] = "";

static void test_capture_begin(void) {
  test_capturing = 1;
  test_skip_requested = 0;
  test_fail_line = 0;
  test_fail_msg[0] = '\0';
}

static void test_capture_end(void) {
  test_capturing = 0;
}

// Called by the Tests.skip() primitive (wren_core.c) so a test file can mark
// itself skipped instead of running assertions.
void bialet_test_mark_skip(void) {
  test_skip_requested = 1;
}

static void bialet_wren_write(WrenVM* vm, const char* message) {
  (void)vm;
  message(yellow("Log"), message);
  // A failed prepare (e.g. SQLITE_BUSY on the shared connection) leaves stmt
  // NULL; binding/stepping it would NULL-deref the request thread.
  sqlite3_stmt* stmt = NULL;
  if(sqlite3_prepare_v2(db, "INSERT INTO BIALET_LOGS (message) VALUES (?)", -1,
                        &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, message, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }
}

char* bialet_read_file(const char* path) {
  char fullPath[MAX_URL_LEN];
  int  ret = snprintf(fullPath, sizeof(fullPath), "%s/%s",
                      bialet_config.full_root_dir, path);
  if(ret < 0 || ret >= (int)sizeof(fullPath)) {
    message(red("Error"), "Path too long in bialetReadFile");
    return NULL;
  }

  char resolved[MAX_URL_LEN];
  if(realpath_n(fullPath, resolved, sizeof(resolved)) == NULL) {
    return NULL;
  }

  size_t root_len = strlen(bialet_config.full_root_dir);
  if(strncmp(resolved, bialet_config.full_root_dir, root_len) != 0 ||
     (resolved[root_len] != '/' && resolved[root_len] != '\\' &&
      resolved[root_len] != '\0')) {
    message(red("Error"), "Path traversal attempt in bialetReadFile");
    return NULL;
  }

  // Reopen the already-contained resolved path without following a symlink, so
  // a link swapped in after the realpath() check cannot feed out-of-root bytes
  // to Markdown.file. On POSIX this mirrors the module loader's no-follow open;
  // on Windows read_file() itself opens without following junctions.
#ifndef _WIN32
  return read_file_fd(open_fd_no_follow(resolved));
#else
  return read_file(resolved);
#endif
}

char* read_file(const char* path) {
  if(path == NULL)
    return NULL;
  char* buffer = 0;
  long  length;
  // read_file is used for framework files and module sources; opening without
  // following symlinks/junctions keeps a planted link from redirecting reads
  // outside the app root after a containment check.
  char  resolved[PATH_MAX];
  FILE* f;
  if(path[0] == '/') {
    f = open_file_no_follow(path);
  } else {
    // Resolve relative paths (e.g. a "./" app root) to absolute before the
    // no-follow open; open_file_no_follow walks absolute paths component by
    // component.
    if(realpath_n(path, resolved, sizeof(resolved)) == NULL)
      return NULL;
    f = open_file_no_follow(resolved);
  }
  if(f) {
    if(fseek(f, 0, SEEK_END) == 0) {
      length = ftell(f);
      if(length >= 0 && fseek(f, 0, SEEK_SET) == 0) {
        buffer = malloc((size_t)length + 1);
        if(buffer) {
          size_t read_bytes = fread(buffer, 1, (size_t)length, f);
          buffer[read_bytes] = '\0';
        }
      }
    }
    fclose(f);
  }
  return buffer;
}

// Wren calls this once it is done compiling a module, so the heap buffer we
// handed over via result.source can be released (the VM does not own it).
static void bialet_wren_free_module_source(WrenVM* vm, const char* name,
                                           WrenLoadModuleResult result) {
  (void)vm;
  (void)name;
  free((char*)result.source);
}

static WrenLoadModuleResult bialet_wren_load_module(WrenVM* vm, const char* name) {

  char                 module[MAX_URL_LEN];
  WrenLoadModuleResult result = {0};

  if(strchr(name, ':') != NULL) {
    char url[MAX_URL_LEN];
    // If name start with https:// or http://
    if(strncmp(name, "http://", 7) == 0 || strncmp(name, "https://", 8) == 0) {
      snprintf(url, MAX_URL_LEN, "%s", name);
    } else if(strncmp(name, BIALET_MODULE_GITHUB_PREFIX,
                      BIALET_EXTERNAL_MODULE_LEN) == 0) {
      name += BIALET_EXTERNAL_MODULE_LEN; // Remove gh:
      char name_copy[MAX_URL_LEN];
      strncpy(name_copy, name, sizeof(name_copy));
      name_copy[sizeof(name_copy) - 1] = '\0';
      char* at = strchr(name_copy, '@');
      char* branch = BIALET_REMOTE_MODULE_DEFAULT_BRANCH;
      if(at != NULL) {
        *at = '\0'; // Remove @
        branch = at + 1;
      }
      char* saveptr = NULL;
      char* user = strtok_r(name_copy, "/", &saveptr);
      char* repo = strtok_r(NULL, "/", &saveptr);
      char* path = strtok_r(NULL, "", &saveptr);
      if(!user || !repo || !path) {
        message(red("Error"), "Invalid GitHub URL.");
        return result;
      }
      int ret = snprintf(url, sizeof(url), BIALET_REMOTE_MODULE_GITHUB_URL, user,
                         repo, branch, path);
      if(ret < 0 || ret >= (int)sizeof(url)) {
        message(red("Error"), "GitHub URL too long.");
        return result;
      }
      name -= 3; // Restore gh:
    } else {
      message(red("Error"), "Import type not supported.");
      return result;
    }
    sqlite3_stmt* stmt = NULL;
    if(sqlite3_prepare_v2(
           db, "SELECT content FROM BIALET_REMOTE_MODULES WHERE module = ? LIMIT 1",
           -1, &stmt, 0) == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
      const char* content = 0;
      if(sqlite3_step(stmt) == SQLITE_ROW) {
        content = (char*)sqlite3_column_text(stmt, 0);
        result.source = string_safe_copy(content);
        result.onComplete = bialet_wren_free_module_source;
      }
      sqlite3_finalize(stmt);
    }
    if(result.source != NULL)
      return result;

    // File not found in cache (or the cache lookup itself failed), try to get
    // it from the URL. Zero the request so timeout/connectTimeout are never
    // read uninitialized; the values below match the http_call_perform defaults.
    struct HttpRequest  req;
    struct HttpResponse resp;
    memset(&req, 0, sizeof(req));
    memset(&resp, 0, sizeof(resp));
    req.method = string_safe_copy("GET");
    req.basicAuth = string_safe_copy("");
    req.raw_headers = string_safe_copy("");
    req.postData = string_safe_copy("");
    req.url = string_safe_copy(url);
    req.timeout = 20000L;
    req.connectTimeout = 2000L;
    http_call_perform(&req, &resp);
    // Check if HTTP request was successful (2xx status codes)
    if(resp.status >= 200 && resp.status < 300 && !resp.error) {
      // File found, save it in cache. resp.body ownership moves into
      // result.source and is released by the onComplete callback.
      result.source = resp.body;
      result.onComplete = bialet_wren_free_module_source;
      sqlite3_stmt* cache_stmt = NULL;
      if(sqlite3_prepare_v2(
             db, "INSERT INTO BIALET_REMOTE_MODULES (module, content) VALUES (?, ?)",
             -1, &cache_stmt, 0) == SQLITE_OK) {
        sqlite3_bind_text(cache_stmt, 1, name, -1, SQLITE_STATIC);
        sqlite3_bind_text(cache_stmt, 2, resp.body, -1, SQLITE_STATIC);
        sqlite3_step(cache_stmt);
        sqlite3_finalize(cache_stmt);
      }
      message(yellow("Remote module saved"), name);
    } else {
      free(resp.body);
      message(red("Error"), "Module not found in GitHub.");
    }
    free(resp.headers);
    free(resp.error_message);
    free(req.method);
    free(req.basicAuth);
    free(req.raw_headers);
    free(req.postData);
    free(req.url);
    return result;
  }

  // Every write to `module` is bounded by sizeof(module). The old code mixed
  // two constants for this one buffer: it was declared MAX_URL_LEN (1024) but
  // appended to with `MAX_MODULE_LEN - strlen(module) - 1` (256-based), so once
  // the prefix passed 255 characters that bound wrapped around to a value near
  // SIZE_MAX and strncat's limit stopped limiting anything. Only an unrelated
  // length guard kept it from overflowing.
  if(name[0] == '/') {
    if(strlen(name) + strlen(bialet_config.full_root_dir) + BIALET_EXTENSION_LEN +
           1 >
       sizeof(module)) {
      message(red("Error"), "Module name too long.");
      return result;
    }
    snprintf(module, sizeof(module), "%s", bialet_config.full_root_dir);
  } else {
    char* calledFrom = string_safe_copy(wrenGetUserData(vm));
    if(calledFrom == NULL) {
      fprintf(stderr,
              "Error: Cannot resolve relative import '%s' without application "
              "context.\n       Use: bialet -t <file> <app_root>\n",
              name);
      return result;
    }
    // Strip the filename, keeping the calling module's directory. Windows
    // paths use backslashes, so find the last separator of either kind.
    char* last_sep = NULL;
    for(char* p = calledFrom; *p != '\0'; p++) {
      if(*p == '/' || *p == '\\')
        last_sep = p;
    }
    if(strlen(name) + strlen(calledFrom) + BIALET_EXTENSION_LEN + 2 >
       sizeof(module)) {
      message(red("Error"), "Module name too long.");
      free(calledFrom);
      return result;
    }
    if(last_sep)
      *last_sep = '\0';
    snprintf(module, sizeof(module), "%s/", calledFrom);
    free(calledFrom);
  }

  size_t used = strlen(module);
  if(used + 1 >= sizeof(module)) {
    message(red("Error"), "Module name too long.");
    return result;
  }
  strncat(module, name, sizeof(module) - used - 1);

  size_t name_len = strlen(module);
  if(name_len < BIALET_EXTENSION_LEN ||
     strcmp(module + name_len - BIALET_EXTENSION_LEN, BIALET_EXTENSION) != 0) {
    if(name_len + BIALET_EXTENSION_LEN >= sizeof(module)) {
      message(red("Error"), "Module name too long.");
      return result;
    }
    strncat(module, BIALET_EXTENSION, sizeof(module) - name_len - 1);
  }

  char resolved[MAX_URL_LEN];
  if(realpath_n(module, resolved, sizeof(resolved)) == NULL) {
    return result;
  }

  size_t root_len = strlen(bialet_config.full_root_dir);
  if(strncmp(resolved, bialet_config.full_root_dir, root_len) != 0 ||
     (resolved[root_len] != '/' && resolved[root_len] != '\\' &&
      resolved[root_len] != '\0')) {
    return result;
  }

  // Reopen the already-contained resolved path with O_NOFOLLOW on every
  // component, so a symlink swap in the check-to-open window cannot feed
  // out-of-root bytes into the interpreter.
#ifndef _WIN32
  char* buffer = read_file_fd(open_fd_no_follow(resolved));
#else
  char* buffer = read_file(resolved);
#endif
  result.source = NULL;

  if(buffer) {
    result.source = buffer;
    result.onComplete = bialet_wren_free_module_source;
  }
  return result;
}

void bialet_wren_error(WrenVM* vm, WrenErrorType errorType, const char* module,
                       const int line, const char* msg) {
  (void)vm;
  if(test_capturing) {
    switch(errorType) {
      case WREN_ERROR_COMPILE:
      case WREN_ERROR_RUNTIME:
        if(msg != NULL)
          snprintf(test_fail_msg, sizeof(test_fail_msg), "%s", msg);
        break;
      case WREN_ERROR_STACK_TRACE:
        // wrenDebugPrintStackTrace() never reports frames from the anonymous
        // core module (where the Test DSL itself lives), only named modules.
        // The test file runs as module "main", so the last (outermost)
        // frame reported here is always its own top-level line -- the exact
        // call site of the failing assertion, not internals of Test.status()
        // and friends.
        test_fail_line = line;
        break;
    }
    return;
  }
  char lineMessage[MAX_LINE_ERROR_LEN];
  snprintf(lineMessage, sizeof(lineMessage), "%s line %d", module, line);
  switch(errorType) {
    case WREN_ERROR_COMPILE: {
      message(red("Compilation Error"), lineMessage, (char*)msg);
      show_errors_capture("Compilation Error", module, line, msg);
    } break;
    case WREN_ERROR_STACK_TRACE: {
      message(red("Stack Error"), lineMessage, (char*)msg);
      show_errors_capture("Stack Error", module, line, msg);
    } break;
    case WREN_ERROR_RUNTIME: {
      message(red("Runtime Error"), (char*)msg);
      show_errors_capture("Runtime Error", module, line, msg);
    } break;
  }
}

static char* sqlite_int_to_string(sqlite3_int64 value) {
  char* str = (char*)malloc(21 * sizeof(char));
  if(str == NULL)
    return NULL;
  snprintf(str, 21, "%lld", value);
  return str;
}

static void query_execute(WrenVM* vm, BialetQuery* query) {
  (void)vm;
  sqlite3_stmt* stmt;
  const char*   columns[MAX_COLUMNS];
  int           colType, colCount = 0, rowCount = 0, bindCounter = 0;

  // Check if the query string contains only whitespace
  const char* str = query->queryString;
  int         isEmpty = 1; // Assume the string is empty or contains only whitespaces
  while(*str) {
    if(!isspace((unsigned char)*str)) {
      isEmpty = 0; // Found a non-whitespace character
      break;
    }
    str++;
  }
  // Ignore empty queries
  if(isEmpty)
    return;

  /* Prepare the query */
  int result = sqlite3_prepare_v2(db, query->queryString, -1, &stmt, 0);
  if(result != SQLITE_OK) {
    message(red("Query Error"), sqlite3_errmsg(db));
    return;
  }

  /* Bind parameters */
  for(int i = 0; i < query->parametersCount; i++) {
    bindCounter = i + 1;
    switch(query->parameters[i].type) {
      case BIALETQUERYTYPE_STRING:
        sqlite3_bind_text(stmt, bindCounter, query->parameters[i].value, -1,
                          SQLITE_STATIC);
        break;
      case BIALETQUERYTYPE_NUMBER:
        sqlite3_bind_double(stmt, bindCounter, atof(query->parameters[i].value));
        break;
      case BIALETQUERYTYPE_BOOLEAN:
        sqlite3_bind_int(stmt, bindCounter, atoi(query->parameters[i].value));
        break;
      case BIALETQUERYTYPE_NULL:
        sqlite3_bind_null(stmt, bindCounter);
        break;
      default:
        message(red("Query Error"), "Uknown type on binding parameters");
        sqlite3_finalize(stmt);
        return;
    }
  }

  /* Execute statement and fetch results */
  int alloc_error = 0;
  while((result = sqlite3_step(stmt)) == SQLITE_ROW) {
    if(!colCount) {
      /* Get column names */
      colCount = sqlite3_column_count(stmt);
      if(colCount > MAX_COLUMNS)
        colCount = MAX_COLUMNS;
      for(int i = 0; i < colCount; i++) {
        columns[i] = sqlite3_column_name(stmt, i);
      }
    }

    if(add_result(query) != 0) {
      alloc_error = 1;
      break;
    }
    for(int i = 0; i < colCount; i++) {
      /* Initialized per column. These were declared once outside the loop and
       * the `default:` branch below only logged before falling through to
       * add_result_row(), which then used whatever the *previous* column left
       * behind -- or, on the first column, indeterminate values. */
      const char* value = NULL;
      int         size = 0;
      int         type = BIALETQUERYTYPE_NULL;

      colType = sqlite3_column_type(stmt, i);
      switch(colType) {
        case SQLITE_INTEGER:
        case SQLITE_FLOAT:
        case SQLITE_TEXT:
          type = colType == SQLITE_TEXT ? BIALETQUERYTYPE_STRING
                                        : BIALETQUERYTYPE_NUMBER;
          value = (const char*)sqlite3_column_text(stmt, i);
          /* sqlite3_column_text returns NULL if the conversion cannot be
           * allocated; strlen() on it was an unconditional NULL dereference. */
          if(value == NULL) {
            alloc_error = 1;
          } else {
            size = (int)strlen(value);
          }
          break;
        case SQLITE_BLOB:
          type = BIALETQUERYTYPE_BLOB;
          /* Note: BLOB data is retrieved correctly from SQLite but may not be
           * properly passed to Wren as binary data. This is due to Wren's string
           * representation. Consider converting BLOBs to base64 strings or handling
           * them as byte arrays if Wren adds support for binary data types. */
          value = sqlite3_column_blob(stmt, i);
          size = sqlite3_column_bytes(stmt, i);
          break;
        case SQLITE_NULL:
          type = BIALETQUERYTYPE_NULL;
          value = NULL;
          size = 1;
          break;
        default:
          message(red("Query Error"), "Unknown type on binding result");
          continue; /* skip this column instead of falling through */
      }
      if(alloc_error)
        break;
      if(add_result_row(query, rowCount, columns[i], value, size, type) != 0) {
        alloc_error = 1;
        break;
      }
    }
    rowCount++;
  }

  if(alloc_error) {
    message(red("Query Error"), "Out of memory while collecting query results");
  }

  /* Check for errors during query execution.
   * SQLITE_DONE means all rows have been fetched successfully.
   * SQLITE_OK is also acceptable (though less common after stepping).
   * Any other result code indicates an error that should be reported.
   * Note: SQLITE_MISUSE can occur from incorrect API usage such as:
   * - Using a finalized statement
   * - Using a statement from a different thread
   * - Calling sqlite3_step after it has returned SQLITE_DONE
   * This error handling ensures proper cleanup even on errors. */
  if(result != SQLITE_DONE && result != SQLITE_OK) {
    message(red("SQL Error"), sqlite3_errmsg(db));
  }
  query->lastInsertId = sqlite_int_to_string(sqlite3_last_insert_rowid(db));
  sqlite3_finalize(stmt);
}
char* escape_special_chars(const char* input) {
  size_t i, j = 0, len = strlen(input);
  char*  output = malloc(len * 2 + 1);
  if(output == NULL)
    return NULL;

  for(i = 0; i < len; i++) {
    if(input[i] == '"' || input[i] == '\\' || input[i] == '%') {
      output[j++] = '\\';
    }
    output[j++] = input[i];
  }
  output[j] = '\0';
  return output;
}

// Bounded byte search: the request body is arbitrary binary data, so nothing
// here may use str*() functions. The old parser ran strstr/strchr/strcasestr
// over hm->message.str, all of which stop at the first NUL -- so uploading any
// file containing a zero byte (every PNG, PDF, zip or executable) silently
// truncated or dropped the part.
static const char* mem_find(const char* hay, size_t hay_len, const char* needle,
                            size_t needle_len) {
  if(needle_len == 0 || hay_len < needle_len)
    return NULL;
  for(size_t i = 0; i + needle_len <= hay_len; i++) {
    if(memcmp(hay + i, needle, needle_len) == 0)
      return hay + i;
  }
  return NULL;
}

// Case-insensitive variant, for header names.
static const char* mem_find_ci(const char* hay, size_t hay_len, const char* needle,
                               size_t needle_len) {
  if(needle_len == 0 || hay_len < needle_len)
    return NULL;
  for(size_t i = 0; i + needle_len <= hay_len; i++) {
    size_t k = 0;
    while(k < needle_len &&
          tolower((unsigned char)hay[i + k]) == tolower((unsigned char)needle[k]))
      k++;
    if(k == needle_len)
      return hay + i;
  }
  return NULL;
}

// Copies the value that runs from [src] up to the first delimiter into [out],
// bounded by both [avail] and [out_size]. Returns the length written.
static size_t copy_until(const char* src, size_t avail, const char* delims,
                         char* out, size_t out_size) {
  size_t n = 0;
  while(n < avail && n + 1 < out_size && strchr(delims, src[n]) == NULL &&
        src[n] != '\0')
    n++;
  memcpy(out, src, n);
  out[n] = '\0';
  return n;
}

int save_uploaded_files(struct HttpMessage* hm, char* filesIds) {
  filesIds[0] = '\0'; // Initialize empty string

  const char* msg = hm->message.str;
  size_t      msg_len = hm->message.len;
  if(msg == NULL || msg_len == 0)
    return 0;

  // End of the request header block.
  const char* headerEnd = mem_find(msg, msg_len, "\r\n\r\n", 4);
  if(!headerEnd)
    return 0;
  size_t head_len = (size_t)(headerEnd - msg);

  static const char kContentType[] = "Content-Type:";
  const char*       ctStart =
      mem_find_ci(msg, head_len, kContentType, sizeof(kContentType) - 1);
  if(!ctStart)
    return 0;
  ctStart += sizeof(kContentType) - 1;
  while(ctStart < headerEnd && (*ctStart == ' ' || *ctStart == '\t'))
    ctStart++;

  static const char kMultipart[] = "multipart/form-data";
  size_t            ct_avail = (size_t)(headerEnd - ctStart);
  if(ct_avail < sizeof(kMultipart) - 1 ||
     mem_find_ci(ctStart, sizeof(kMultipart) - 1, kMultipart,
                 sizeof(kMultipart) - 1) != ctStart)
    return 0;

  static const char kBoundary[] = "boundary=";
  const char*       boundaryStart =
      mem_find_ci(ctStart, ct_avail, kBoundary, sizeof(kBoundary) - 1);
  if(!boundaryStart)
    return 0;
  boundaryStart += sizeof(kBoundary) - 1;

  char   boundary[256];
  size_t boundary_len =
      copy_until(boundaryStart, (size_t)(headerEnd - boundaryStart), "\r\n;",
                 boundary, sizeof(boundary));
  if(boundary_len == 0)
    return 0;

  const char* body = headerEnd + 4;
  const char* end = msg + msg_len;
  if(body >= end)
    return 0;

  // The opening delimiter is "--boundary"; every later one is preceded by CRLF.
  // Matching the CRLF as part of the delimiter is what makes this safe for
  // binary content that happens to contain the boundary string, and it also
  // means the trailing CRLF is not counted as file data.
  char startBoundary[260];
  char delimiter[264];
  int  sb = snprintf(startBoundary, sizeof(startBoundary), "--%.*s",
                     (int)boundary_len, boundary);
  int  dl = snprintf(delimiter, sizeof(delimiter), "\r\n--%.*s", (int)boundary_len,
                     boundary);
  if(sb < 0 || sb >= (int)sizeof(startBoundary) || dl < 0 ||
     dl >= (int)sizeof(delimiter))
    return 0;
  size_t sb_len = (size_t)sb;
  size_t dl_len = (size_t)dl;

  const char* cursor = mem_find(body, (size_t)(end - body), startBoundary, sb_len);
  int         firstFile = 1;
  int         uploadedFiles = 0;

  while(cursor != NULL) {
    const char* part = cursor + sb_len;
    if(part + 2 <= end && part[0] == '-' && part[1] == '-')
      break; // closing delimiter "--boundary--"
    if(part < end && *part == '\r')
      part++;
    if(part < end && *part == '\n')
      part++;
    if(part >= end)
      break;

    const char* partHeaderEnd = mem_find(part, (size_t)(end - part), "\r\n\r\n", 4);
    if(!partHeaderEnd)
      break;
    size_t      part_head_len = (size_t)(partHeaderEnd - part);
    const char* fileData = partHeaderEnd + 4;

    // Where this part's data ends, and where to resume scanning.
    const char* nextDelim =
        mem_find(fileData, (size_t)(end - fileData), delimiter, dl_len);
    const char* fileEnd = nextDelim ? nextDelim : end;
    const char* resume = nextDelim ? nextDelim + 2 : NULL; // skip the CRLF

    static const char kDisposition[] = "Content-Disposition:";
    const char*       cdStart =
        mem_find_ci(part, part_head_len, kDisposition, sizeof(kDisposition) - 1);
    if(!cdStart) {
      cursor = resume
                   ? mem_find(resume, (size_t)(end - resume), startBoundary, sb_len)
                   : NULL;
      continue;
    }
    size_t cd_avail = part_head_len - (size_t)(cdStart - part);

    static const char kFilename[] = "filename=\"";
    const char*       filenameStart =
        mem_find_ci(cdStart, cd_avail, kFilename, sizeof(kFilename) - 1);
    char filename[256];
    filename[0] = '\0';
    if(filenameStart != NULL) {
      filenameStart += sizeof(kFilename) - 1;
      copy_until(filenameStart, (size_t)(partHeaderEnd - filenameStart), "\"",
                 filename, sizeof(filename));
    }
    // A part with no filename is a plain form field, not an upload.
    if(filename[0] == '\0') {
      cursor = resume
                   ? mem_find(resume, (size_t)(end - resume), startBoundary, sb_len)
                   : NULL;
      continue;
    }

    static const char kName[] = "name=\"";
    const char* nameStart = mem_find_ci(cdStart, cd_avail, kName, sizeof(kName) - 1);
    char        fieldName[256];
    fieldName[0] = '\0';
    if(nameStart != NULL) {
      nameStart += sizeof(kName) - 1;
      copy_until(nameStart, (size_t)(partHeaderEnd - nameStart), "\"", fieldName,
                 sizeof(fieldName));
    }

    char        contentTypeStr[256] = "application/octet-stream";
    const char* partCtStart =
        mem_find_ci(part, part_head_len, kContentType, sizeof(kContentType) - 1);
    if(partCtStart != NULL) {
      partCtStart += sizeof(kContentType) - 1;
      while(partCtStart < partHeaderEnd &&
            (*partCtStart == ' ' || *partCtStart == '\t'))
        partCtStart++;
      copy_until(partCtStart, (size_t)(partHeaderEnd - partCtStart), "\r\n;",
                 contentTypeStr, sizeof(contentTypeStr));
    }

    size_t fileSize = (size_t)(fileEnd - fileData);

    // Validate file size to prevent disk abuse
    if(fileSize > bialet_config.max_upload_size) {
      char sizeMsg[512];
      snprintf(
          sizeMsg, sizeof(sizeMsg),
          "File '%s' exceeds maximum upload size (%zu bytes > %zu bytes allowed)",
          filename, fileSize, bialet_config.max_upload_size);
      message(red("Upload Error"), sizeMsg);
      cursor = resume
                   ? mem_find(resume, (size_t)(end - resume), startBoundary, sb_len)
                   : NULL;
      continue;
    }

    // Skip further parts once the per-request file count cap is reached so a
    // single request cannot insert an unbounded number of rows into
    // BIALET_FILES (CPU/disk amplification).
    if(uploadedFiles >= MAX_UPLOAD_FILES) {
      message(red("Upload Error"), "Too many files in request, skipping rest");
      break;
    }

    // Save file to database
    sqlite3_stmt* stmt = NULL;
    int           result = sqlite3_prepare_v2(db,
                                              "INSERT INTO BIALET_FILES (name, "
                                                        "originalFileName, type, file, size, isTemp) "
                                                        "VALUES (?, ?, ?, ?, ?, 1)",
                                              -1, &stmt, 0);

    if(result == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, fieldName, -1, SQLITE_STATIC);
      sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
      sqlite3_bind_text(stmt, 3, contentTypeStr, -1, SQLITE_STATIC);
      sqlite3_bind_blob64(stmt, 4, fileData, (sqlite3_uint64)fileSize,
                          SQLITE_STATIC);
      sqlite3_bind_int64(stmt, 5, (sqlite3_int64)fileSize);

      if(sqlite3_step(stmt) == SQLITE_DONE) {
        sqlite3_int64 fileId = sqlite3_last_insert_rowid(db);

        // Append file ID to filesIds string safely
        char idStr[32];
        snprintf(idStr, sizeof(idStr), "%lld", fileId);

        size_t currentLen = strlen(filesIds);
        size_t neededLen = currentLen + (firstFile ? 0 : 1) + strlen(idStr);

        if(neededLen < MAX_URL_LEN) {
          if(!firstFile) {
            strncat(filesIds, ",", MAX_URL_LEN - currentLen - 1);
          }
          strncat(filesIds, idStr, MAX_URL_LEN - strlen(filesIds) - 1);
          firstFile = 0;
        } else {
          message(red("Upload Error"), "Too many files uploaded");
        }
      } else {
        message(red("Upload Error"), sqlite3_errmsg(db));
      }
      sqlite3_finalize(stmt);
      uploadedFiles++;
    } else {
      message(red("Upload Error"), sqlite3_errmsg(db));
    }

    cursor = resume ? mem_find(resume, (size_t)(end - resume), startBoundary, sb_len)
                    : NULL;
  }

  // Steady-state recovery for BIALET_FILES: without a purge the temp blobs
  // accumulate ~10 MB per upload request indefinitely. The Wren-side Db.clean
  // only runs during migrations, so enforce the same purge here, throttled to
  // at most once a minute to keep the per-request cost near zero.
  {
    static time_t last_purge = 0;
    time_t        now = time(NULL);
    if(now - last_purge >= 60) {
      last_purge = now;
      sqlite3_stmt* purge_stmt;
      if(sqlite3_prepare_v2(db,
                            "DELETE FROM BIALET_FILES WHERE isTemp = 1 AND "
                            "createdAt < datetime('now', '-1 day')",
                            -1, &purge_stmt, 0) == SQLITE_OK) {
        sqlite3_step(purge_stmt);
        sqlite3_finalize(purge_stmt);
      }
    }
  }

  return 1;
}

struct BialetResponse bialet_run(char* module, char* code, struct HttpMessage* hm) {
  struct BialetResponse r;
  r.status = HTTP_OK;
  r.header = NULL;
  r.body = NULL;
  r.length = 0;
  r.body_owned = 0;
  r.header_owned = 0;
  int     error = 0;
  WrenVM* vm = 0;

  show_errors_clear();

  vm = wrenNewVM(&wren_config);
  wrenSetUserData(vm, module);
  wrenInterpret(vm, MAIN_MODULE_NAME, MAIN_MODULE_SOURCE);
  if(hm) {
    /* Initialize request */
    wrenEnsureSlots(vm, 4);
    wrenGetVariable(vm, MAIN_MODULE_NAME, "Request", 0);
    WrenHandle* requestClass = wrenGetSlotHandle(vm, 0);
    WrenHandle* initMethod = wrenMakeCallHandle(vm, "init(_,_,_)");
    wrenSetSlotHandle(vm, 0, requestClass);
    wrenSetSlotString(vm, 1, hm->message.str);
    wrenSetSlotString(vm, 2, hm->routes.str);

    char filesIds[MAX_URL_LEN] = "";
    // Save uploaded files with size validation (max_upload_size config)
    save_uploaded_files(hm, filesIds);
    wrenSetSlotString(vm, 3, filesIds);

    if((error = wrenCall(vm, initMethod) != WREN_RESULT_SUCCESS))
      message(red("Runtime Error"), "Failed to initialize request");
    wrenReleaseHandle(vm, requestClass);
    wrenReleaseHandle(vm, initMethod);
  }
  /* Run user code */
  if(!error) {
    WrenInterpretResult result = wrenInterpret(vm, module, code);
    error = result != WREN_RESULT_SUCCESS;
  }
  if(!error) {
    wrenEnsureSlots(vm, 2);
    int type = wrenGetSlotType(vm, 0);
    if(type == WREN_TYPE_STRING) {
      const char* returnBody = wrenGetSlotString(vm, 0);
      r.body = string_safe_copy(returnBody);
      r.body_owned = 1;
    } else if(IS_INSTANCE(vm->apiStack[0])) {
      /* A handler may return an HtmlNode: an HTML literal already rendered by
       * the template escape machinery. Stringify it so the page is served. */
      wrenGetVariable(vm, module, "HtmlNode", 1);
      if(AS_INSTANCE(vm->apiStack[0])->obj.classObj == AS_CLASS(vm->apiStack[1])) {
        WrenHandle* toString = wrenMakeCallHandle(vm, "toString");
        wrenSetSlotHandle(vm, 0, wrenGetSlotHandle(vm, 0));
        if(wrenCall(vm, toString) == WREN_RESULT_SUCCESS) {
          const char* body = wrenGetSlotString(vm, 0);
          r.body = string_safe_copy(body);
          r.body_owned = 1;
        }
      }
    }

    wrenGetVariable(vm, module, "Response", 0);
    WrenHandle* responseClass = wrenGetSlotHandle(vm, 0);
    if(r.body == NULL || strlen(r.body) == 0) {
      /* Get body from response */
      WrenHandle* outMethod = wrenMakeCallHandle(vm, "out");
      wrenSetSlotHandle(vm, 0, responseClass);
      if((error = wrenCall(vm, outMethod) != WREN_RESULT_SUCCESS)) {
        message(red("Runtime Error"), "Failed to get body");
      } else if(wrenGetSlotType(vm, 0) != WREN_TYPE_STRING) {
        /* wrenGetSlotString only asserts the slot type, and asserts compile out
         * with NDEBUG -- so a handler returning a non-string from Response.out
         * reinterpreted whatever was in the slot as a char*. */
        message(red("Runtime Error"), "Response body is not a string");
        error = 1;
      } else {
        const char* body = wrenGetSlotString(vm, 0);
        if(body[0] != BIALET_FILE_CHAR) {
          r.body = string_safe_copy(body);
          r.body_owned = 1;
        } else {
          /* Handle BIALET_FILE_CHAR response for file serving.
           * This retrieves file content from the database when the response body
           * starts with BIALET_FILE_CHAR. Consider moving this logic to the server
           * layer in the future to better separate concerns between the Wren runtime
           * and response preparation. */
          sqlite3_stmt* stmt;
          int           result = sqlite3_prepare_v2(
              db, "SELECT file FROM BIALET_FILES WHERE id = ?", -1, &stmt, 0);
          if(!(error = result != SQLITE_OK)) {
            sqlite3_bind_text(stmt, 1, body + 1, -1, SQLITE_STATIC);
            if(sqlite3_step(stmt) == SQLITE_ROW) {
              int len = 0;
              len = sqlite3_column_bytes(stmt, 0);
              r.body = safe_malloc(len + 1);
              memcpy(r.body, sqlite3_column_blob(stmt, 0), len);
              r.length = len;
              r.body_owned = 1;
            } else {
              // If the id is not found, we will send an internal server error.
              message(red("Error file not found"), sqlite3_errmsg(db));
              error = 1;
            }
            sqlite3_finalize(stmt);
          }
        }
      }
      wrenReleaseHandle(vm, outMethod);
    }
    /* Get status from response */
    wrenEnsureSlots(vm, 1);
    WrenHandle* statusMethod = wrenMakeCallHandle(vm, "status");
    wrenSetSlotHandle(vm, 0, responseClass);
    if((error = wrenCall(vm, statusMethod) != WREN_RESULT_SUCCESS)) {
      message(red("Runtime Error"), "Failed to get status");
    } else if(wrenGetSlotType(vm, 0) != WREN_TYPE_NUM) {
      message(red("Runtime Error"), "Response status is not a number");
      error = 1;
    } else {
      const double status = wrenGetSlotDouble(vm, 0);
      r.status = (int)status;
    }
    wrenReleaseHandle(vm, statusMethod);
    /* Get headers from response */
    if(hm) {
      wrenEnsureSlots(vm, 1);
      WrenHandle* headersMethod = wrenMakeCallHandle(vm, "headers");
      wrenSetSlotHandle(vm, 0, responseClass);
      if((error = wrenCall(vm, headersMethod) != WREN_RESULT_SUCCESS)) {
        message(red("Runtime Error"), "Failed to get headers");
      } else if(wrenGetSlotType(vm, 0) != WREN_TYPE_STRING) {
        message(red("Runtime Error"), "Response headers are not a string");
        error = 1;
      } else {
        const char* headersString = wrenGetSlotString(vm, 0);
        r.header = string_safe_copy(headersString);
        r.header_owned = 1;
      }
      wrenReleaseHandle(vm, headersMethod);
    }
    /* Check if an error method was called for custom error pages */
    wrenEnsureSlots(vm, 1);
    wrenGetVariable(vm, module, "Response", 0);
    WrenHandle* useErrorHandle = wrenGetSlotHandle(vm, 0);
    WrenHandle* useErrorFallback = wrenMakeCallHandle(vm, "useErrorFallback()");
    wrenSetSlotHandle(vm, 0, useErrorHandle);
    if(wrenCall(vm, useErrorFallback) == WREN_RESULT_SUCCESS &&
       wrenGetSlotType(vm, 0) == WREN_TYPE_BOOL) {
      if(wrenGetSlotBool(vm, 0) &&
         (r.status == 403 || r.status == 404 || r.status == 413 || r.status == 429 ||
          r.status == 500)) {
        custom_error(r.status, &r);
      }
    }
    wrenReleaseHandle(vm, useErrorFallback);
    wrenReleaseHandle(vm, useErrorHandle);
    /* Clean Wren vm */
    wrenReleaseHandle(vm, responseClass);
  }
  wrenFreeVM(vm);

  if(error) {
    if(hm != NULL && show_errors_enabled()) {
      char* page = show_errors_page();
      if(page != NULL) {
        r.status = HTTP_ERROR;
        r.body = page;
        r.body_owned = 1;
        r.length = strlen(page);
        r.header = BIALET_HEADERS;
        r.header_owned = 0;
      } else {
        custom_error(HTTP_ERROR, &r);
      }
    } else {
      custom_error(HTTP_ERROR, &r);
    }
  }

  if(!hm) {
    r.header = NULL;
    r.header_owned = 0;
  }

  return r;
}

int bialet_run_cli(char* code) {
  struct BialetResponse response = bialet_run(CLI_MODULE_NAME, code, NULL);
  int                   status = response.status == HTTP_ERROR ? 1 : 0;
  if(status == 0 && response.body != NULL) {
    // fwrite with the known length rather than printf("%s"): the body may be
    // binary (a file served through Response.file) and would be cut at the
    // first NUL, and printf("%s", NULL) on an empty response was undefined
    // behavior.
    size_t len = response.length;
    if(len == 0)
      len = strlen(response.body);
    fwrite(response.body, 1, len, stdout);
  }
  // The response owned its body/header and was discarded here.
  if(response.body_owned)
    free(response.body);
  if(response.header_owned)
    free(response.header);
  return status;
}

int bialet_validate_syntax(const char* filePath) {
  char abs_path[MAX_URL_LEN];

  if(realpath_n(filePath, abs_path, sizeof(abs_path)) == NULL) {
    fprintf(stderr, "Error: Cannot resolve file '%s'\n", filePath);
    return 1;
  }

  size_t root_len = strlen(bialet_config.full_root_dir);
  if(strncmp(abs_path, bialet_config.full_root_dir, root_len) != 0 ||
     (abs_path[root_len] != '/' && abs_path[root_len] != '\\' &&
      abs_path[root_len] != '\0')) {
    fprintf(stderr, "Error: File '%s' is outside the application root directory\n",
            filePath);
    return 1;
  }

  char* code = read_file(abs_path);
  if(code == NULL) {
    fprintf(stderr, "Error: Cannot read file '%s'\n", filePath);
    return 1;
  }

  WrenVM* vm = wrenNewVM(&wren_config);
  wrenSetUserData(vm, abs_path);
  /* Initialize core classes (Date, Response) before validating, so scripts
   * that touch Date/Response at the top level don't read uninitialized
   * state and crash. Mirrors bialet_run(). */
  wrenInterpret(vm, MAIN_MODULE_NAME, MAIN_MODULE_SOURCE);
  WrenInterpretResult result = wrenInterpret(vm, abs_path, code);
  wrenFreeVM(vm);
  free(code);

  if(result == WREN_RESULT_COMPILE_ERROR) {
    return 1;
  }
  return 0;
}

#include <dirent.h>
#include <sys/stat.h>

#define TESTS_DIR "_tests"
#define TEST_INIT_FILE "_init.wren"
#define MAX_TEST_FILES 100

// ANSI SGR codes for the -T runner's own printf-based output. Kept local
// (rather than reusing messages.h's green()/red()/yellow()) because those
// route their allocation through a pending-free list that only message()
// drains; calling them here without message() would leak, and after
// MSG_MAX_PENDING calls would silently stop coloring altogether.
#define TEST_COLOR_GREEN 32
#define TEST_COLOR_RED 31
#define TEST_COLOR_YELLOW 33

const char* bialet_get_full_root_dir() {
  return bialet_config.full_root_dir;
}

static int is_test_file(const char* name) {
  size_t len = strlen(name);
  if(len < 6)
    return 0; // min: x.wren
  if(strcmp(name + len - 5, ".wren") != 0)
    return 0;
  if(strcmp(name, TEST_INIT_FILE) == 0)
    return 0;
  return 1;
}

typedef enum { TEST_RESULT_PASS, TEST_RESULT_FAIL, TEST_RESULT_SKIP } TestResult;

typedef struct {
  char name[MAX_MODULE_LEN];
  int  line;
  char msg[TEST_FAIL_MSG_LEN];
} TestFailure;

static long long test_monotonic_ms(void) {
#ifdef _WIN32
  return (long long)GetTickCount64();
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

// Prints [text] wrapped in the given SGR color when color output is enabled
// (isatty + -q + output_color, the same determination the rest of the app
// uses), otherwise prints it plain.
static void print_colored(const char* text, int color) {
  if(message_color_enabled())
    printf("\033[%dm%s\033[0m", color, text);
  else
    fputs(text, stdout);
}

static TestResult run_test_file(const char* testPath, const char* initPath,
                                int* outLine, char* outMsg, size_t outMsgSize) {
  char* code = read_file(testPath);
  if(code == NULL) {
    *outLine = 0;
    // The test path can exceed the fixed message buffer, and an unbounded %s
    // trips -Wformat-truncation under -Werror. Bound the copy: 256-byte
    // buffer - "Cannot read test file: " (23) - NUL = 232.
    snprintf(outMsg, outMsgSize, "Cannot read test file: %.232s", testPath);
    return TEST_RESULT_FAIL;
  }

  WrenVM* vm = wrenNewVM(&wren_config);

  // Initialize Response and Date in main module
  wrenInterpret(vm, MAIN_MODULE_NAME, MAIN_MODULE_SOURCE);

  // Run init file if provided
  if(initPath != NULL) {
    char* initCode = read_file(initPath);
    if(initCode != NULL) {
      wrenInterpret(vm, MAIN_MODULE_NAME, initCode);
      free(initCode);
    }
  }

  // Run test code in main module
  test_capture_begin();
  WrenInterpretResult result = wrenInterpret(vm, MAIN_MODULE_NAME, code);
  int                 skipped = test_skip_requested;
  int                 failLine = test_fail_line;
  char                failMsg[TEST_FAIL_MSG_LEN];
  snprintf(failMsg, sizeof(failMsg), "%s", test_fail_msg);
  test_capture_end();

  wrenFreeVM(vm);
  free(code);

  if(skipped)
    return TEST_RESULT_SKIP;
  if(result != WREN_RESULT_SUCCESS) {
    *outLine = failLine;
    snprintf(outMsg, outMsgSize, "%s", failMsg[0] ? failMsg : "Test failed");
    return TEST_RESULT_FAIL;
  }
  return TEST_RESULT_PASS;
}

int bialet_run_tests(const char* testDir, const char* rootDir) {
  (void)rootDir;
  long long startMs = test_monotonic_ms();
  int       quiet = bialet_config.quiet;

  char testsPath[MAX_MODULE_LEN];
  snprintf(testsPath, sizeof(testsPath), "%s/%s", testDir, TESTS_DIR);

  struct stat st;
  if(stat(testsPath, &st) != 0 || !S_ISDIR(st.st_mode)) {
    fprintf(stderr, "Error: Test directory not found: %s\n", testsPath);
    return 1;
  }

  if(!quiet)
    printf("Running tests in %s...\n\n", testsPath);

  DIR* dir = opendir(testsPath);
  if(dir == NULL) {
    fprintf(stderr, "Error: Cannot open test directory: %s\n", testsPath);
    return 1;
  }

  // Collect test files
  char* testFiles[MAX_TEST_FILES];
  int   testCount = 0;

  struct dirent* entry;
  int            capped = 0;
  while((entry = readdir(dir)) != NULL) {
    if(!is_test_file(entry->d_name))
      continue;
      // A directory named "foo.wren" is not a test file.
#ifdef DT_DIR
    if(entry->d_type == DT_DIR)
      continue;
#endif
    if(testCount >= MAX_TEST_FILES) {
      // Reaching the cap used to end the loop silently, so tests beyond the
      // hundredth simply never ran and the summary still said "0 failed".
      capped++;
      continue;
    }
    char* name = strdup(entry->d_name);
    if(name == NULL) {
      // Was unchecked: a NULL entry later reached snprintf("%s", NULL).
      fprintf(stderr, "Error: out of memory collecting test files\n");
      capped++;
      continue;
    }
    testFiles[testCount] = name;
    testCount++;
  }
  closedir(dir);

  if(capped > 0) {
    fprintf(stderr, "Warning: %d test file(s) not run (limit %d)\n", capped,
            MAX_TEST_FILES);
  }

  if(testCount == 0) {
    if(!quiet)
      printf("No tests found.\n");
    return 0;
  }

  // Check for init file
  char initPath[MAX_MODULE_LEN + 16];
  snprintf(initPath, sizeof(initPath), "%s/%s", testsPath, TEST_INIT_FILE);
  char* initPathPtr = (stat(initPath, &st) == 0) ? initPath : NULL;

  int         passed = 0;
  int         failed = 0;
  int         skipped = 0;
  TestFailure failures[MAX_TEST_FILES];

  // In -q, test-triggered System.log/migration output (via message()) would
  // otherwise interleave with the crux-style summary below. Redirect it to
  // the null device for the duration of the run and restore it afterward.
  FILE* quietSink = NULL;
  FILE* prevLogFile = NULL;
  if(quiet) {
    quietSink = fopen(
#ifdef _WIN32
        "NUL",
#else
        "/dev/null",
#endif
        "w");
    if(quietSink != NULL)
      prevLogFile = message_set_log_file(quietSink);
  }

  // Run each test file
  for(int i = 0; i < testCount; i++) {
    char testPath[MAX_MODULE_LEN * 2];
    snprintf(testPath, sizeof(testPath), "%s/%s", testsPath, testFiles[i]);

    int        line = 0;
    char       msg[TEST_FAIL_MSG_LEN];
    long long  testStartMs = test_monotonic_ms();
    TestResult result =
        run_test_file(testPath, initPathPtr, &line, msg, sizeof(msg));
    long long testElapsedMs = test_monotonic_ms() - testStartMs;

    switch(result) {
      case TEST_RESULT_PASS:
        passed++;
        if(!quiet) {
          printf("  ");
          print_colored("✓", TEST_COLOR_GREEN);
          printf(" %s (%lldms)\n", testFiles[i], testElapsedMs);
        }
        break;
      case TEST_RESULT_SKIP:
        skipped++;
        if(!quiet) {
          printf("  ");
          print_colored("○", TEST_COLOR_YELLOW);
          printf(" %s\n", testFiles[i]);
        }
        break;
      case TEST_RESULT_FAIL:
        snprintf(failures[failed].name, sizeof(failures[failed].name), "%s",
                 testFiles[i]);
        failures[failed].line = line;
        snprintf(failures[failed].msg, sizeof(failures[failed].msg), "%s", msg);
        failed++;
        if(!quiet) {
          printf("  ");
          print_colored("✗", TEST_COLOR_RED);
          printf(" %s (%lldms)\n      %s\n", testFiles[i], testElapsedMs, msg);
        }
        break;
    }

    free(testFiles[i]);
  }

  if(quietSink != NULL) {
    message_set_log_file(prevLogFile);
    fclose(quietSink);
  }

  long long elapsedMs = test_monotonic_ms() - startMs;
  int       ran = passed + failed;

  if(quiet) {
    printf("%d of %d tests failed in %lldms\n", failed, ran, elapsedMs);
    for(int i = 0; i < failed; i++) {
      // Strip the ".wren" extension for the display name; is_test_file()
      // guarantees every entry here ends with it. A %.*s precision avoids
      // the fixed stem buffer that -Wformat-truncation rejected.
      size_t stemLen = strlen(failures[i].name);
      if(stemLen > 5)
        stemLen -= 5;
      printf("\n### FAIL %s/%s:%d - %.*s\n%s\n", TESTS_DIR, failures[i].name,
             failures[i].line, (int)stemLen, failures[i].name, failures[i].msg);
    }
  } else {
    char line[64];
    printf("\nSummary:\n\n");
    printf("Total Tests: %d\n", ran);
    snprintf(line, sizeof(line), "Passed Tests: %d", passed);
    print_colored(line, TEST_COLOR_GREEN);
    printf("\n");
    snprintf(line, sizeof(line), "Failed Tests: %d", failed);
    print_colored(line, TEST_COLOR_RED);
    printf("\n");
    snprintf(line, sizeof(line), "Skipped Tests: %d", skipped);
    print_colored(line, TEST_COLOR_YELLOW);
    printf("\n");
    printf("Elapsed Time: %lldms\n", elapsedMs);
  }

  return (failed > 0) ? 1 : 0;
}
static char resolved_db_path[PATH_MAX];

// Runs a statement and reports failure. Every sqlite3_exec below previously
// discarded its return code, so a pragma that did not apply -- WAL mode on a
// filesystem that cannot support it, for instance -- left the database running
// with different durability than configured, silently.
static void sqlite_exec_checked(const char* sql) {
  char* errmsg = NULL;
  if(sqlite3_exec(db, sql, NULL, NULL, &errmsg) != SQLITE_OK) {
    message(red("SQL Error"), sql, errmsg ? errmsg : sqlite3_errmsg(db));
  }
  sqlite3_free(errmsg);
}

static void apply_sqlite_pragmas() {
  char pragma_cmd[256];

  // Foreign keys (configurable: 0=OFF, 1=ON)
  snprintf(pragma_cmd, sizeof(pragma_cmd), "PRAGMA foreign_keys = %s;",
           bialet_config.sqlite_foreign_keys ? "ON" : "OFF");
  sqlite_exec_checked(pragma_cmd);

  // Synchronous mode (configurable: 0=OFF, 1=NORMAL, 2=FULL, 3=EXTRA)
  const char* sync_modes[] = {"OFF", "NORMAL", "FULL", "EXTRA"};
  int         sync_mode = bialet_config.sqlite_synchronous;
  if(sync_mode < 0 || sync_mode > 3)
    sync_mode = 1; // Default to NORMAL
  snprintf(pragma_cmd, sizeof(pragma_cmd), "PRAGMA synchronous = %s;",
           sync_modes[sync_mode]);
  sqlite_exec_checked(pragma_cmd);

  // WAL mode (configurable via wal_mode flag)
  if(bialet_config.wal_mode) {
    sqlite_exec_checked("PRAGMA journal_mode = WAL;");
  }
  sqlite_exec_checked("PRAGMA journal_size_limit = " BIALET_SQLITE_JOURNAL_SIZE ";");
  sqlite_exec_checked("PRAGMA mmap_size = " BIALET_SQLITE_MMAP_SIZE ";");
  sqlite_exec_checked("PRAGMA cache_size = " BIALET_SQLITE_CACHE_SIZE ";");
  if(sqlite3_busy_timeout(db, BIALET_SQLITE_BUSY_TIMEOUT) != SQLITE_OK) {
    message(red("SQL Error"), "Could not set busy timeout");
  }
}

// Enables the development flags (live reload + showing errors in the browser)
// in the BIALET_CONFIG table. Idempotent: missing or disabled values are set
// to "1", already-enabled values are left untouched. Runs in dev mode so the
// behavior is one-time and self-healing across restarts.
void bialet_enable_dev_flags() {
  if(db == NULL)
    return;

  sqlite_exec_checked("CREATE TABLE IF NOT EXISTS BIALET_CONFIG (key TEXT PRIMARY "
                      "KEY, val TEXT)");

  const char* sql =
      "INSERT OR REPLACE INTO BIALET_CONFIG (key, val) VALUES (?, '1')";
  const char* keys[] = {LIVERELOAD_KEY, SHOW_ERRORS_KEY};

  for(size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
    sqlite3_stmt* stmt = NULL;
    if(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
      continue;
    sqlite3_bind_text(stmt, 1, keys[i], -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }
}

void bialet_init(struct BialetConfig* config) {
  bialet_config = *config;
  char db_path[PATH_MAX];

  // strlen() - 1 was stored in an int and then indexed unconditionally, so an
  // empty -d value gave config->db_path[-1] -- an out-of-bounds read *and*
  // write, into argv memory.
  size_t db_len = strlen(config->db_path);
  if(db_len == 0) {
    message(red("Error"), "Database path is empty");
    exit(BIALET_SQLITE_ERROR);
  }

  // A drive-qualified path (C:\...) is absolute on Windows; without this check
  // a temp DB returned by GetTempFileNameA would be joined onto root_dir.
  int is_abs = config->db_path[0] == '/';
#ifdef _WIN32
  if(!is_abs && db_len >= 3 && config->db_path[1] == ':' &&
     (config->db_path[2] == '/' || config->db_path[2] == '\\')) {
    is_abs = 1;
  }
#endif
  if(is_abs) {
    if(db_len >= sizeof(db_path)) {
      message(red("Error"), "Database path too long");
      exit(BIALET_SQLITE_ERROR);
    }
    memcpy(db_path, config->db_path, db_len + 1);
  } else {
    // Trim trailing slashes on our copy instead of mutating the caller's
    // argv-backed string in place.
    while(db_len > 0 && config->db_path[db_len - 1] == '/') {
      db_len--;
    }
    if(db_len == 0) {
      message(red("Error"), "Database path is empty");
      exit(BIALET_SQLITE_ERROR);
    }
    int ret = snprintf(db_path, sizeof(db_path), "%s/%.*s", config->root_dir,
                       (int)db_len, config->db_path);
    if(ret < 0 || ret >= (int)sizeof(db_path)) {
      message(red("Error"), "Database path too long");
      exit(BIALET_SQLITE_ERROR);
    }
  }
  strncpy(resolved_db_path, db_path, sizeof(resolved_db_path) - 1);
  resolved_db_path[sizeof(resolved_db_path) - 1] = '\0';
  if(sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                     NULL) != SQLITE_OK) {
    message(red("SQL Error"), "Can't open database in", config->db_path);
    exit(BIALET_SQLITE_ERROR);
  }
  apply_sqlite_pragmas();

  wrenInitConfiguration(&wren_config);
  wren_config.writeFn = &bialet_wren_write;
  wren_config.errorFn = &bialet_wren_error;
  wren_config.queryFn = &query_execute;
  wren_config.loadModuleFn = &bialet_wren_load_module;
  wren_config.enableTests = config->enable_tests;

  http_call_init(&bialet_config);
}

void bialet_cleanup() {
  if(db) {
    // sqlite3_close() fails with SQLITE_BUSY when any statement is still
    // unfinalized and then leaves the handle open; its return was discarded, so
    // the connection just leaked. sqlite3_close_v2() marks the handle as a
    // zombie and releases it once the last statement goes away.
    int rc = sqlite3_close_v2(db);
    if(rc != SQLITE_OK) {
      message(red("SQL Error"), "Error closing database", sqlite3_errstr(rc));
    }
    db = NULL;
  }
}

// Re-opens the SQLite connection against the same resolved path, re-applying
// the configured pragmas. Used after fork() so the HTTP child does not keep
// sharing the parent's pre-fork connection with the cron/dmon threads, which
// SQLite forbids.
void bialet_reopen_db() {
  if(db) {
    sqlite3_close_v2(db);
    db = NULL;
  }
  if(sqlite3_open_v2(resolved_db_path, &db,
                     SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                     NULL) != SQLITE_OK) {
    message(red("SQL Error"), "Can't reopen database after fork");
    exit(BIALET_SQLITE_ERROR);
  }
  apply_sqlite_pragmas();
}

BialetQuery* create_bialet_query() {
  BialetQuery* query = (BialetQuery*)malloc(sizeof(BialetQuery));
  if(query == NULL)
    return NULL;
  query->results = NULL;
  query->resultsCount = 0;
  query->parameters = NULL;
  query->parametersCount = 0;
  query->queryString = NULL;
  query->lastInsertId = NULL;
  return query;
}

int add_result(BialetQuery* query) {
  int                count = query->resultsCount + 1;
  BialetQueryResult* new_results = (BialetQueryResult*)realloc(
      query->results, (size_t)count * sizeof(BialetQueryResult));
  if(new_results == NULL)
    return -1;
  query->results = new_results;
  query->resultsCount = count;
  BialetQueryResult* newResult = &query->results[count - 1];
  newResult->rows = NULL;
  newResult->rowCount = 0;
  return 0;
}

int add_result_row(BialetQuery* query, int resultIndex, const char* name,
                   const char* value, int size, BialetQueryType type) {
  if(resultIndex < 0 || resultIndex >= query->resultsCount)
    return -1;

  BialetQueryResult* result = &query->results[resultIndex];
  int                count = result->rowCount + 1;
  BialetQueryRow*    new_rows =
      (BialetQueryRow*)realloc(result->rows, (size_t)count * sizeof(BialetQueryRow));
  if(new_rows == NULL)
    return -1;
  result->rows = new_rows;
  result->rowCount = count;
  BialetQueryRow* newRow = &result->rows[count - 1];
  newRow->name = string_safe_copy(name != NULL ? name : "");
  if(value != NULL && size > 0) {
    newRow->value = safe_malloc((size_t)size);
    memcpy(newRow->value, value, size);
  } else {
    newRow->value = string_safe_copy("");
  }
  newRow->size = size;
  newRow->type = type;
  return 0;
}

int add_parameter(BialetQuery* query, const char* value, BialetQueryType type) {
  char* copy = value != NULL ? strdup(value) : NULL;
  if(value != NULL && copy == NULL)
    return -1;
  int                   count = query->parametersCount + 1;
  BialetQueryParameter* new_parameters = (BialetQueryParameter*)realloc(
      query->parameters, (size_t)count * sizeof(BialetQueryParameter));
  if(new_parameters == NULL) {
    free(copy);
    return -1;
  }
  query->parameters = new_parameters;
  query->parametersCount = count;
  BialetQueryParameter* newParameter = &query->parameters[count - 1];
  newParameter->value = copy;
  newParameter->type = type;
  return 0;
}

void free_bialet_query(BialetQuery* query) {
  if(!query)
    return; // Guard clause to prevent dereferencing a NULL pointer

  // Free each row in each result
  for(int i = 0; i < query->resultsCount; i++) {
    for(int j = 0; j < query->results[i].rowCount; j++) {
      free(query->results[i].rows[j].name); // Free row name
      query->results[i].rows[j].name = NULL;

      free(query->results[i].rows[j].value); // Free row value
      query->results[i].rows[j].value = NULL;
    }
    free(query->results[i].rows); // Free the rows array itself
    query->results[i].rows = NULL;
  }
  free(query->results); // Free the results array
  query->results = NULL;

  // Free each parameter value
  for(int i = 0; i < query->parametersCount; i++) {
    free(query->parameters[i].value);
    query->parameters[i].value = NULL;
  }
  free(query->parameters); // Free the parameters array
  query->parameters = NULL;

  // Free queryString and lastInsertId if they exist
  if(query->queryString) {
    free(query->queryString);
    query->queryString = NULL;
  }
  if(query->lastInsertId) {
    free(query->lastInsertId);
    query->lastInsertId = NULL;
  }

  // Finally, free the BialetQuery structure itself
  free(query);
}
