/*
 * This file is part of Bialet, which is licensed under the
 * GNU General Public License, version 2 (GPL-2.0).
 *
 * Copyright (c) 2023 Rodrigo Arce
 *
 * SPDX-License-Identifier: GPL-2.0-only
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
#include "wren_vm.h"
#include <ctype.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <sqlite3.h>
#include <string.h>
#include <time.h>

// Wren generated code
#include "bialet.wren.inc"

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
#define BIALET_EXTERNAL_IMPORT_URL                                                  \
  "https://raw.githubusercontent.com/%s/refs/heads/%s/export.wren"
#define BIALET_EXTERNAL_DEFAULT_BRANCH "main"

#define MAIN_MODULE_NAME "bialet"
#define CLI_MODULE_NAME "bialet_cli"

WrenConfiguration          wren_config;
static struct BialetConfig bialet_config;
sqlite3*                   db;

static void bialetWrenWrite(WrenVM* vm, const char* text) {
  message(yellow("Log"), text);
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

  char                 module[MAX_MODULE_LEN];
  char*                lastSlash;
  WrenLoadModuleResult result = {0};

  if(name[0] == '/') {
    if(strlen(name) + strlen(bialet_config.root_dir) + strlen(BIALET_EXTENSION) + 1 >
       MAX_MODULE_LEN) {
      message(red("Error"), "Module name too long.");
      return result;
    }
    snprintf(module, MAX_MODULE_LEN, "%s", bialet_config.root_dir);
  } else {
    /* @TODO Security: prevent load modules from parent directories */
    /* @TODO Create an object for the user data */
    char* user_data = wrenGetUserData(vm);
    char* called_module = string_safe_copy(user_data);
    lastSlash = strrchr(called_module, '/');
    if(strlen(name) + strlen(called_module) + strlen(BIALET_EXTENSION) + 2 >
       MAX_MODULE_LEN) {
      message(red("Error"), "Module name too long.");
      return result;
    }
    if(lastSlash)
      *lastSlash = '\0';
    snprintf(module, MAX_MODULE_LEN, "%s/", called_module);
  }

  strncat(module, name, MAX_MODULE_LEN - strlen(module) - 1);
  strncat(module, BIALET_EXTENSION, MAX_MODULE_LEN - strlen(module) - 1);

  char* buffer = readFile(module);
  result.source = NULL;

  if(buffer) {
    result.source = buffer;
  } else if(strchr(name, '/') != NULL) {
    // @TODO Refactor the external import, the code is too large
    // File not exists, try to get from cache
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(db,
                       "SELECT content FROM BIALET_IMPORT WHERE module = ? LIMIT 1",
                       -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    const char* content = 0;
    if(sqlite3_step(stmt) == SQLITE_ROW) {
      content = (char*)sqlite3_column_text(stmt, 0);
      result.source = string_safe_copy(content);
      sqlite3_finalize(stmt);
    } else {
      // File not found in cache, try to get from GitHub
      sqlite3_finalize(stmt);
      struct HttpRequest  req;
      struct HttpResponse resp;
      req.method = string_safe_copy("GET");
      req.basicAuth = string_safe_copy("");
      req.raw_headers = string_safe_copy("");
      req.postData = string_safe_copy("");
      // Use repository@branch, if exists
      char* branch = strchr(name, '@');
      if(branch == NULL)
        branch = BIALET_EXTERNAL_DEFAULT_BRANCH;
      else
        branch++;
      char url[MAX_URL_LEN];
      sprintf(url, BIALET_EXTERNAL_IMPORT_URL, strtok((char*)name, "@"), branch);
      req.url = string_safe_copy(url);
      httpCallPerform(&req, &resp);
      // @TODO Fix this when status are added in httpCallPerform
      if(resp.status >= 0) {
        // File found, save it in cache
        result.source = resp.body;
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(
            db, "INSERT INTO BIALET_IMPORT (module, content) VALUES (?, ?)", -1,
            &stmt, 0);
        sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, resp.body, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        message(yellow("External import saved"), name);
      } else {
        message(red("Error"), "Module not found in GitHub.");
      }
    }
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

static void randomString(WrenVM* vm) {
  const int len = wrenGetSlotDouble(vm, 1);
  char      random_str[len + 1];
  char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  srand(time(0));
  for(int i = 0; i < len; i++) {
    int random_index = rand() % (sizeof(charset) - 1);
    random_str[i] = charset[random_index];
  }

  random_str[len] = '\0';
  wrenSetSlotString(vm, 0, random_str);
}

static void hashPassword(WrenVM* vm) {
  const char*   password = wrenGetSlotString(vm, 1);
  unsigned char salt[16];
  if(!RAND_bytes(salt, sizeof(salt))) {
    perror("Failed to generate salt");
    return;
  }

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int  hash_len;
  EVP_MD_CTX*   ctx = EVP_MD_CTX_new();
  const EVP_MD* md = EVP_sha256();

  EVP_DigestInit_ex(ctx, md, NULL);
  EVP_DigestUpdate(ctx, password, strlen(password));
  EVP_DigestUpdate(ctx, salt, sizeof(salt));
  EVP_DigestFinal_ex(ctx, hash, &hash_len);

  EVP_MD_CTX_free(ctx);

  static char result[128];
  for(unsigned int i = 0; i < hash_len; i++) {
    sprintf(result + (i * 2), "%02x", hash[i]);
  }

  strcat(result, "/");
  for(int i = 0; i < sizeof(salt); i++) {
    sprintf(result + strlen(result), "%02x", salt[i]);
  }

  wrenEnsureSlots(vm, 2);
  wrenSetSlotString(vm, 0, result);
}

static void verifyPassword(WrenVM* vm) {
  int         result = 0;
  const char* password = wrenGetSlotString(vm, 1);
  const char* hash_and_salt = wrenGetSlotString(vm, 2);

  char stored_hash[65], stored_salt[33];
  strncpy(stored_hash, hash_and_salt, 64);
  stored_hash[64] = 0;
  strcpy(stored_salt, hash_and_salt + 65);

  unsigned char salt[16];
  for(int i = 0; i < 16; i++) {
    sscanf(stored_salt + i * 2, "%2hhx", &salt[i]);
  }

  unsigned char new_hash[EVP_MAX_MD_SIZE];
  unsigned int  new_hash_len;
  EVP_MD_CTX*   ctx = EVP_MD_CTX_new();
  const EVP_MD* md = EVP_sha256();

  EVP_DigestInit_ex(ctx, md, NULL);
  EVP_DigestUpdate(ctx, password, strlen(password));
  EVP_DigestUpdate(ctx, salt, sizeof(salt));
  EVP_DigestFinal_ex(ctx, new_hash, &new_hash_len);

  EVP_MD_CTX_free(ctx);

  char new_hash_str[65];
  for(unsigned int i = 0; i < new_hash_len; i++) {
    sprintf(new_hash_str + (i * 2), "%02x", new_hash[i]);
  }
  new_hash_str[64] = 0;
  result = strcmp(new_hash_str, stored_hash) == 0;
  wrenEnsureSlots(vm, 2);
  wrenSetSlotBool(vm, 0, result);
}

static void httpCall(WrenVM* vm) {

  // Get data from Wren
  const char* url = wrenGetSlotString(vm, 1);
  const char* method = wrenGetSlotString(vm, 2);
  const char* raw_headers = wrenGetSlotString(vm, 3);
  const char* postData = wrenGetSlotString(vm, 4);
  const char* basicAuth = wrenGetSlotString(vm, 5);

  // Set request
  struct HttpRequest request;
  request.url = string_safe_copy(url);
  request.method = string_safe_copy(method);
  request.raw_headers = string_safe_copy(raw_headers);
  request.postData = string_safe_copy(postData);
  request.basicAuth = string_safe_copy(basicAuth);

  // Initialize response
  struct HttpResponse response;
  response.error = 0;
  response.status = HTTP_OK;
  response.headers = "Content-Type: text/json";
  response.body = "{}";

  // Perform request
  httpCallPerform(&request, &response);
  printf("Response: %s\n", response.body);
  printf("Status: %d\n", response.status);
  printf("Headers: %s\n", response.headers);
  printf("Error: %d\n", response.error);

  // Set response to Wren
  wrenEnsureSlots(vm, 6);
  wrenSetSlotNewList(vm, 0);
  wrenSetSlotDouble(vm, 5, response.status);
  wrenSetSlotString(vm, 6, string_safe_copy(response.headers));
  wrenSetSlotString(vm, 7, string_safe_copy(response.body));
  wrenSetSlotDouble(vm, 8, response.error);
  wrenInsertInList(vm, 0, 0, 5);
  wrenInsertInList(vm, 0, 1, 6);
  wrenInsertInList(vm, 0, 2, 7);
  wrenInsertInList(vm, 0, 3, 8);

  free(request.url);
  free(request.method);
  free(request.raw_headers);
  free(request.postData);
  free(request.basicAuth);
}

WrenForeignMethodFn wrenBindForeignMethod(WrenVM* vm, const char* module,
                                          const char* className, bool isStatic,
                                          const char* signature) {
  if(strcmp(module, MAIN_MODULE_NAME) == 0) {
    if(strcmp(className, "Util") == 0) {
      if(strcmp(signature, "randomString_(_)") == 0) {
        return randomString;
      }
      if(strcmp(signature, "hash_(_)") == 0) {
        return hashPassword;
      }
      if(strcmp(signature, "verify_(_,_)") == 0) {
        return verifyPassword;
      }
    }
    if(strcmp(className, "Http") == 0) {
      if(strcmp(signature, "call_(_,_,_,_,_)") == 0) {
        return httpCall;
      }
    }
  }
  return NULL;
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

struct BialetResponse bialetRun(char* module, char* code,
                                struct HttpMessage* hm) {
  struct BialetResponse r;
  r.length = 0;
  int     error = 0;
  WrenVM* vm = 0;

  vm = wrenNewVM(&wren_config);
  wrenSetUserData(vm, module);
  wrenInterpret(vm, MAIN_MODULE_NAME, bialetModuleSource);
  if(hm) {
    /* Initialize request */
    wrenEnsureSlots(vm, 4);
    wrenGetVariable(vm, MAIN_MODULE_NAME, "Request", 0);
    WrenHandle* requestClass = wrenGetSlotHandle(vm, 0);
    WrenHandle* initMethod = wrenMakeCallHandle(vm, "init(_,_,_)");
    wrenSetSlotHandle(vm, 0, requestClass);
    wrenSetSlotString(vm, 1, get_string(hm->message));
    wrenSetSlotString(vm, 2, get_string(hm->routes));

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
    wrenGetVariable(vm, MAIN_MODULE_NAME, "Response", 0);
    WrenHandle* responseClass = wrenGetSlotHandle(vm, 0);
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
    r.status = HTTP_ERROR;
    r.header = BIALET_HEADERS;
    r.body = BIALET_ERROR_PAGE;
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
  sqlite3_exec(db, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL);
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
  wren_config.bindForeignMethodFn = &wrenBindForeignMethod;

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
