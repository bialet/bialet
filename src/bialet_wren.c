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
#include "bialet_wren.h"

#include "bialet.h"
#include "http_call.h"
#include "messages.h"
#include "server.h"
#include "utils.h"
#include "wren.h"
#include <ctype.h>
#include <sqlite3.h>
#include <string.h>
#include <time.h>

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
#define BIALET_REMOTE_MODULE_GITHUB_URL                                             \
  "https://raw.githubusercontent.com/%s/%s/refs/heads/%s/%s" BIALET_EXTENSION
#define BIALET_REMOTE_MODULE_DEFAULT_BRANCH "main"

#define MAIN_MODULE_NAME "main"
#define CLI_MODULE_NAME "bialet_cli"

WrenConfiguration          wren_config;
static struct BialetConfig bialet_config;
sqlite3*                   db;

static void bialetWrenWrite(WrenVM* vm, const char* message) {
  message(yellow("Log"), message);
  sqlite3_stmt* stmt;
  sqlite3_prepare_v2(db, "INSERT INTO BIALET_LOGS (message) VALUES (?)", -1, &stmt,
                     0);
  sqlite3_bind_text(stmt, 1, message, -1, SQLITE_STATIC);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

char* bialetReadFile(const char* path) {
  char fullPath[MAX_URL_LEN];
  strcpy(fullPath, bialet_config.full_root_dir);
  strcat(fullPath, "/");
  strcat(fullPath, path);
  return readFile(fullPath);
}

char* readFile(const char* path) {
  char* buffer = 0;
  long  length;
  FILE* f = fopen(path, "rb");
  if(f) {
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    fseek(f, 0, SEEK_SET);
    buffer = malloc(length + 1);
    if(buffer) {
      fread(buffer, 1, length, f);
      buffer[length] = '\0';
    }
    fclose(f);
  }
  return buffer;
}

static WrenLoadModuleResult bialetWrenLoadModule(WrenVM* vm, const char* name) {

  char                 module[MAX_URL_LEN];
  char*                lastSlash;
  WrenLoadModuleResult result = {0};

  if(strchr(name, ':') != NULL) {
    char url[MAX_URL_LEN];
    // If name start with https:// or http://
    if(strncmp(name, "http://", 7) == 0 || strncmp(name, "https://", 8) == 0) {
      snprintf(url, MAX_URL_LEN, "%s", name);
    } else if(strncmp(name, "gh:", 3) == 0) {
      name += 3; // Remove gh:
      char name_copy[MAX_URL_LEN];
      strncpy(name_copy, name, sizeof(name_copy));
      name_copy[sizeof(name_copy) - 1] = '\0';
      char* at = strchr(name_copy, '@');
      char* branch = BIALET_REMOTE_MODULE_DEFAULT_BRANCH;
      if(at != NULL) {
        *at = '\0'; // Remove @
        branch = at + 1;
      }
      char* user = strtok(name_copy, "/");
      char* repo = strtok(NULL, "/");
      char* path = strtok(NULL, "");
      if(!user || !repo || !path) {
        message(red("Error"), "Invalid GitHub URL.");
        return result;
      }
      sprintf(url, BIALET_REMOTE_MODULE_GITHUB_URL, user, repo, branch, path);
      name -= 3; // Restore gh:
    } else {
      message(red("Error"), "Import type not supported.");
      return result;
    }
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(
        db, "SELECT content FROM BIALET_REMOTE_MODULES WHERE module = ? LIMIT 1", -1,
        &stmt, 0);
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    const char* content = 0;
    if(sqlite3_step(stmt) == SQLITE_ROW) {
      content = (char*)sqlite3_column_text(stmt, 0);
      result.source = string_safe_copy(content);
      sqlite3_finalize(stmt);
    } else {
      // File not found in cache, try to get from URL
      sqlite3_finalize(stmt);
      struct HttpRequest  req;
      struct HttpResponse resp;
      req.method = string_safe_copy("GET");
      req.basicAuth = string_safe_copy("");
      req.raw_headers = string_safe_copy("");
      req.postData = string_safe_copy("");
      req.url = string_safe_copy(url);
      httpCallPerform(&req, &resp);
      // @TODO Fix this when status are added in httpCallPerform
      if(resp.status >= 0) {
        // File found, save it in cache
        result.source = resp.body;
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(
            db, "INSERT INTO BIALET_REMOTE_MODULES (module, content) VALUES (?, ?)",
            -1, &stmt, 0);
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, resp.body, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        message(yellow("Remote module saved"), name);
      } else {
        message(red("Error"), "Module not found in GitHub.");
      }
    }
    return result;
  }

  if(name[0] == '/') {
    if(strlen(name) + strlen(bialet_config.full_root_dir) + BIALET_EXTENSION_LEN +
           1 >
       MAX_URL_LEN) {
      message(red("Error"), "Module name too long.");
      return result;
    }
    snprintf(module, MAX_URL_LEN, "%s", bialet_config.full_root_dir);
  } else {
    char* calledFrom = string_safe_copy(wrenGetUserData(vm));
    lastSlash = strrchr(calledFrom, '/');
    if(strlen(name) + strlen(calledFrom) + BIALET_EXTENSION_LEN + 2 >
       MAX_MODULE_LEN) {
      message(red("Error"), "Module name too long.");
      return result;
    }
    if(lastSlash)
      *lastSlash = '\0';
    snprintf(module, MAX_MODULE_LEN, "%s/", calledFrom);
  }

  strncat(module, name, MAX_MODULE_LEN - strlen(module) - 1);
  size_t name_len = strlen(module);
  if(name_len < BIALET_EXTENSION_LEN ||
     strcmp(module + name_len - BIALET_EXTENSION_LEN, BIALET_EXTENSION) != 0) {
    strncat(module, BIALET_EXTENSION, MAX_MODULE_LEN - strlen(module) - 1);
  }

  char resolved[MAX_URL_LEN];
  if(realpath(module, resolved) == NULL) {
    return result;
  }

  size_t root_len = strlen(bialet_config.full_root_dir);
  if(strncmp(resolved, bialet_config.full_root_dir, root_len) != 0 ||
     (resolved[root_len] != '/' && resolved[root_len] != '\0')) {
    return result;
  }

  char* buffer = readFile(module);
  result.source = NULL;

  if(buffer) {
    result.source = buffer;
  }
  return result;
}

void bialetWrenError(WrenVM* vm, WrenErrorType errorType, const char* module,
                     const int line, const char* msg) {
  char lineMessage[MAX_LINE_ERROR_LEN];
  sprintf(lineMessage, "%s line %d", module, line);
  switch(errorType) {
    case WREN_ERROR_COMPILE: {
      message(red("Compilation Error"), lineMessage, (char*)msg);
    } break;
    case WREN_ERROR_STACK_TRACE: {
      message(red("Stack Error"), lineMessage, (char*)msg);
    } break;
    case WREN_ERROR_RUNTIME: {
      message(red("Runtime Error"), (char*)msg);
    } break;
  }
}

static char* sqliteIntToString(sqlite3_int64 value) {
  char* str = (char*)malloc(21 * sizeof(char));
  if(str == NULL)
    return NULL;
  sprintf(str, "%lld", value);
  return str;
}

static void queryExecute(WrenVM* vm, BialetQuery* query) {
  sqlite3_stmt* stmt;
  char*         columns[MAX_COLUMNS];
  const char*   value;
  int           colType, colCount = 0, type, rowCount = 0, bindCounter = 0, size = 0;

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
        break;
    }
  }

  /* Execute statement and fetch results */
  while((result = sqlite3_step(stmt)) == SQLITE_ROW) {
    if(!colCount) {
      /* Get column names */
      colCount = sqlite3_column_count(stmt);
      for(int i = 0; i < colCount; i++) {
        columns[i] = string_safe_copy(sqlite3_column_name(stmt, i));
      }
    }

    addResult(query);
    for(int i = 0; i < colCount; i++) {
      colType = sqlite3_column_type(stmt, i);
      switch(colType) {
        case SQLITE_INTEGER:
        case SQLITE_FLOAT:
          type = BIALETQUERYTYPE_NUMBER;
          value = (const char*)sqlite3_column_text(stmt, i);
          size = strlen(value);
          break;
        case SQLITE_TEXT:
          type = BIALETQUERYTYPE_STRING;
          value = (const char*)sqlite3_column_text(stmt, i);
          size = strlen(value);
          break;
        case SQLITE_BLOB:
          type = BIALETQUERYTYPE_BLOB;
          // @FIXME This retrieving is working, but it is not passed correctly to
          // Wren
          value = sqlite3_column_blob(stmt, i);
          size = sqlite3_column_bytes(stmt, i);
          break;
        case SQLITE_NULL:
          type = BIALETQUERYTYPE_NULL;
          value = NULL;
          size = 1;
          break;
        default:
          message(red("Query Error"), "Uknown type on binding result");
          break;
      }
      addResultRow(query, rowCount, columns[i], value, size, type);
    }
    rowCount++;
  }

  /* @TODO @FIXME Getting SQLITE_MISUSED */
  /* If SQLite ever returns SQLITE_MISUSE from any interface, that means that
   * the application */
  /* is incorrectly coded and needs to be fixed. Do not ship an application that
   * sometimes returns */
  /* SQLITE_MISUSE from a standard SQLite interface because that application
   * contains potentially */
  /* serious bugs. */
  if(result != SQLITE_DONE && result != SQLITE_OK && result != SQLITE_EMPTY) {
    message(red("SQL Error"), sqlite3_errmsg(db));
  }
  query->lastInsertId = sqliteIntToString(sqlite3_last_insert_rowid(db));
  sqlite3_finalize(stmt);
}

char* escapeSpecialChars(const char* input) {
  int   i, j = 0, len = strlen(input);
  char* output = malloc(len * 2 + 1); // Worst case: all characters need escaping
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

int saveUploadedFiles(struct HttpMessage* hm, char* filesIds) {
  // TODO: Save uploaded files, implement again with new server
  return 1;
}

struct BialetResponse bialetRun(char* module, char* code, struct HttpMessage* hm) {
  struct BialetResponse r;
  r.status = HTTP_OK;
  r.header = NULL;
  r.body = NULL;
  r.length = 0;
  int     error = 0;
  WrenVM* vm = 0;

  vm = wrenNewVM(&wren_config);
  wrenSetUserData(vm, module);
  wrenInterpret(vm, MAIN_MODULE_NAME, "Response.init\nDate.init");
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
    // @TODO @FIXME This is called always and it can be abuse to fill the disk
    saveUploadedFiles(hm, filesIds);
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
    wrenEnsureSlots(vm, 1);
    int type = wrenGetSlotType(vm, 0);
    if(type == WREN_TYPE_STRING) {
      const char* returnBody = wrenGetSlotString(vm, 0);
      r.body = string_safe_copy(returnBody);
    }

    wrenGetVariable(vm, module, "Response", 0);
    WrenHandle* responseClass = wrenGetSlotHandle(vm, 0);
    if(r.body == NULL || strlen(r.body) == 0) {
      /* Get body from response */
      WrenHandle* outMethod = wrenMakeCallHandle(vm, "out");
      wrenSetSlotHandle(vm, 0, responseClass);
      if((error = wrenCall(vm, outMethod) != WREN_RESULT_SUCCESS)) {
        message(red("Runtime Error"), "Failed to get body");
      } else {
        const char* body = wrenGetSlotString(vm, 0);
        if(body[0] != BIALET_FILE_CHAR) {
          r.body = string_safe_copy(body);
        } else {
          // @TODO Move this to the server
          // Use the BIALET_FILE_CHAR to request the file instead of send the
          // actual body. It will search the file in the database.
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
      } else {
        const char* headersString = wrenGetSlotString(vm, 0);
        r.header = string_safe_copy(headersString);
      }
      wrenReleaseHandle(vm, headersMethod);
    }
    /* Clean Wren vm */
    wrenReleaseHandle(vm, responseClass);
  }
  wrenFreeVM(vm);

  if(error) {
    custom_error(HTTP_ERROR, &r);
  }

  if(!hm)
    r.header = NULL;

  return r;
}

int bialetRunCli(char* code) {
  struct BialetResponse response = bialetRun(CLI_MODULE_NAME, code, NULL);
  if(response.status == HTTP_ERROR)
    return 1;
  printf("%s", response.body);
  return 0;
}

void bialetInit(struct BialetConfig* config) {
  char db_path[MAX_MODULE_LEN];
  int  lastChar = (int)strlen(config->db_path) - 1;
  if(config->db_path[0] == '/') {
    strcpy(db_path, config->db_path);
  } else {
    if(config->db_path[lastChar] == '/') {
      config->db_path[lastChar] = '\0';
    }
    strcpy(db_path, config->root_dir);
    strcat(db_path, "/");
    strcat(db_path, config->db_path);
  }
  if(sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                     NULL) != SQLITE_OK) {
    message(red("SQL Error"), "Can't open database in", config->db_path);
    exit(BIALET_SQLITE_ERROR);
  }
  // Set Pragmas
  // @TODO Adjust the pragmas to the user configuration
  sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
  sqlite3_exec(db, "PRAGMA synchronous = NORMAL;", NULL, NULL, NULL);
  if(config->wal_mode) {
    sqlite3_exec(db, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL);
  }
  sqlite3_exec(db, "PRAGMA journal_size_limit = " BIALET_SQLITE_JOURNAL_SIZE ";",
               NULL, NULL, NULL);
  sqlite3_exec(db, "PRAGMA mmap_size = " BIALET_SQLITE_MMAP_SIZE ";", NULL, NULL,
               NULL);
  sqlite3_exec(db, "PRAGMA cache_size = " BIALET_SQLITE_CACHE_SIZE ";", NULL, NULL,
               NULL);
  sqlite3_busy_timeout(db, BIALET_SQLITE_BUSY_TIMEOUT);

  bialet_config = *config;
  wrenInitConfiguration(&wren_config);
  wren_config.writeFn = &bialetWrenWrite;
  wren_config.errorFn = &bialetWrenError;
  wren_config.queryFn = &queryExecute;
  wren_config.loadModuleFn = &bialetWrenLoadModule;

  httpCallInit(&bialet_config);
}

BialetQuery* createBialetQuery() {
  BialetQuery* query = (BialetQuery*)malloc(sizeof(BialetQuery));
  query->results = NULL;
  query->resultsCount = 0;
  query->parameters = NULL;
  query->parametersCount = 0;
  return query;
}

void addResult(BialetQuery* query) {
  query->resultsCount++;
  query->results = (BialetQueryResult*)realloc(
      query->results, query->resultsCount * sizeof(BialetQueryResult));
  BialetQueryResult* newResult = &query->results[query->resultsCount - 1];
  newResult->rows = NULL;
  newResult->rowCount = 0;
}

void addResultRow(BialetQuery* query, int resultIndex, const char* name,
                  const char* value, int size, BialetQueryType type) {
  if(resultIndex < 0 || resultIndex >= query->resultsCount)
    return;

  BialetQueryResult* result = &query->results[resultIndex];
  result->rowCount++;
  result->rows = (BialetQueryRow*)realloc(result->rows,
                                          result->rowCount * sizeof(BialetQueryRow));
  BialetQueryRow* newRow = &result->rows[result->rowCount - 1];
  newRow->name = name != NULL ? string_safe_copy(name) : "";
  if(value != NULL && size > 0) {
    newRow->value = safe_malloc(size);
    memcpy(newRow->value, value, size);
  } else {
    newRow->value = "";
  }
  newRow->size = size;
  newRow->type = type;
}

void addParameter(BialetQuery* query, const char* value, BialetQueryType type) {
  query->parametersCount++;
  query->parameters = (BialetQueryParameter*)realloc(
      query->parameters, query->parametersCount * sizeof(BialetQueryParameter));
  BialetQueryParameter* newParameter =
      &query->parameters[query->parametersCount - 1];
  newParameter->value = value != NULL ? strdup(value) : NULL;
  newParameter->type = type;
}

void freeBialetQuery(BialetQuery* query) {
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
