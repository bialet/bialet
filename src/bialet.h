/*
 * This file is part of Bialet, which is licensed under the
 * MIT License.
 *
 * Copyright (c) 2023-2025 Rodrigo Arce
 *
 * SPDX-License-Identifier: MIT
 *
 * For full license text, see LICENSE.md.
 */
#ifndef BIALET_CONFIG_H
#define BIALET_CONFIG_H

#ifdef _WIN32
#define IS_WIN 1
#define IS_UNIX 0
#define IS_MAC 0
#define IS_LINUX 0
#else
#define IS_UNIX 1
#define IS_WIN 0
#if __APPLE__
#define IS_MAC 1
#define IS_LINUX 0
#else
#define IS_LINUX 1
#define IS_MAC 0
#endif
#endif

#ifdef _WIN64

#include <winsock2.h>

#include <windows.h>

#include <stdio.h>

#else

#include <stdio.h>

#endif

#define MAX_NUMBER_LENGTH 100

struct BialetConfig {
  char* root_dir;
  char* host;
  int   port;

  FILE* log_file;
  int   debug;
  int   output_color;

  int mem_soft_limit, mem_hard_limit, cpu_soft_limit, cpu_hard_limit;

  char* db_path;
  char* ignored_files;
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

BialetQuery* createBialetQuery();
void         addResultRow(BialetQuery* query, int resultIndex, const char* name,
                          const char* value, int size, BialetQueryType type);
void         addResult(BialetQuery* query);
void addParameter(BialetQuery* query, const char* value, BialetQueryType type);
void freeBialetQuery(BialetQuery* query);

#define BIALET_USAGE                                                                \
  "🚲 bialet\n\nUsage: %s [-h host] [-p port] [-l log] [-d database] "              \
  "root_dir\n"

/* Welcome, not found and error pages */
#define BIALET_HEADERS "Content-Type: text/html; charset=UTF-8\r\n"
#define BIALET_HEADER_PAGE                                                          \
  "<!DOCTYPE html><body style=\"font:2.3rem "                                       \
  "system-ui;text-align:center;margin:2em;color:#024\"><h1>"
#define BIALET_FOOTER_PAGE                                                          \
  "</p><p style=\"font-size:.8em;margin-top:2em\">Powered by 🚲 <b><a "             \
  "href=\"https://bialet.dev\" style=\"color:#007FAD\" >Bialet"
#define BIALET_WELCOME_PAGE                                                         \
  BIALET_HEADER_PAGE                                                                \
  "👋 Welcome to Bialet</h1><p>You're in! What's next?<p>Check out our <b><a "      \
  "href=\"https://bialet.dev/getting-started.html\" "                               \
  "style=\"color:#007FAD\">Getting Started "                                        \
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

#endif
