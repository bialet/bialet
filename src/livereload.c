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
  response->length = (int)len;
  return 1;
}

void livereload_notify(void) {
  if(!enabled)
    return;
  version = (long)time(NULL);
}

int livereload_inject_response(struct BialetResponse* response) {
  if(!enabled)
    return 0;
  if(response->body == NULL || response->header == NULL)
    return 0;
  if(strstr(response->header, "text/html") == NULL)
    return 0;

  size_t body_len = (size_t)response->length;
  if(body_len == 0)
    body_len = strlen(response->body);
  if(body_len == 0)
    return 0;

  size_t script_len = sizeof(kScript) - 1;

  const char* marker = strstr(response->body, "</body>");
  size_t      insert_at = marker ? (size_t)(marker - response->body) : body_len;

  size_t new_len = body_len + script_len;
  char*  new_body = (char*)malloc(new_len + 1);
  if(new_body == NULL)
    return 0;

  memcpy(new_body, response->body, insert_at);
  memcpy(new_body + insert_at, kScript, script_len);
  memcpy(new_body + insert_at + script_len, response->body + insert_at,
         body_len - insert_at);
  new_body[new_len] = '\0';

  free(response->body);
  response->body = new_body;
  response->body_owned = 1;
  response->length = (int)new_len;
  return 1;
}
