#include "show_errors.h"

#include "messages.h"
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>

static int    enabled = 0;
static char*  captured = NULL;
static size_t captured_len = 0;
static size_t captured_cap = 0;

extern sqlite3* db;

void show_errors_init(void) {
  if(db == NULL)
    return;

  sqlite3_stmt* stmt = NULL;
  const char*   sql = "SELECT val FROM BIALET_CONFIG WHERE key = ?";
  if(sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK)
    return;

  sqlite3_bind_text(stmt, 1, SHOW_ERRORS_KEY, -1, SQLITE_STATIC);
  if(sqlite3_step(stmt) == SQLITE_ROW) {
    const char* val = (const char*)sqlite3_column_text(stmt, 0);
    if(val != NULL && strcmp(val, "0") != 0 && val[0] != '\0') {
      enabled = 1;
      message(yellow("Showing errors in browser"));
    }
  }
  sqlite3_finalize(stmt);
}

int show_errors_enabled(void) {
  return enabled;
}

void show_errors_clear(void) {
  captured_len = 0;
  if(captured != NULL)
    captured[0] = '\0';
}

static void capture_append(const char* text, size_t len) {
  if(text == NULL || len == 0)
    return;
  if(captured_len + len + 1 > captured_cap) {
    size_t new_cap = captured_cap == 0 ? 256 : captured_cap * 2;
    while(new_cap < captured_len + len + 1)
      new_cap *= 2;
    char* new_buf = (char*)realloc(captured, new_cap);
    if(new_buf == NULL)
      return;
    captured = new_buf;
    captured_cap = new_cap;
  }
  memcpy(captured + captured_len, text, len);
  captured_len += len;
  captured[captured_len] = '\0';
}

void show_errors_capture(const char* type, const char* module, int line,
                         const char* msg) {
  if(!enabled)
    return;
  if(msg == NULL)
    msg = "";

  int needed = 0;
  if(type != NULL)
    needed += snprintf(NULL, 0, "%s: ", type);
  if(module != NULL && line > 0)
    needed += snprintf(NULL, 0, "%s line %d: ", module, line);
  else if(module != NULL)
    needed += snprintf(NULL, 0, "%s: ", module);
  needed += snprintf(NULL, 0, "%s", msg);

  char* line_buf = (char*)malloc((size_t)needed + 1);
  if(line_buf == NULL)
    return;

  size_t offset = 0;
  if(type != NULL)
    offset += (size_t)snprintf(line_buf + offset, (size_t)needed + 1 - offset,
                               "%s: ", type);
  if(module != NULL && line > 0)
    offset += (size_t)snprintf(line_buf + offset, (size_t)needed + 1 - offset,
                               "%s line %d: ", module, line);
  else if(module != NULL)
    offset += (size_t)snprintf(line_buf + offset, (size_t)needed + 1 - offset,
                               "%s: ", module);
  snprintf(line_buf + offset, (size_t)needed + 1 - offset, "%s", msg);

  capture_append(line_buf, strlen(line_buf));
  capture_append("\n", 1);
  free(line_buf);
}

// Escapes text for safe embedding inside HTML. Returns a newly allocated
// string, or NULL on allocation failure.
static char* html_escape(const char* src) {
  size_t len = strlen(src);
  size_t cap = len * 6 + 1;
  char*  out = (char*)malloc(cap);
  if(out == NULL)
    return NULL;

  size_t j = 0;
  for(size_t i = 0; i < len; i++) {
    const char* rep = NULL;
    switch(src[i]) {
      case '&':
        rep = "&amp;";
        break;
      case '<':
        rep = "&lt;";
        break;
      case '>':
        rep = "&gt;";
        break;
      case '"':
        rep = "&quot;";
        break;
      case '\'':
        rep = "&#39;";
        break;
      default:
        break;
    }
    if(rep != NULL) {
      size_t rep_len = strlen(rep);
      if(j + rep_len + 1 > cap) {
        size_t new_cap = cap * 2;
        char*  nb = (char*)realloc(out, new_cap);
        if(nb == NULL) {
          free(out);
          return NULL;
        }
        out = nb;
        cap = new_cap;
      }
      memcpy(out + j, rep, rep_len);
      j += rep_len;
    } else {
      out[j++] = src[i];
    }
  }
  out[j] = '\0';
  return out;
}

char* show_errors_page(void) {
  if(captured == NULL || captured_len == 0)
    return NULL;

  char* escaped = html_escape(captured);
  if(escaped == NULL)
    return NULL;

  const char* head = "<!DOCTYPE html><html lang=\"en\"><head><meta "
                     "charset=\"utf-8\"/><title>Bialet Error</title></head><body "
                     "style=\"font:1rem system-ui,monospace;margin:2em;color:#024\">"
                     "<h1>Bialet Error</h1><pre style=\"white-space:pre-wrap\">";
  const char* tail =
      "</pre><hr><p style=\"font-size:.85em\">Enable with "
      "<code>Config.enable(\"BIALET_SHOW_ERRORS\")</code>. Disable it before "
      "deploying — this page exposes error details to visitors.</p></body></html>";

  size_t head_len = strlen(head);
  size_t esc_len = strlen(escaped);
  size_t tail_len = strlen(tail);
  char*  page = (char*)malloc(head_len + esc_len + tail_len + 1);
  if(page == NULL) {
    free(escaped);
    return NULL;
  }

  memcpy(page, head, head_len);
  memcpy(page + head_len, escaped, esc_len);
  memcpy(page + head_len + esc_len, tail, tail_len + 1);
  free(escaped);
  return page;
}
