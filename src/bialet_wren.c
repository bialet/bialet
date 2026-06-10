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
#include "messages.h"
#include "server.h"
#include "utils.h"
#include "wren.h"
#include <ctype.h>
#include <sqlite3.h>
#include <string.h>
#include <strings.h>
#include <time.h>

// Portable case-insensitive substring search (avoids _GNU_SOURCE dependency)
static const char* bialet_strcasestr(const char* haystack, const char* needle) {
  if(!needle[0])
    return haystack;
  for(; *haystack; haystack++) {
    if(tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
      const char* h = haystack;
      const char* n = needle;
      while(*h && *n && tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
        h++;
        n++;
      }
      if(!*n)
        return haystack;
    }
  }
  return NULL;
}

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
#define CLI_MODULE_NAME "bialet_cli"

WrenConfiguration          wren_config;
static struct BialetConfig bialet_config;
sqlite3*                   db;

static void bialetWrenWrite(WrenVM* vm, const char* message) {
  (void)vm;
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
  int ret = snprintf(fullPath, sizeof(fullPath), "%s/%s", 
                     bialet_config.full_root_dir, path);
  if(ret < 0 || ret >= (int)sizeof(fullPath)) {
    message(red("Error"), "Path too long in bialetReadFile");
    return NULL;
  }
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
    if(length < 0) {
      fclose(f);
      return NULL;
    }
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
      char* user = strtok(name_copy, "/");
      char* repo = strtok(NULL, "/");
      char* path = strtok(NULL, "");
      if(!user || !repo || !path) {
        message(red("Error"), "Invalid GitHub URL.");
        return result;
      }
      int ret = snprintf(url, sizeof(url), BIALET_REMOTE_MODULE_GITHUB_URL, user, repo, branch, path);
      if(ret < 0 || ret >= (int)sizeof(url)) {
        message(red("Error"), "GitHub URL too long.");
        return result;
      }
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
      resp.error = 0;
      req.method = string_safe_copy("GET");
      req.basicAuth = string_safe_copy("");
      req.raw_headers = string_safe_copy("");
      req.postData = string_safe_copy("");
      req.url = string_safe_copy(url);
      httpCallPerform(&req, &resp);
      // Check if HTTP request was successful (2xx status codes)
      if(resp.status >= 200 && resp.status < 300 && !resp.error) {
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
  (void)vm;
  char lineMessage[MAX_LINE_ERROR_LEN];
  snprintf(lineMessage, sizeof(lineMessage), "%s line %d", module, line);
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
  snprintf(str, 21, "%lld", value);
  return str;
}

static void queryExecute(WrenVM* vm, BialetQuery* query) {
  (void)vm;
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
        sqlite3_finalize(stmt);
        return;
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
          message(red("Query Error"), "Uknown type on binding result");
          break;
      }
      addResultRow(query, rowCount, columns[i], value, size, type);
    }
    rowCount++;
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
  filesIds[0] = '\0'; // Initialize empty string

  // Find Content-Type header to check if it's multipart/form-data
  const char* headers = hm->message.str;
  const char* headerEnd = strstr(headers, "\r\n\r\n");

  if(!headerEnd)
    return 0;

  // Search for Content-Type header
  const char* ctStart = bialet_strcasestr(headers, "Content-Type:");
  if(!ctStart || ctStart > headerEnd)
    return 0;

  ctStart += 13; // Skip "Content-Type:"
  while(*ctStart == ' ')
    ctStart++; // Skip spaces

  // Check if it's multipart/form-data
  if(strncasecmp(ctStart, "multipart/form-data", 19) != 0)
    return 0;

  // Extract boundary
  const char* boundaryStart = bialet_strcasestr(ctStart, "boundary=");
  if(!boundaryStart || boundaryStart > headerEnd)
    return 0;

  boundaryStart += 9; // Skip "boundary="
  char boundary[256];
  int  i = 0;
  while(boundaryStart[i] && boundaryStart[i] != '\r' && boundaryStart[i] != '\n' &&
        boundaryStart[i] != ';' && i < 255) {
    boundary[i] = boundaryStart[i];
    i++;
  }
  boundary[i] = '\0';

  // Find the body (after \r\n\r\n)
  const char* body = headerEnd + 4;
  size_t      bodyLen = hm->message.len - (body - hm->message.str);

  // Create full boundary markers
  char startBoundary[260];
  char endBoundary[264];
  snprintf(startBoundary, sizeof(startBoundary), "--%s", boundary);
  snprintf(endBoundary, sizeof(endBoundary), "--%s--", boundary);

  // Parse multipart parts
  const char* part = body;
  int         firstFile = 1;

  while(part && part < body + bodyLen) {
    // Find next boundary
    part = strstr(part, startBoundary);
    if(!part)
      break;

    part += strlen(startBoundary);

    // Skip CRLF after boundary
    if(*part == '\r')
      part++;
    if(*part == '\n')
      part++;

    // Check if this is the end boundary
    if(strncmp(part - strlen(startBoundary), endBoundary, strlen(endBoundary)) ==
       0) {
      break;
    }

    // Parse headers for this part
    const char* partHeaderEnd = strstr(part, "\r\n\r\n");
    if(!partHeaderEnd || partHeaderEnd >= body + bodyLen)
      break;

    // Extract Content-Disposition
    const char* cdStart = bialet_strcasestr(part, "Content-Disposition:");
    if(!cdStart || cdStart > partHeaderEnd)
      continue;

    // Extract filename
    const char* filenameStart = bialet_strcasestr(cdStart, "filename=\"");
    if(!filenameStart || filenameStart > partHeaderEnd) {
      part = partHeaderEnd + 4;
      continue;
    }

    filenameStart += 10; // Skip 'filename="'
    const char* filenameEnd = strchr(filenameStart, '"');
    if(!filenameEnd || filenameEnd > partHeaderEnd)
      continue;

    char filename[256];
    int  fnLen = filenameEnd - filenameStart;
    if(fnLen > 255)
      fnLen = 255;
    strncpy(filename, filenameStart, fnLen);
    filename[fnLen] = '\0';

    // Skip empty filenames
    if(filename[0] == '\0') {
      part = partHeaderEnd + 4;
      continue;
    }

    // Extract field name
    const char* nameStart = bialet_strcasestr(cdStart, "name=\"");
    if(!nameStart || nameStart > partHeaderEnd)
      continue;

    nameStart += 6; // Skip 'name="'
    const char* nameEnd = strchr(nameStart, '"');
    if(!nameEnd || nameEnd > partHeaderEnd)
      continue;

    char fieldName[256];
    int  nameLen = nameEnd - nameStart;
    if(nameLen > 255)
      nameLen = 255;
    strncpy(fieldName, nameStart, nameLen);
    fieldName[nameLen] = '\0';

    // Extract Content-Type for this part
    const char* partCtStart = bialet_strcasestr(part, "Content-Type:");
    char        contentTypeStr[256] = "application/octet-stream";
    if(partCtStart && partCtStart < partHeaderEnd) {
      partCtStart += 13; // Skip "Content-Type:"
      while(*partCtStart == ' ')
        partCtStart++;

      int ctLen = 0;
      while(partCtStart[ctLen] && partCtStart[ctLen] != '\r' &&
            partCtStart[ctLen] != '\n' && ctLen < 255) {
        contentTypeStr[ctLen] = partCtStart[ctLen];
        ctLen++;
      }
      contentTypeStr[ctLen] = '\0';
    }

    // Find file data (after part headers)
    const char* fileData = partHeaderEnd + 4;

    // Find end of file data (next boundary)
    const char* nextBoundary = strstr(fileData, startBoundary);
    if(!nextBoundary)
      nextBoundary = body + bodyLen;

    // Calculate file size (remove trailing \r\n before boundary)
    size_t fileSize = nextBoundary - fileData;
    if(fileSize >= 2 && fileData[fileSize - 2] == '\r' &&
       fileData[fileSize - 1] == '\n') {
      fileSize -= 2;
    }

    // Validate file size to prevent disk abuse
    extern struct BialetConfig bialet_config;
    if(fileSize > bialet_config.max_upload_size) {
      // Skip this file - it exceeds the maximum allowed size
      char sizeMsg[512];
      snprintf(
          sizeMsg, sizeof(sizeMsg),
          "File '%s' exceeds maximum upload size (%zu bytes > %zu bytes allowed)",
          filename, fileSize, bialet_config.max_upload_size);
      message(red("Upload Error"), sizeMsg);
      part = fileData;
      continue;
    }

    // Save file to database
    sqlite3_stmt* stmt;
    int           result = sqlite3_prepare_v2(db,
                                              "INSERT INTO BIALET_FILES (name, "
                                                        "originalFileName, type, file, size, isTemp) "
                                                        "VALUES (?, ?, ?, ?, ?, 1)",
                                              -1, &stmt, 0);

    if(result == SQLITE_OK) {
      sqlite3_bind_text(stmt, 1, fieldName, -1, SQLITE_STATIC);
      sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_STATIC);
      sqlite3_bind_text(stmt, 3, contentTypeStr, -1, SQLITE_STATIC);
      sqlite3_bind_blob(stmt, 4, fileData, fileSize, SQLITE_STATIC);
      sqlite3_bind_int(stmt, 5, fileSize);

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
      }
      sqlite3_finalize(stmt);
    }

    part = fileData;
  }

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
    // Save uploaded files with size validation (max_upload_size config)
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

int bialetValidateSyntax(const char* filePath) {
  char* code = readFile(filePath);
  if(code == NULL) {
    fprintf(stderr, "Error: Cannot read file '%s'\n", filePath);
    return 1;
  }

  WrenVM*             vm = wrenNewVM(&wren_config);
  WrenInterpretResult result = wrenInterpret(vm, filePath, code);
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

const char* bialetGetFullRootDir() {
  return bialet_config.full_root_dir;
}

static int isTestFile(const char* name) {
  size_t len = strlen(name);
  if(len < 6)
    return 0; // min: x.wren
  if(strcmp(name + len - 5, ".wren") != 0)
    return 0;
  if(strcmp(name, TEST_INIT_FILE) == 0)
    return 0;
  return 1;
}

static int runTestFile(const char* testPath, const char* testName,
                       const char* initPath) {
  (void)testName;
  char* code = readFile(testPath);
  if(code == NULL) {
    fprintf(stderr, "  ✗ Cannot read test file: %s\n", testPath);
    return 1;
  }

  WrenVM* vm = wrenNewVM(&wren_config);

  // Initialize Response and Date in main module
  wrenInterpret(vm, MAIN_MODULE_NAME, "Response.init\nDate.init");

  // Run init file if provided
  if(initPath != NULL) {
    char* initCode = readFile(initPath);
    if(initCode != NULL) {
      wrenInterpret(vm, MAIN_MODULE_NAME, initCode);
      free(initCode);
    }
  }

  // Run test code in main module
  WrenInterpretResult result = wrenInterpret(vm, MAIN_MODULE_NAME, code);

  wrenFreeVM(vm);
  free(code);

  return (result == WREN_RESULT_SUCCESS) ? 0 : 1;
}

int bialetRunTests(const char* testDir, const char* rootDir) {
  (void)rootDir;
  char testsPath[MAX_MODULE_LEN];
  snprintf(testsPath, sizeof(testsPath), "%s/%s", testDir, TESTS_DIR);

  struct stat st;
  if(stat(testsPath, &st) != 0 || !S_ISDIR(st.st_mode)) {
    fprintf(stderr, "Error: Test directory not found: %s\n", testsPath);
    return 1;
  }

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
  while((entry = readdir(dir)) != NULL && testCount < MAX_TEST_FILES) {
    if(isTestFile(entry->d_name)) {
      testFiles[testCount] = strdup(entry->d_name);
      testCount++;
    }
  }
  closedir(dir);

  if(testCount == 0) {
    printf("No tests found.\n");
    return 0;
  }

  // Check for init file
  char initPath[MAX_MODULE_LEN + 16];
  snprintf(initPath, sizeof(initPath), "%s/%s", testsPath, TEST_INIT_FILE);
  char* initPathPtr = (stat(initPath, &st) == 0) ? initPath : NULL;

  int passed = 0;
  int failed = 0;

  // Run each test file
  for(int i = 0; i < testCount; i++) {
    char testPath[MAX_MODULE_LEN * 2];
    snprintf(testPath, sizeof(testPath), "%s/%s", testsPath, testFiles[i]);

    printf("  Running %s...\n", testFiles[i]);

    int result = runTestFile(testPath, testFiles[i], initPathPtr);
    if(result == 0) {
      passed++;
      printf("    ✓ Passed\n");
    } else {
      failed++;
      printf("    ✗ Failed\n");
    }

    free(testFiles[i]);
  }

  printf("\n%d passed, %d failed\n", passed, failed);
  return (failed > 0) ? 1 : 0;
}
void bialetInit(struct BialetConfig* config) {
  char db_path[MAX_MODULE_LEN];
  int  lastChar = (int)strlen(config->db_path) - 1;
  if(config->db_path[0] == '/') {
    strncpy(db_path, config->db_path, sizeof(db_path) - 1);
    db_path[sizeof(db_path) - 1] = '\0';
  } else {
    if(config->db_path[lastChar] == '/') {
      config->db_path[lastChar] = '\0';
    }
    int ret = snprintf(db_path, sizeof(db_path), "%s/%s", 
                       config->root_dir, config->db_path);
    if(ret < 0 || ret >= (int)sizeof(db_path)) {
      message(red("Error"), "Database path too long");
      exit(BIALET_SQLITE_ERROR);
    }
  }
  if(sqlite3_open_v2(db_path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,
                     NULL) != SQLITE_OK) {
    message(red("SQL Error"), "Can't open database in", config->db_path);
    exit(BIALET_SQLITE_ERROR);
  }
  // Set Pragmas using configuration values
  char pragma_cmd[256];

  // Foreign keys (configurable: 0=OFF, 1=ON)
  snprintf(pragma_cmd, sizeof(pragma_cmd), "PRAGMA foreign_keys = %s;",
           config->sqlite_foreign_keys ? "ON" : "OFF");
  sqlite3_exec(db, pragma_cmd, NULL, NULL, NULL);

  // Synchronous mode (configurable: 0=OFF, 1=NORMAL, 2=FULL, 3=EXTRA)
  const char* sync_modes[] = {"OFF", "NORMAL", "FULL", "EXTRA"};
  int         sync_mode = config->sqlite_synchronous;
  if(sync_mode < 0 || sync_mode > 3)
    sync_mode = 1; // Default to NORMAL
  snprintf(pragma_cmd, sizeof(pragma_cmd), "PRAGMA synchronous = %s;",
           sync_modes[sync_mode]);
  sqlite3_exec(db, pragma_cmd, NULL, NULL, NULL);

  // WAL mode (configurable via wal_mode flag)
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
  wren_config.enableTests = config->enable_tests;

  httpCallInit(&bialet_config);
}

void bialetCleanup() {
  if(db) {
    sqlite3_close(db);
    db = NULL;
  }
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
