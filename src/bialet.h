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
#ifndef BIALET_CONFIG_H
#define BIALET_CONFIG_H

#define BIALET_VERSION "1.0.0"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define IS_MAC 0
#define IS_LINUX 0
// Bialet logo is a bycicle however there is no emoji support on Windows terminal.
// We will use a dash instead, empty logo looks bad as well.
#define BIALET_LOGO "-"
#else
#define BIALET_LOGO "🚲"
#if __APPLE__
#define IS_MAC 1
#define IS_LINUX 0
#else
#define IS_LINUX 1
#define IS_MAC 0
#endif
#endif

#ifdef _WIN32
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#ifndef WINVER
#define WINVER 0x0601
#endif

#include <winsock2.h>

#include <windows.h>

#include <wchar.h>
#include <winbase.h>
#ifndef PATH_MAX
#define PATH_MAX _MAX_PATH
#endif
#endif

char* realpath_n(const char* path, char* resolved, size_t resolved_size);

/* Marks a printf-style function so the compiler checks its callers' format
 * strings and silences -Wformat-nonliteral inside the wrapper itself. GCC,
 * Clang and MinGW all support it; anything else gets no checking. */
#if defined(__GNUC__) || defined(__clang__)
#define BIALET_PRINTF_FORMAT(fmt_idx, args_idx)                                     \
  __attribute__((format(printf, fmt_idx, args_idx)))
#else
#define BIALET_PRINTF_FORMAT(fmt_idx, args_idx)
#endif
#define MAX_NUMBER_LENGTH 100
#define BIALET_EXTENSION ".wren"
#define BIALET_EXTENSION_LEN 5
#define BIALET_DEFAULT_PORT 7001
#define BIALET_DEFAULT_HOST "127.0.0.1"

struct BialetConfig {
  char* root_dir;
  char* full_root_dir;
  char* host;
  int   port;

  FILE* log_file;
  int   debug;
  int   quiet;
  int   output_color;

  int mem_soft_limit, mem_hard_limit, cpu_soft_limit, cpu_hard_limit;

  char* db_path;
  char* ignored_files;
  int   wal_mode;

  /* Max upload size in bytes (default 10MB) */
  size_t max_upload_size;

  /* Max request body size in bytes. Configured with -b (default 128KB) and
   * clamped at startup to a memory-safe ceiling (soft limit / 512) so worst-
   * case body parsing (one Wren string per line) stays inside RLIMIT_AS. */
  size_t max_post_size;

  /* SQLite pragma settings */
  int sqlite_foreign_keys; /* Default: 1 (ON) */
  int sqlite_synchronous;  /* 0=OFF, 1=NORMAL, 2=FULL, 3=EXTRA; Default: 1 */

  /* Set to true when running tests with -T flag */
  int enable_tests;
};

struct BialetResponse {
  int    status;
  char*  header;
  char*  body;
  size_t length;
  /* Whether header/body are heap-allocated and owned by this struct (vs
   * static strings or buffers owned elsewhere, e.g. file_content). */
  int body_owned;
  int header_owned;
};

typedef enum {
  BIALETQUERYTYPE_NULL,
  BIALETQUERYTYPE_NUMBER,
  BIALETQUERYTYPE_STRING,
  BIALETQUERYTYPE_BLOB,
  BIALETQUERYTYPE_BOOLEAN
} BialetQueryType;

typedef struct {
  char*           name;
  char*           value;
  int             size;
  BialetQueryType type;
} BialetQueryRow;

typedef struct {
  BialetQueryRow* rows;
  int             rowCount;
} BialetQueryResult;

typedef struct {
  char*           value;
  BialetQueryType type;
} BialetQueryParameter;

typedef struct {
  BialetQueryResult*    results;
  int                   resultsCount;
  BialetQueryParameter* parameters;
  int                   parametersCount;
  char*                 queryString;
  char*                 lastInsertId;
} BialetQuery;

BialetQuery* create_bialet_query();
/* Returns 0 on success, -1 when the result/row/parameter could not be grown
 * (allocation failure). Callers must not use the failed slot and should abort
 * the query rather than dereference the un-grown array. */
int  add_result_row(BialetQuery* query, int resultIndex, const char* name,
                    const char* value, int size, BialetQueryType type);
int  add_result(BialetQuery* query);
int  add_parameter(BialetQuery* query, const char* value, BialetQueryType type);
void free_bialet_query(BialetQuery* query);

/* Welcome, not found and error pages */
#define BIALET_HEADERS "Content-Type: text/html; charset=UTF-8\r\n"
/* Shared page chrome. Bialet's brand palette (BRAND.md), hardcoded so the
 * default pages carry no CSS variables or external assets. Wren's
 * Response.defaultPage_ builds the same chrome from these macros, so the
 * C-side fallbacks and the Wren API render the exact same template. */
#define BIALET_CSS_PAGE                                                             \
  "<style>body{background:#fff;color:#024;font-family:system-ui;font-size:clamp(1." \
  "8rem, 2.5vw, 2rem);"                                                             \
  "line-height:2em;text-align:center;padding:.5em;max-width:45ch;margin:auto}"      \
  "a{color:#06f;text-decoration:none}"                                              \
  "a:hover{color:#04f;text-decoration:underline}"                                   \
  "a:visited{color:#06f}"                                                           \
  "code{color:#b00;font-family:inherit}"                                            \
  "@media (prefers-color-scheme:dark){body{background:#024;color:#fff}"             \
  "a{color:#0bf}a:hover{color:#0ff;text-decoration:underline}a:visited{color:#0bf}" \
  "code{color:#fa0}}</style>"
#define BIALET_HEADER_PAGE "<!DOCTYPE html>" BIALET_CSS_PAGE "<h1>"
#define BIALET_FOOTER_PAGE                                                          \
  "</p><p "                                                                         \
  "style=\"font-size:.8em;position:fixed;bottom:0;left:0;width:100%;text-align:"    \
  "center\">Powered by 🚲 <b><a "                                                   \
  "href=\"https://bialet.dev\">Bialet</a></b></p></body></html>"
#define BIALET_WELCOME_PAGE                                                         \
  BIALET_HEADER_PAGE                                                                \
  "👋 Welcome to Bialet</h1><p>You're in! What's next?</p><p>Check out our "        \
  "<b><a href=\"https://bialet.dev/getting-started.html\">Getting Started "         \
  "guide</a></b>." BIALET_FOOTER_PAGE
#define BIALET_NOT_FOUND_PAGE                                                       \
  BIALET_HEADER_PAGE                                                                \
  "⚠️ Not found</h1><p>Uh-oh! No route found." BIALET_FOOTER_PAGE
#define BIALET_ERROR_PAGE                                                           \
  BIALET_HEADER_PAGE                                                                \
  "🚨 Internal Server Error</h1><p>Oops! Something broke." BIALET_FOOTER_PAGE
#define BIALET_FORBIDDEN_PAGE                                                       \
  BIALET_HEADER_PAGE                                                                \
  "🚫 Forbidden</h1><p>Sorry, you don't have permission to "                        \
  "access this page." BIALET_FOOTER_PAGE
#define BIALET_PAYLOAD_TOO_LARGE_PAGE                                               \
  BIALET_HEADER_PAGE                                                                \
  "📦 Payload Too Large</h1><p>The request body is too large." BIALET_FOOTER_PAGE
#define BIALET_TOO_MANY_REQUESTS_PAGE                                               \
  BIALET_HEADER_PAGE                                                                \
  "⏳ Too Many Requests</h1><p>Please slow down and try again "                     \
  "later." BIALET_FOOTER_PAGE

#endif
