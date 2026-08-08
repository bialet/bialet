#include "livereload.h"

#include "messages.h"
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int  enabled = 0;
static long version = 0;

static const char kScript[] = "<script>"
                              "(function(){var v=null;setInterval(function(){"
                              "var x=new XMLHttpRequest();"
                              "x.onload=function(){if(v===null)v=x.responseText;"
                              "else if(v!==x.responseText)location.reload()};"
                              "x.open('GET','/_livereload');x.send()"
                              "},1000)})()"
                              "</script>";

extern sqlite3* db;

void livereload_init(void) {
  if(db == NULL)
    return;

  sqlite3_stmt* stmt = NULL;
  const char*   sql = "SELECT val FROM BIALET_CONFIG WHERE key = ?";
  if(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    return;

  sqlite3_bind_text(stmt, 1, LIVERELOAD_KEY, -1, SQLITE_STATIC);
  if(sqlite3_step(stmt) == SQLITE_ROW) {
    const char* val = (const char*)sqlite3_column_text(stmt, 0);
    if(val != NULL && strcmp(val, "0") != 0 && val[0] != '\0') {
      enabled = 1;
      version = (long)time(NULL);
      message(yellow("Live reloading"));
    }
  }
  sqlite3_finalize(stmt);
}

int livereload_enabled(void) {
  return enabled;
}

int livereload_is_poll(const char* uri) {
  return uri != NULL && strcmp(uri, "/_livereload") == 0;
}

int livereload_try_handle(const char* uri, struct BialetResponse* response) {
  if(!enabled)
    return 0;
  if(strcmp(uri, "/_livereload") != 0)
    return 0;

  static char version_str[32];
  int         len = snprintf(version_str, sizeof(version_str), "%ld", version);

  response->status = 200;
  response->header = (char*)"Content-Type: text/plain\r\n";
  response->body = version_str;
  response->length = (size_t)len;
  return 1;
}

void livereload_notify(void) {
  if(!enabled)
    return;
  version = (long)time(NULL);
}

// Portable bounded substring search (POSIX memmem is not available on
// Windows). Returns a pointer to the first [needle] within [haystack_len]
// bytes of [haystack], or NULL.
static const char* find_bytes(const char* haystack, size_t haystack_len,
                              const char* needle, size_t needle_len) {
  if(needle_len == 0 || haystack_len < needle_len)
    return NULL;
  for(size_t i = 0; i <= haystack_len - needle_len; i++) {
    if(memcmp(haystack + i, needle, needle_len) == 0)
      return haystack + i;
  }
  return NULL;
}

int livereload_inject_response(struct BialetResponse* response) {
  if(!enabled)
    return 0;
  if(response->header == NULL)
    return 0;
  if(strstr(response->header, "text/html") == NULL)
    return 0;

  size_t body_len = response->length;
  if(body_len == 0 && response->body != NULL && response->body_owned) {
    // The length field is authoritative for opaque caller buffers (e.g. static
    // files). Only fall back to strlen() for bodies this struct owns, which
    // are always NUL-terminated.
    body_len = strlen(response->body);
  }

  size_t script_len = sizeof(kScript) - 1;

  size_t      insert_at = body_len;
  const char* marker = NULL;
  if(response->body != NULL && body_len > 0) {
    marker = find_bytes(response->body, body_len, "</body>", 7);
    if(marker)
      insert_at = (size_t)(marker - response->body);
  }
  if(insert_at > body_len)
    return 0;

  size_t new_len = body_len + script_len;
  char*  new_body = (char*)malloc(new_len + 1);
  if(new_body == NULL)
    return 0;

  if(insert_at > 0)
    memcpy(new_body, response->body, insert_at);
  memcpy(new_body + insert_at, kScript, script_len);
  if(body_len > insert_at)
    memcpy(new_body + insert_at + script_len, response->body + insert_at,
           body_len - insert_at);
  new_body[new_len] = '\0';

  // Only free a body this struct owns. Static literals (error pages) and
  // caller-owned buffers (e.g. file_content) are left to their owners.
  if(response->body_owned)
    free(response->body);
  response->body = new_body;
  response->body_owned = 1;
  response->length = new_len;
  return 1;
}
