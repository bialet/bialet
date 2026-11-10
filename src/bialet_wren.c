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
#include <limits.h>
#include <sqlite3.h>
#include <stdint.h>
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

// Aggregate ceilings for unprocessed uploads stored in BIALET_FILES. The
// per-request caps only bound one request; without an aggregate bound an
// unauthenticated client could keep posting 100-part bodies and grow the shared
// database until every write fails with SQLITE_FULL. A byte sum alone is not
// enough: a file part with a filename and no data is a legal row of size 0, so
// a row ceiling is needed too. Both aggregates are read through the index on
// isTemp. When either is exhausted the request is refused with 507 instead of
// being served with the file silently missing.
#define MAX_TEMP_UPLOAD_BYTES ((sqlite3_int64)64 * 1024 * 1024)
#define MAX_TEMP_UPLOAD_ROWS 4096

// Deletes temp uploads older than a day. Called at startup and, throttled, from
// the upload path: the old code only purged inside save_uploaded_files, so an
// app that stopped receiving multipart requests never reclaimed anything.
static void purge_temp_uploads(void) {
  if(db == NULL)
    return;
  sqlite3_stmt* stmt = NULL;
  if(sqlite3_prepare_v2(db,
                        "DELETE FROM BIALET_FILES WHERE isTemp = 1 AND "
                        "createdAt < datetime('now', '-1 day')",
                        -1, &stmt, 0) == SQLITE_OK) {
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }
}

// Total bytes and row count currently held by temporary uploads. One statement
// for both aggregates, over the index on isTemp: the pre-handler path must not
// pay a full scan of every file row, including the app's own saved files.
static void temp_upload_usage(sqlite3_int64* bytes, sqlite3_int64* rows) {
  *bytes = 0;
  *rows = 0;
  sqlite3_stmt* stmt = NULL;
  if(db == NULL)
    return;
  if(sqlite3_prepare_v2(db,
                        "SELECT IFNULL(SUM(size), 0), COUNT(*) FROM BIALET_FILES "
                        "WHERE isTemp = 1",
                        -1, &stmt, 0) == SQLITE_OK) {
    if(sqlite3_step(stmt) == SQLITE_ROW) {
      *bytes = sqlite3_column_int64(stmt, 0);
      *rows = sqlite3_column_int64(stmt, 1);
    }
    sqlite3_finalize(stmt);
  }
}

// Delete the oldest retained temporary upload and report the bytes it actually
// freed, or -1 when nothing is evictable. Measuring the removed row is what
// keeps the ceiling honest: predicting the freed bytes from the pool mean
// under-frees whenever the oldest rows are smaller than average, and the caller
// then refused a part that would have fit.
static sqlite3_int64 evict_oldest_temp_upload(void) {
  if(db == NULL)
    return -1;
  sqlite3_stmt* sel = NULL;
  sqlite3_int64 id = -1;
  sqlite3_int64 size = 0;
  if(sqlite3_prepare_v2(db,
                        "SELECT id, IFNULL(size, 0) FROM BIALET_FILES "
                        "WHERE isTemp = 1 ORDER BY createdAt ASC, id ASC LIMIT 1",
                        -1, &sel, 0) != SQLITE_OK)
    return -1;
  if(sqlite3_step(sel) == SQLITE_ROW) {
    id = sqlite3_column_int64(sel, 0);
    size = sqlite3_column_int64(sel, 1);
  }
  sqlite3_finalize(sel);
  if(id < 0)
    return -1;
  sqlite3_stmt* del = NULL;
  if(sqlite3_prepare_v2(db, "DELETE FROM BIALET_FILES WHERE id = ?", -1, &del, 0) ==
     SQLITE_OK) {
    sqlite3_bind_int64(del, 1, id);
    sqlite3_step(del);
    sqlite3_finalize(del);
  }
  return size;
}

// Make room for a part of [fileSize] bytes by evicting the oldest temporary
// rows one at a time and subtracting the bytes each deletion really freed, so a
// byte deficit is always closed by real space. A part that cannot fit an empty
// pool is refused before anything is evicted; a part that still cannot fit after
// every evictable row is gone is refused by the caller's second test.
static void make_room_for_temp_upload(sqlite3_int64 fileSize) {
  if(fileSize > MAX_TEMP_UPLOAD_BYTES)
    return;
  sqlite3_int64 bytes = 0;
  sqlite3_int64 rows = 0;
  temp_upload_usage(&bytes, &rows);
  for(int guard = 0; guard < 2 * MAX_TEMP_UPLOAD_ROWS; guard++) {
    if(rows < MAX_TEMP_UPLOAD_ROWS && bytes + fileSize <= MAX_TEMP_UPLOAD_BYTES)
      return;
    sqlite3_int64 freed = evict_oldest_temp_upload();
    if(freed < 0)
      return; // nothing evictable left: the caller's second test refuses
    bytes -= freed;
    if(bytes < 0)
      bytes = 0;
    if(rows > 0)
      rows--;
  }
}

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
  // Wren text is not trusted at the log boundary: an app may log a request
  // value (docs/reference.md shows System.log("Request from %(Request.uri)")),
  // and this stream is the operator's log file, stdout and BIALET_LOGS. Escape
  // control bytes exactly like every C-side wire-derived log value, so a value
  // cannot forge a record or drive the terminal.
  char*       safe = log_escape(message);
  const char* text = safe ? safe : "(unprintable log line)";
  message(yellow("Log"), text);
  // A failed prepare (e.g. SQLITE_BUSY on the shared connection) leaves stmt
  // NULL; binding/stepping it would NULL-deref the request thread.
  sqlite3_stmt* stmt = NULL;
  if(sqlite3_prepare_v2(db, "INSERT INTO BIALET_LOGS (message) VALUES (?)", -1,
                        &stmt, 0) == SQLITE_OK) {
    sqlite3_bind_text(stmt, 1, text, -1, SQLITE_STATIC);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }
  free(safe);
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

  // Apply the same private-file boundary the HTTP layer enforces: a component
  // starting with '_' or '.' is not readable through the Wren file API either.
  // Without this, Markdown.file(Request.get("page")) could read the app's own
  // _db.sqlite3, .env or .git/config, all of which the URL router refuses to
  // serve. Only the root-relative portion is checked so an app hosted under a
  // "_"- or "."-named directory still works.
  if(has_forbidden_uri_component(resolved + root_len)) {
    char* log_path = log_escape(path);
    message(red("Error"), "Private file access blocked in bialetReadFile",
            log_path ? log_path : "(unprintable)");
    free(log_path);
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

// Resolve a (possibly relative) import name into an absolute, canonical
// module path before Wren registers it as the importer for any imports the
// loaded module makes in turn. Without this step, a relative "./x" import
// inside a file that was itself reached transitively (e.g. "pages/a" imports
// "./b", and "b" imports "./c") would resolve against a fixed base -- the
// entry file's own directory -- instead of "b"'s own directory, breaking as
// soon as "b" is imported from anywhere other than directly by the entry
// file.
//
// Every write to `module` is bounded by sizeof(module). The old code mixed
// two constants for this one buffer: it was declared MAX_URL_LEN (1024) but
// appended to with `MAX_MODULE_LEN - strlen(module) - 1` (256-based), so once
// the prefix passed 255 characters that bound wrapped around to a value near
// SIZE_MAX and strncat's limit stopped limiting anything. Only an unrelated
// length guard kept it from overflowing.
static const char* bialet_wren_resolve_module(WrenVM* vm, const char* importer,
                                              const char* name) {
  (void)vm;

  // URLs and "gh:" references are canonical identifiers on their own -- no
  // directory joining applies to them.
  if(strchr(name, ':') != NULL)
    return name;

  // Wren's built-in optional modules ("random", "meta") are looked up by
  // this exact bare name once loadModuleFn reports no source -- see
  // importModule() in wren_vm.c. Leave them untouched so that fallback still
  // matches; joining them against a directory would only ever produce a
  // nonexistent file path.
  if(strcmp(name, "random") == 0 || strcmp(name, "meta") == 0)
    return name;

  char module[MAX_URL_LEN];

  if(name[0] == '/') {
    if(strlen(name) + strlen(bialet_config.full_root_dir) + 1 > sizeof(module)) {
      message(red("Error"), "Module name too long.");
      return NULL;
    }
    snprintf(module, sizeof(module), "%s", bialet_config.full_root_dir);
  } else {
    // Relative import: resolve against the *importing* module's own
    // directory -- the `importer` argument Wren passes in here -- rather
    // than a fixed value, so transitively imported files can use "./x"
    // imports relative to themselves.
    char* calledFrom = string_safe_copy(importer);
    if(calledFrom == NULL) {
      fprintf(stderr,
              "Error: Cannot resolve relative import '%s' without application "
              "context.\n       Use: bialet -t <file> <app_root>\n",
              name);
      return NULL;
    }
    // Strip the filename, keeping the calling module's directory. Windows
    // paths use backslashes, so find the last separator of either kind.
    char* last_sep = NULL;
    for(char* p = calledFrom; *p != '\0'; p++) {
      if(*p == '/' || *p == '\\')
        last_sep = p;
    }
    if(strlen(name) + strlen(calledFrom) + 2 > sizeof(module)) {
      message(red("Error"), "Module name too long.");
      free(calledFrom);
      return NULL;
    }
    if(last_sep)
      *last_sep = '\0';
    snprintf(module, sizeof(module), "%s/", calledFrom);
    free(calledFrom);
  }

  size_t used = strlen(module);
  if(used + 1 >= sizeof(module)) {
    message(red("Error"), "Module name too long.");
    return NULL;
  }
  strncat(module, name, sizeof(module) - used - 1);

  return string_safe_copy(module);
}

static WrenLoadModuleResult bialet_wren_load_module(WrenVM* vm, const char* name) {
  (void)vm;

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

  // By the time we get here, `name` is already an absolute, canonical
  // module path produced by bialet_wren_resolve_module() -- no directory
  // joining is needed, only the (still relative-to-this-call) extension
  // normalization and containment check below.
  if(strlen(name) >= sizeof(module)) {
    message(red("Error"), "Module name too long.");
    return result;
  }
  snprintf(module, sizeof(module), "%s", name);

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
  // Wren error text can embed request bytes (Fiber.abort of a request value, or
  // the framework's JsonScanner quoting the token it rejected). Escape it for
  // the log only; show_errors_capture() keeps the raw text for the error page,
  // which HTML-escapes it before rendering.
  char*       safeMsg = log_escape(msg);
  const char* logMsg = safeMsg ? safeMsg : "(unprintable)";
  switch(errorType) {
    case WREN_ERROR_COMPILE: {
      message(red("Compilation Error"), lineMessage, (char*)logMsg);
      show_errors_capture("Compilation Error", module, line, msg);
    } break;
    case WREN_ERROR_STACK_TRACE: {
      message(red("Stack Error"), lineMessage, (char*)logMsg);
      show_errors_capture("Stack Error", module, line, msg);
    } break;
    case WREN_ERROR_RUNTIME: {
      message(red("Runtime Error"), (char*)logMsg);
      show_errors_capture("Runtime Error", module, line, msg);
    } break;
  }
  free(safeMsg);
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
          size = 0;
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
//
// memchr() finds the candidate positions of the needle's first byte using the
// platform's vectorized scan, so a multipart body is walked once per searched
// pattern instead of comparing [needle] bytes at every offset. With up to
// MAX_UPLOAD_FILES parts, each part previously rescanned the whole remaining
// body byte by byte.
static const char* mem_find(const char* hay, size_t hay_len, const char* needle,
                            size_t needle_len) {
  if(needle_len == 0 || hay_len < needle_len)
    return NULL;
  if(needle_len == 1)
    return (const char*)memchr(hay, needle[0], hay_len);

  const char* p = hay;
  size_t      remaining = hay_len - needle_len + 1;
  while(remaining > 0) {
    const char* hit = (const char*)memchr(p, needle[0], remaining);
    if(hit == NULL)
      return NULL;
    if(memcmp(hit, needle, needle_len) == 0)
      return hit;
    // Resume one byte after this candidate; recompute how much is left.
    remaining -= (size_t)(hit - p) + 1;
    p = hit + 1;
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

// What save_uploaded_files() learned about the request beyond the file ids.
//
// For a multipart request the parts are already being walked there, so the
// text fields are collected in the same pass. [fields] is a heap string of
// percent-encoded "name=value&name=value" pairs for Request.init to decode
// (NULL when there are none); the caller frees it. [head_len] is how many bytes
// of the message Wren needs to see: the request line and headers only, so that
// file content is never copied into the VM or split by it line by line. For any
// other request it is the whole message.
struct MultipartInfo {
  char*  fields;
  size_t fields_len;
  size_t head_len;
  int    is_multipart;
  int    storage_full;
};

struct FieldBuf {
  char*  data;
  size_t len;
  size_t cap;
  int    failed;
};

static int field_buf_reserve(struct FieldBuf* fb, size_t extra) {
  if(fb->failed)
    return 0;
  if(extra > SIZE_MAX - fb->len - 1) {
    fb->failed = 1;
    return 0;
  }
  size_t need = fb->len + extra + 1;
  if(need <= fb->cap)
    return 1;
  size_t cap = fb->cap ? fb->cap : 256;
  while(cap < need) {
    if(cap > SIZE_MAX / 2) {
      cap = need;
      break;
    }
    cap *= 2;
  }
  char* grown = (char*)realloc(fb->data, cap);
  if(grown == NULL) {
    fb->failed = 1;
    return 0;
  }
  fb->data = grown;
  fb->cap = cap;
  return 1;
}

static void field_buf_put(struct FieldBuf* fb, char c) {
  if(field_buf_reserve(fb, 1))
    fb->data[fb->len++] = c;
}

// Appends [src] percent-encoded. With [normalize_newlines], CRLF becomes a
// single LF, which is what a <textarea> value should look like to the app.
//
// Only ASCII letters, digits and "-_.~" are left as they are. isalnum() is not
// used: it follows the process locale and may accept bytes above 127.
static int is_unreserved(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
         (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}

static int skips_cr(const char* src, size_t n, size_t i, int normalize_newlines) {
  return normalize_newlines && src[i] == '\r' && i + 1 < n && src[i + 1] == '\n';
}

static void field_buf_put_encoded(struct FieldBuf* fb, const char* src, size_t n,
                                  int normalize_newlines) {
  static const char hex[] = "0123456789ABCDEF";
  // Measure first so the buffer grows by exactly what is needed. Reserving the
  // worst case (3 bytes per input byte) would triple the footprint of a large
  // text field for nothing.
  size_t out = 0;
  for(size_t i = 0; i < n; i++) {
    if(skips_cr(src, n, i, normalize_newlines))
      continue;
    size_t add = is_unreserved((unsigned char)src[i]) ? 1 : 3;
    if(out > SIZE_MAX - add) {
      fb->failed = 1;
      return;
    }
    out += add;
  }
  if(!field_buf_reserve(fb, out))
    return;
  for(size_t i = 0; i < n; i++) {
    unsigned char c = (unsigned char)src[i];
    if(skips_cr(src, n, i, normalize_newlines))
      continue;
    if(is_unreserved(c)) {
      fb->data[fb->len++] = (char)c;
    } else {
      fb->data[fb->len++] = '%';
      fb->data[fb->len++] = hex[c >> 4];
      fb->data[fb->len++] = hex[c & 15];
    }
  }
}

// Case-insensitive equality of two spans of [n] bytes.
static int header_name_eq(const char* a, const char* b, size_t n) {
  for(size_t i = 0; i < n; i++) {
    if(tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
      return 0;
  }
  return 1;
}

// Returns the value of the LAST header line whose field name is [name],
// anchored at the start of a line (so "X-Content-Type:" does not match) and
// bounded to that line, matching the Wren header map's last-wins semantics.
// On a hit, *value_end points at the CR/LF that ends the value line.
static const char* find_last_header_value(const char* msg, size_t head_len,
                                          const char* name, size_t name_len,
                                          const char** value_end) {
  const char* p = msg;
  const char* end = msg + head_len;
  const char* found = NULL;

  // Skip the request line first: a header name must start a header line.
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
    // A line that is blank after trimming SP/HTAB ends the header block, the
    // same rule Request.init applies. Without this the detector read a
    // Content-Type the app-visible map had already treated as body bytes.
    size_t visible = 0;
    for(size_t i = 0; i < line_len; i++)
      if(p[i] != ' ' && p[i] != '\t')
        visible++;
    if(visible == 0)
      break;
    // A field name may not contain whitespace before the colon; the app-visible
    // map skips such a line too, so a request can never be multipart to one
    // parser and not to the other.
    if(line_len > name_len && p[name_len] == ':' &&
       header_name_eq(p, name, name_len)) {
      const char* v = p + name_len + 1;
      while(v < eol && (*v == ' ' || *v == '\t'))
        v++;
      found = v;
      if(value_end != NULL)
        *value_end = eol;
    }
    if(eol + 1 >= end)
      break;
    p = eol + 2;
  }
  return found;
}

static int ascii_ieq(char a, char b) {
  return tolower((unsigned char)a) == tolower((unsigned char)b);
}

// Finds a header line inside a part's header block. Anchored to the start of a
// line so the name cannot match inside another header's value.
static const char* find_part_header(const char* head, size_t head_len,
                                    const char* name, size_t name_len) {
  const char* p = head;
  size_t      left = head_len;
  while(left >= name_len) {
    const char* hit = mem_find_ci(p, left, name, name_len);
    if(hit == NULL)
      return NULL;
    if(hit == head || hit[-1] == '\n')
      return hit;
    left -= (size_t)(hit - p) + 1;
    p = hit + 1;
  }
  return NULL;
}

// Reads parameter [key] from a header value such as
//   form-data; name="a\"b"; filename="x.csv"
// into [out], bounded by [out_size]. Quoted values may escape '"' and '\' with
// a backslash; unquoted tokens are accepted too. Returns 1 when the parameter
// is present, even with an empty value, and [out] is always NUL-terminated.
// Matching whole parameter names is what keeps `name` from matching the tail of
// `filename`, and only the requested parameter is ever copied into [out].
static int header_param(const char* v, size_t len, const char* key, char* out,
                        size_t out_size) {
  size_t key_len = strlen(key);
  size_t i = 0;
  out[0] = '\0';
  while(i < len && v[i] != ';')
    i++;
  while(i < len) {
    i++; // the ';'
    while(i < len && (v[i] == ' ' || v[i] == '\t'))
      i++;
    size_t key_start = i;
    while(i < len && v[i] != '=' && v[i] != ';')
      i++;
    size_t key_end = i;
    while(key_end > key_start && (v[key_end - 1] == ' ' || v[key_end - 1] == '\t'))
      key_end--;
    int match = key_end - key_start == key_len;
    for(size_t k = 0; match && k < key_len; k++)
      match = ascii_ieq(v[key_start + k], key[k]);

    size_t n = 0;
    if(i < len && v[i] == '=') {
      i++;
      while(i < len && (v[i] == ' ' || v[i] == '\t'))
        i++;
      if(i < len && v[i] == '"') {
        i++;
        while(i < len && v[i] != '"') {
          char c = v[i];
          if(c == '\\' && i + 1 < len && (v[i + 1] == '"' || v[i + 1] == '\\'))
            c = v[++i];
          if(match && n + 1 < out_size)
            out[n++] = c;
          i++;
        }
        if(i < len)
          i++; // the closing quote
      } else {
        while(i < len && v[i] != ';' && v[i] != ' ' && v[i] != '\t' &&
              v[i] != '\r' && v[i] != '\n') {
          if(match && n + 1 < out_size)
            out[n++] = v[i];
          i++;
        }
      }
    }
    if(match) {
      out[n] = '\0';
      return 1;
    }
    while(i < len && v[i] != ';')
      i++;
  }
  return 0;
}

static const char* next_part(const char* resume, const char* end,
                             const char* start_boundary, size_t sb_len) {
  return resume ? mem_find(resume, (size_t)(end - resume), start_boundary, sb_len)
                : NULL;
}

// Saves the file parts of a multipart request to BIALET_FILES (their ids go to
// [filesIds]) and collects its text fields into [mp]. Returns 1 when the
// request carried a multipart body.
int save_uploaded_files(struct HttpMessage* hm, char* filesIds,
                        struct MultipartInfo* mp) {
  filesIds[0] = '\0'; // Initialize empty string
  mp->fields = NULL;
  mp->fields_len = 0;
  mp->is_multipart = 0;
  mp->storage_full = 0;

  const char* msg = hm->message.str;
  size_t      msg_len = hm->message.len;
  mp->head_len = msg_len;
  if(msg == NULL || msg_len == 0)
    return 0;

  // End of the request header block.
  const char* headerEnd = mem_find(msg, msg_len, "\r\n\r\n", 4);
  if(!headerEnd)
    return 0;
  size_t head_len = (size_t)(headerEnd - msg);

  // The declaration must be the value of a real Content-Type field, not a
  // substring of some other header (or of the request line): the old
  // unanchored search let "X-Content-Type: multipart/form-data; boundary=zz"
  // select the multipart parser while Request.header("content-type") was null.
  static const char kContentType[] = "Content-Type";
  const char*       ctEnd = NULL;
  const char* ctStart = find_last_header_value(msg, head_len, kContentType,
                                               sizeof(kContentType) - 1, &ctEnd);
  if(!ctStart || ctEnd == NULL || ctEnd <= ctStart)
    return 0;

  static const char kMultipart[] = "multipart/form-data";
  size_t            ct_avail = (size_t)(ctEnd - ctStart);
  if(ct_avail < sizeof(kMultipart) - 1 ||
     mem_find_ci(ctStart, sizeof(kMultipart) - 1, kMultipart,
                 sizeof(kMultipart) - 1) != ctStart)
    return 0;

  // The boundary is a parameter of this field only, so the search ends at the
  // header line's CRLF instead of running to the end of the header block (where
  // a later "X-Boundary: boundary=zz" used to supply it).
  static const char kBoundary[] = "boundary=";
  const char*       boundaryStart =
      mem_find_ci(ctStart, ct_avail, kBoundary, sizeof(kBoundary) - 1);
  if(!boundaryStart)
    return 0;
  boundaryStart += sizeof(kBoundary) - 1;

  // The boundary may be quoted (RFC 2046 allows it, and it is required when it
  // contains characters such as ':' or ' ').
  char   boundary[256];
  size_t boundary_len;
  if(boundaryStart < ctEnd && *boundaryStart == '"') {
    boundary_len = copy_until(boundaryStart + 1, (size_t)(ctEnd - boundaryStart) - 1,
                              "\"", boundary, sizeof(boundary));
  } else {
    boundary_len = copy_until(boundaryStart, (size_t)(ctEnd - boundaryStart), "; \t",
                              boundary, sizeof(boundary));
  }
  // RFC 2046 caps a boundary at 70 characters. One that fills the buffer was
  // truncated, and matching on a truncated delimiter would split the body at the
  // wrong places.
  if(boundary_len == 0 || boundary_len >= sizeof(boundary) - 1)
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

  // Reclaim expired temp uploads before accepting more (at most once a minute
  // per process; startup does an unconditional purge).
  {
    static time_t last_purge = 0;
    time_t        now = time(NULL);
    if(now - last_purge >= 60) {
      last_purge = now;
      purge_temp_uploads();
    }
  }
  sqlite3_int64 tempBytes = 0;
  sqlite3_int64 tempRows = 0;
  temp_upload_usage(&tempBytes, &tempRows);

  const char* cursor = mem_find(body, (size_t)(end - body), startBoundary, sb_len);
  int         firstFile = 1;
  int         uploadedFiles = 0;
  int         filesCapped = 0;
  int         fieldsCapped = 0;
  size_t      fieldBytes = 0;

  struct FieldBuf fb = {0};

  if(cursor != NULL) {
    mp->is_multipart = 1;
    mp->head_len = (size_t)(body - msg);
  }

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

    // Where this part ends comes first, and the header search is bounded by it.
    // Searching for the blank line up to the end of the whole body let a part
    // without one run into the next parts (swallowing their fields), and made
    // each such part rescan the rest of the body: quadratic in the part count.
    const char* nextDelim = mem_find(part, (size_t)(end - part), delimiter, dl_len);
    const char* fileEnd = nextDelim ? nextDelim : end;
    const char* resume = nextDelim ? nextDelim + 2 : NULL; // skip the CRLF
    // The blank line may end on the CRLF that opens the delimiter, which is
    // what an empty value looks like.
    const char* searchEnd = nextDelim ? nextDelim + 2 : end;

    const char* partHeaderEnd =
        mem_find(part, (size_t)(searchEnd - part), "\r\n\r\n", 4);
    if(!partHeaderEnd) {
      cursor = next_part(resume, end, startBoundary, sb_len);
      continue;
    }
    size_t      part_head_len = (size_t)(partHeaderEnd - part);
    const char* fileData = partHeaderEnd + 4;
    if(fileData > fileEnd)
      fileData = fileEnd;

    static const char kDisposition[] = "Content-Disposition:";
    const char*       cdStart = find_part_header(part, part_head_len, kDisposition,
                                                 sizeof(kDisposition) - 1);
    if(!cdStart) {
      cursor = next_part(resume, end, startBoundary, sb_len);
      continue;
    }
    const char* cdValue = cdStart + sizeof(kDisposition) - 1;
    size_t      cd_avail = part_head_len - (size_t)(cdValue - part);
    const char* cdEol = (const char*)memchr(cdValue, '\n', cd_avail);
    size_t      cd_len = cdEol ? (size_t)(cdEol - cdValue) : cd_avail;

    char fieldName[1024];
    header_param(cdValue, cd_len, "name", fieldName, sizeof(fieldName));

    char filename[256];
    int  isFile =
        header_param(cdValue, cd_len, "filename", filename, sizeof(filename));
    if(!isFile) {
      char ignored[2];
      isFile = header_param(cdValue, cd_len, "filename*", ignored, sizeof(ignored));
    }

    // A part without a filename is a plain text field. The body is already
    // capped by max_post_size, but enforce the same cap on what is collected
    // here so a field can never outgrow it.
    if(!isFile) {
      if(fieldName[0] != '\0' && !fieldsCapped) {
        size_t nameLen = strlen(fieldName);
        size_t valueLen = (size_t)(fileEnd - fileData);
        if(nameLen > bialet_config.max_post_size ||
           valueLen > bialet_config.max_post_size - nameLen ||
           fieldBytes > bialet_config.max_post_size - nameLen - valueLen) {
          message(red("Upload Error"),
                  "Form fields exceed the maximum post size, skipping the rest");
          fieldsCapped = 1;
        } else {
          fieldBytes += nameLen + valueLen;
          if(fb.len > 0)
            field_buf_put(&fb, '&');
          field_buf_put_encoded(&fb, fieldName, nameLen, 0);
          field_buf_put(&fb, '=');
          field_buf_put_encoded(&fb, fileData, valueLen, 1);
        }
      }
      cursor = next_part(resume, end, startBoundary, sb_len);
      continue;
    }

    // An empty filename is a file input left blank in the browser.
    if(filename[0] == '\0') {
      cursor = next_part(resume, end, startBoundary, sb_len);
      continue;
    }

    char              contentTypeStr[256] = "application/octet-stream";
    static const char kPartContentType[] = "Content-Type:";
    const char* partCtStart = find_part_header(part, part_head_len, kPartContentType,
                                               sizeof(kPartContentType) - 1);
    if(partCtStart != NULL) {
      partCtStart += sizeof(kPartContentType) - 1;
      while(partCtStart < partHeaderEnd &&
            (*partCtStart == ' ' || *partCtStart == '\t'))
        partCtStart++;
      copy_until(partCtStart, (size_t)(partHeaderEnd - partCtStart), "\r\n;",
                 contentTypeStr, sizeof(contentTypeStr));
    }

    size_t fileSize = (size_t)(fileEnd - fileData);

    // Validate file size to prevent disk abuse
    if(fileSize > bialet_config.max_upload_size) {
      // The filename is wire-supplied: it comes from the part's
      // Content-Disposition byte for byte and may carry ESC or CR. Escape it
      // before it reaches the log, like every other attacker-controlled value.
      char  sizeMsg[1200];
      char* safeName = log_escape(filename);
      snprintf(
          sizeMsg, sizeof(sizeMsg),
          "File '%s' exceeds maximum upload size (%zu bytes > %zu bytes allowed)",
          safeName ? safeName : "(unprintable)", fileSize,
          bialet_config.max_upload_size);
      free(safeName);
      message(red("Upload Error"), sizeMsg);
      cursor = next_part(resume, end, startBoundary, sb_len);
      continue;
    }

    // Skip further files once the per-request file count cap is reached so a
    // single request cannot insert an unbounded number of rows into
    // BIALET_FILES (CPU/disk amplification). Text fields after the cap are
    // still read.
    if(uploadedFiles >= MAX_UPLOAD_FILES) {
      if(!filesCapped)
        message(red("Upload Error"), "Too many files in request, skipping rest");
      filesCapped = 1;
      cursor = next_part(resume, end, startBoundary, sb_len);
      continue;
    }

    // Aggregate cap: per-request limits do not bound total stored bytes and
    // rows. A refusal is reported to the caller (507) rather than served as a
    // 200 with the file silently missing, so an app cannot answer as if the
    // upload succeeded.
    if(tempRows >= MAX_TEMP_UPLOAD_ROWS ||
       tempBytes + (sqlite3_int64)fileSize > MAX_TEMP_UPLOAD_BYTES) {
      // Reclaim the oldest retained rows before refusing: without this an
      // anonymous client can fill the pool with zero-byte parts and deny every
      // other upload for the whole one-day retention window.
      make_room_for_temp_upload((sqlite3_int64)fileSize);
      temp_upload_usage(&tempBytes, &tempRows);
    }
    if(tempRows >= MAX_TEMP_UPLOAD_ROWS ||
       tempBytes + (sqlite3_int64)fileSize > MAX_TEMP_UPLOAD_BYTES) {
      if(!mp->storage_full)
        message(red("Upload Error"),
                "Temporary upload storage is full, refusing the remaining files");
      mp->storage_full = 1;
      cursor = next_part(resume, end, startBoundary, sb_len);
      continue;
    }

    // Save file to database
    sqlite3_stmt* stmt = NULL;
    int result = sqlite3_prepare_v2(db,
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
        tempBytes += (sqlite3_int64)fileSize;
        tempRows++;

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

    cursor = next_part(resume, end, startBoundary, sb_len);
  }

  if(fb.failed) {
    message(red("Upload Error"), "Out of memory reading form fields");
    free(fb.data);
  } else if(fb.len > 0) {
    fb.data[fb.len] = '\0';
    mp->fields = fb.data;
    mp->fields_len = fb.len;
  } else {
    free(fb.data);
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
  wrenInterpret(vm, MAIN_MODULE_NAME, MAIN_MODULE_SOURCE);
  if(hm) {
    /* Initialize request */
    wrenEnsureSlots(vm, 5);
    wrenGetVariable(vm, MAIN_MODULE_NAME, "Request", 0);
    WrenHandle* requestClass = wrenGetSlotHandle(vm, 0);
    WrenHandle* initMethod = wrenMakeCallHandle(vm, "init(_,_,_,_)");
    wrenSetSlotHandle(vm, 0, requestClass);

    char                 filesIds[MAX_URL_LEN] = "";
    struct MultipartInfo mp;
    // Save uploaded files with size validation (max_upload_size config) and
    // collect the text fields of a multipart body.
    save_uploaded_files(hm, filesIds, &mp);
    if(mp.storage_full) {
      // The aggregate temporary-upload ceiling is exhausted. Answer explicitly
      // instead of running the handler with the file missing, so the client
      // cannot mistake a refused upload for a successful one.
      free(mp.fields);
      wrenReleaseHandle(vm, requestClass);
      wrenReleaseHandle(vm, initMethod);
      wrenFreeVM(vm);
      r.status = 507;
      r.body = string_safe_copy("Temporary upload storage is full");
      r.body_owned = 1;
      return r;
    }
    if(mp.is_multipart) {
      // Only the request line and headers: the body is file content and form
      // fields, which reach Wren already parsed (Request.post / Request.file).
      wrenSetSlotBytes(vm, 1, hm->message.str, mp.head_len);
    } else {
      wrenSetSlotString(vm, 1, hm->message.str);
    }
    wrenSetSlotString(vm, 2, hm->routes.str);
    wrenSetSlotString(vm, 3, filesIds);
    wrenSetSlotString(vm, 4, mp.fields ? mp.fields : "");
    free(mp.fields);

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
      /* A blob selected by Response.file(id) replaces the body. The id is read
       * from Response's own field, out of band: it used to be a 0x1A marker at
       * the start of the body, which any client-influenced Response.out output
       * could forge to read an arbitrary BIALET_FILES row. */
      int         fileApplied = 0;
      WrenHandle* fileIdMethod = wrenMakeCallHandle(vm, "fileId");
      wrenEnsureSlots(vm, 1);
      wrenSetSlotHandle(vm, 0, responseClass);
      if(wrenCall(vm, fileIdMethod) == WREN_RESULT_SUCCESS &&
         wrenGetSlotType(vm, 0) == WREN_TYPE_NUM) {
        sqlite3_stmt* stmt;
        int           result = sqlite3_prepare_v2(
            db, "SELECT file FROM BIALET_FILES WHERE id = ?", -1, &stmt, 0);
        if(!(error = result != SQLITE_OK)) {
          sqlite3_bind_int64(stmt, 1, (sqlite3_int64)wrenGetSlotDouble(vm, 0));
          if(sqlite3_step(stmt) == SQLITE_ROW) {
            const void* blob = sqlite3_column_blob(stmt, 0);
            size_t      len = (size_t)sqlite3_column_bytes(stmt, 0);
            r.body = safe_malloc(len + 1);
            /* A zero-length BLOB makes sqlite3_column_blob() return NULL, and an
             * owned body must be NUL-terminated at r.length: write_response,
             * handle_client, custom_error and livereload_inject_response all fall
             * back to strlen() when length == 0. */
            if(len > 0 && blob != NULL)
              memcpy(r.body, blob, len);
            r.body[len] = '\0';
            r.length = len;
            r.body_owned = 1;
            fileApplied = 1;
          } else {
            // If the id is not found, we will send an internal server error.
            message(red("Error file not found"), sqlite3_errmsg(db));
            error = 1;
          }
          sqlite3_finalize(stmt);
        }
      }
      wrenReleaseHandle(vm, fileIdMethod);

      if(!fileApplied && !error) {
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
          r.body = string_safe_copy(wrenGetSlotString(vm, 0));
          r.body_owned = 1;
        }
        wrenReleaseHandle(vm, outMethod);
      }
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
  // A relative -d is joined onto root_dir, so the database can land inside the
  // served tree, where the static handler's only name-based protection is the
  // '_'/'.' rule. Resolve the real location and refuse to start when the
  // database (or its -wal/-shm siblings, which share the base name) would be
  // downloadable. The default _db.sqlite3 passes; -d data.sqlite3 does not.
  {
    char real_db[PATH_MAX];
    int  have_real = realpath_n(db_path, real_db, sizeof(real_db)) != NULL;
    if(!have_real) {
      // The file does not exist yet: resolve the parent directory and re-join
      // the base name, so a relative path is still checked at its real place.
      char        dir[PATH_MAX];
      char*       slash = strrchr(db_path, '/');
      const char* base = slash ? slash + 1 : db_path;
      int         ok = 0;
      if(slash == NULL) {
        ok = realpath_n(".", real_db, sizeof(real_db)) != NULL;
      } else {
        size_t dlen = (size_t)(slash - db_path);
        if(dlen == 0)
          dlen = 1; // "/name"
        if(dlen < sizeof(dir)) {
          memcpy(dir, db_path, dlen);
          dir[dlen] = '\0';
          ok = realpath_n(dir, real_db, sizeof(real_db)) != NULL;
        }
      }
      if(ok) {
        size_t rlen = strlen(real_db);
        size_t blen = strlen(base);
        if(rlen > 0 && real_db[rlen - 1] == '/')
          rlen--;
        if(rlen + 1 + blen < sizeof(real_db)) {
          real_db[rlen] = '/';
          memcpy(real_db + rlen + 1, base, blen + 1);
          have_real = 1;
        }
      }
    }
    if(have_real) {
      size_t root_len = strlen(bialet_config.full_root_dir);
      if(strncmp(real_db, bialet_config.full_root_dir, root_len) == 0 &&
         real_db[root_len] == '/' &&
         !has_forbidden_uri_component(real_db + root_len)) {
        message(red("Error"),
                "Database inside the app root must start with '_' or '.' "
                "(like _db.sqlite3), or live outside the app root:",
                db_path);
        exit(BIALET_SQLITE_ERROR);
      }
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
  purge_temp_uploads();

  wrenInitConfiguration(&wren_config);
  wren_config.writeFn = &bialet_wren_write;
  wren_config.errorFn = &bialet_wren_error;
  wren_config.queryFn = &query_execute;
  wren_config.resolveModuleFn = &bialet_wren_resolve_module;
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
  query->resultsCapacity = 0;
  query->parameters = NULL;
  query->parametersCount = 0;
  query->parametersCapacity = 0;
  query->queryString = NULL;
  query->lastInsertId = NULL;
  return query;
}

// Grows a dynamically sized array of [elem_size] elements so that at least
// [needed] fit, doubling the previous capacity (starting at 8). Returns the new
// base pointer, or NULL on allocation failure with [*data] left untouched.
//
// Collecting results used to realloc one element at a time, so a 2000-row
// SELECT performed 2000 reallocs each copying the whole array: quadratic in
// rows, and the dominant cost of any non-trivial query.
static void* grow_array(void* data, int* capacity, int needed, size_t elem_size) {
  if(needed <= *capacity)
    return data;
  int new_capacity = *capacity > 0 ? *capacity : 8;
  while(new_capacity < needed) {
    if(new_capacity > INT_MAX / 2)
      return NULL;
    new_capacity *= 2;
  }
  if((size_t)new_capacity > SIZE_MAX / elem_size)
    return NULL;
  void* grown = realloc(data, (size_t)new_capacity * elem_size);
  if(grown == NULL)
    return NULL;
  *capacity = new_capacity;
  return grown;
}

int add_result(BialetQuery* query) {
  int                count = query->resultsCount + 1;
  BialetQueryResult* new_results = (BialetQueryResult*)grow_array(
      query->results, &query->resultsCapacity, count, sizeof(BialetQueryResult));
  if(new_results == NULL)
    return -1;
  query->results = new_results;
  query->resultsCount = count;
  BialetQueryResult* newResult = &query->results[count - 1];
  newResult->rows = NULL;
  newResult->rowCount = 0;
  newResult->rowCapacity = 0;
  return 0;
}

int add_result_row(BialetQuery* query, int resultIndex, const char* name,
                   const char* value, int size, BialetQueryType type) {
  if(resultIndex < 0 || resultIndex >= query->resultsCount)
    return -1;

  BialetQueryResult* result = &query->results[resultIndex];
  int                count = result->rowCount + 1;
  BialetQueryRow*    new_rows = (BialetQueryRow*)grow_array(
      result->rows, &result->rowCapacity, count, sizeof(BialetQueryRow));
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
  BialetQueryParameter* new_parameters = (BialetQueryParameter*)grow_array(
      query->parameters, &query->parametersCapacity, count,
      sizeof(BialetQueryParameter));
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
