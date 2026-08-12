#include "markdown.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OUTPUT 2 * 1024 * 1024

typedef struct {
  char*  data;
  size_t pos;
  size_t cap;
} MdBuf;

// Ensures [extra] more bytes (plus a NUL terminator) fit in the buffer.
// The output is hard-capped at MAX_OUTPUT, so inputs that cannot fit are
// rejected instead of overflowing the allocation.
static bool md_reserve(MdBuf* b, size_t extra) {
  if(b->pos + extra + 1 <= b->cap)
    return true;
  size_t needed = b->pos + extra + 1;
  size_t new_cap = b->cap * 2;
  if(new_cap < needed)
    new_cap = needed;
  if(new_cap > MAX_OUTPUT)
    new_cap = MAX_OUTPUT;
  if(new_cap < needed)
    return false;
  char* tmp = realloc(b->data, new_cap);
  if(tmp == NULL)
    return false;
  b->data = tmp;
  b->cap = new_cap;
  return true;
}

static bool md_appendn(MdBuf* b, const char* s, size_t n) {
  if(!md_reserve(b, n))
    return false;
  memcpy(b->data + b->pos, s, n);
  b->pos += n;
  b->data[b->pos] = '\0';
  return true;
}

static bool md_append(MdBuf* b, const char* s) {
  return md_appendn(b, s, strlen(s));
}

static bool md_printf(MdBuf* b, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  int needed = vsnprintf(NULL, 0, fmt, args);
  va_end(args);
  if(needed < 0)
    return false;
  size_t extra = (size_t)needed;
  if(!md_reserve(b, extra))
    return false;
  va_start(args, fmt);
  vsnprintf(b->data + b->pos, extra + 1, fmt, args);
  va_end(args);
  b->pos += extra;
  return true;
}

static bool is_ordered_list_item(const char* line) {
  if(*line < '0' || *line > '9')
    return false;
  while(*line >= '0' && *line <= '9')
    line++;
  return (*line == '.' && *(line + 1) == ' ');
}

static const char* skip_ordered_list_prefix(const char* line) {
  while(*line >= '0' && *line <= '9')
    line++;
  return line + 2;
}

static bool escape_html(const char* src, MdBuf* b) {
  while(*src) {
    const char* special = src;
    while(*special && *special != '&' && *special != '<' && *special != '>')
      special++;
    if(special > src) {
      if(!md_appendn(b, src, (size_t)(special - src)))
        return false;
      src = special;
    }
    if(*src == '\0')
      break;
    const char* repl = *src == '&' ? "&amp;" : (*src == '<' ? "&lt;" : "&gt;");
    if(!md_append(b, repl))
      return false;
    src++;
  }
  return true;
}

static bool render_inline(const char* src, MdBuf* b) {
  while(*src) {
    if(strncmp(src, "**", 2) == 0) {
      if(!md_append(b, "<strong>"))
        return false;
      src += 2;
      const char* end = strstr(src, "**");
      if(end) {
        if(!md_printf(b, "%.*s</strong>", (int)(end - src), src))
          return false;
        src = end + 2;
        continue;
      }
    } else if(*src == '*' && src[1] != '*') {
      if(!md_append(b, "<em>"))
        return false;
      src++;
      const char* end = strchr(src, '*');
      if(end) {
        if(!md_printf(b, "%.*s</em>", (int)(end - src), src))
          return false;
        src = end + 1;
        continue;
      }
    } else if(*src == '`') {
      src++;
      const char* end = strchr(src, '`');
      if(end) {
        if(!md_append(b, "<code>"))
          return false;
        if(!escape_html(src, b))
          return false;
        if(!md_append(b, "</code>"))
          return false;
        src = end + 1;
        continue;
      }
    } else if(strncmp(src, "![", 2) == 0) {
      const char* alt_end = strchr(src + 2, ']');
      const char* url_start = alt_end ? strchr(alt_end, '(') : NULL;
      const char* url_end = url_start ? strchr(url_start, ')') : NULL;
      if(alt_end && url_start && url_end) {
        if(!md_printf(b, "<img alt=\"%.*s\" src=\"%.*s\">",
                      (int)(alt_end - (src + 2)), src + 2,
                      (int)(url_end - url_start - 1), url_start + 1))
          return false;
        src = url_end + 1;
        continue;
      }
    } else if(*src == '[') {
      const char* text_end = strchr(src, ']');
      const char* url_start = text_end ? strchr(text_end, '(') : NULL;
      const char* url_end = url_start ? strchr(url_start, ')') : NULL;
      if(text_end && url_start && url_end) {
        if(!md_printf(b, "<a href=\"%.*s\">%.*s</a>", (int)(url_end - url_start - 1),
                      url_start + 1, (int)(text_end - (src + 1)), src + 1))
          return false;
        src = url_end + 1;
        continue;
      }
    }
    if(!md_appendn(b, src, 1))
      return false;
    src++;
  }
  return true;
}

char* markdown_to_html(const char* markdown) {
  MdBuf buf = {0};
  buf.data = calloc(1, MAX_OUTPUT);
  buf.cap = MAX_OUTPUT;
  char* input = strdup(markdown);
  if(buf.data == NULL || input == NULL) {
    free(input);
    free(buf.data);
    return NULL;
  }

  // Split into lines preserving empty lines
  int    line_capacity = 256;
  char** lines = malloc(line_capacity * sizeof(char*));
  if(lines == NULL) {
    free(input);
    free(buf.data);
    return NULL;
  }
  int   line_count = 0;
  char* p = input;
  char* line_start = input;

  while(*p) {
    if(*p == '\n') {
      *p = '\0';
      if(line_count >= line_capacity) {
        line_capacity *= 2;
        char** tmp = realloc(lines, line_capacity * sizeof(char*));
        if(tmp == NULL) {
          free(input);
          free(buf.data);
          free(lines);
          return NULL;
        }
        lines = tmp;
      }
      lines[line_count++] = line_start;
      line_start = p + 1;
    }
    p++;
  }
  if(line_start < p) {
    if(line_count >= line_capacity) {
      line_capacity++;
      char** tmp = realloc(lines, line_capacity * sizeof(char*));
      if(tmp == NULL) {
        free(input);
        free(buf.data);
        free(lines);
        return NULL;
      }
      lines = tmp;
    }
    lines[line_count++] = line_start;
  }

  bool in_list = false, in_olist = false, in_blockquote = false,
       in_codeblock = false, in_table = false;
  bool table_header_parsed = false;

  int i = 0;

  // Skip metadata if present at the beginning
  if(i < line_count && strcmp(lines[i], "---") == 0) {
    i++;
    while(i < line_count && strcmp(lines[i], "---") != 0)
      i++;
    if(i < line_count)
      i++;
  }

  while(i < line_count) {
    char* line = lines[i];

    while(*line == ' ' && !in_codeblock)
      line++;

    if(strncmp(line, "```", 3) == 0) {
      if(!in_codeblock) {
        if(!md_append(&buf, "<pre><code>"))
          goto fail;
        in_codeblock = true;
      } else {
        if(!md_append(&buf, "</code></pre>\n"))
          goto fail;
        in_codeblock = false;
      }
      i++;
      continue;
    }

    if(in_codeblock) {
      if(!escape_html(line, &buf))
        goto fail;
      if(!md_append(&buf, "\n"))
        goto fail;
      i++;
      continue;
    }

    if(line[0] == '>') {
      if(!in_blockquote) {
        if(!md_append(&buf, "<blockquote>\n"))
          goto fail;
        in_blockquote = true;
      }
      // Skip the marker and at most one following space without ever moving
      // past the terminator. `line + 2` read one byte beyond the NUL for a line
      // consisting of exactly ">", which any document can contain.
      const char* quoted = line + 1;
      if(*quoted == ' ')
        quoted++;
      if(!render_inline(quoted, &buf))
        goto fail;
      if(!md_append(&buf, "<br>\n"))
        goto fail;
      i++;
      continue;
    } else if(in_blockquote) {
      if(!md_append(&buf, "</blockquote>\n"))
        goto fail;
      in_blockquote = false;
    }

    if(line[0] == '|' && strchr(line + 1, '|')) {
      if(!in_table) {
        if(!md_append(&buf, "<table>\n"))
          goto fail;
        in_table = true;
        table_header_parsed = false;
      }

      int is_separator = 1;
      for(const char* c = line; *c; ++c) {
        if(*c != '|' && *c != '-' && *c != ' ') {
          is_separator = 0;
          break;
        }
      }

      if(is_separator) {
        i++;
        continue;
      }

      if(!md_append(&buf, "<tr>"))
        goto fail;
      char* row = strdup(line);
      char* saveptr = NULL;
      char* cell = strtok_r(row, "|", &saveptr);
      while(cell) {
        while(*cell == ' ')
          cell++;
        char* end = cell + strlen(cell) - 1;
        while(end > cell && *end == ' ')
          *end-- = '\0';

        if(!md_printf(&buf, table_header_parsed ? "<td>" : "<th>"))
          goto fail;
        if(!render_inline(cell, &buf))
          goto fail;
        if(!md_printf(&buf, table_header_parsed ? "</td>" : "</th>"))
          goto fail;
        cell = strtok_r(NULL, "|", &saveptr);
      }
      free(row);
      if(!md_append(&buf, "</tr>\n"))
        goto fail;
      table_header_parsed = true;
      i++;
      continue;
    } else if(in_table) {
      if(!md_append(&buf, "</table>\n"))
        goto fail;
      in_table = false;
    }

    if(is_ordered_list_item(line)) {
      if(in_list) {
        if(!md_append(&buf, "</ul>\n"))
          goto fail;
        in_list = false;
      }
      if(!in_olist) {
        if(!md_append(&buf, "<ol>\n"))
          goto fail;
        in_olist = true;
      }
      if(!md_append(&buf, "<li>"))
        goto fail;
      if(!render_inline(skip_ordered_list_prefix(line), &buf))
        goto fail;
      if(!md_append(&buf, "</li>\n"))
        goto fail;
      i++;
      continue;
    } else if(in_olist) {
      if(!md_append(&buf, "</ol>\n"))
        goto fail;
      in_olist = false;
    }

    if(strncmp(line, "- ", 2) == 0 || strncmp(line, "* ", 2) == 0) {
      if(in_olist) {
        if(!md_append(&buf, "</ol>\n"))
          goto fail;
        in_olist = false;
      }
      if(!in_list) {
        if(!md_append(&buf, "<ul>\n"))
          goto fail;
        in_list = true;
      }
      if(!md_append(&buf, "<li>"))
        goto fail;
      if(!render_inline(line + 2, &buf))
        goto fail;
      if(!md_append(&buf, "</li>\n"))
        goto fail;
      i++;
      continue;
    } else if(in_list) {
      if(!md_append(&buf, "</ul>\n"))
        goto fail;
      in_list = false;
    }

    if(line[0] == '#') {
      int level = 0;
      while(line[level] == '#' && level < 6)
        level++;
      if(line[level] == ' ') {
        if(!md_printf(&buf, "<h%d>", level))
          goto fail;
        if(!render_inline(line + level + 1, &buf))
          goto fail;
        if(!md_printf(&buf, "</h%d>\n", level))
          goto fail;
        i++;
        continue;
      }
    }

    if(*line != '\0') {
      if(!md_append(&buf, "<p>"))
        goto fail;
      if(!render_inline(line, &buf))
        goto fail;

      // Collect consecutive non-empty lines into the same paragraph
      i++;
      while(i < line_count) {
        char* next_line = lines[i];

        // Trim leading spaces
        while(*next_line == ' ')
          next_line++;

        // Empty line ends the paragraph
        if(*next_line == '\0') {
          break;
        }

        // Check if this line starts a special block
        bool is_special =
            (next_line[0] == '#') || (strncmp(next_line, "- ", 2) == 0) ||
            (strncmp(next_line, "* ", 2) == 0) ||
            (strncmp(next_line, "```", 3) == 0) || (next_line[0] == '>') ||
            (next_line[0] == '|' && strchr(next_line + 1, '|')) ||
            is_ordered_list_item(next_line);

        if(is_special) {
          break;
        }

        // Add space and continue the paragraph
        if(!md_append(&buf, " "))
          goto fail;
        if(!render_inline(next_line, &buf))
          goto fail;
        i++;
      }

      if(!md_append(&buf, "</p>\n"))
        goto fail;
      continue;
    }

    i++;
  }

  if(in_list && !md_append(&buf, "</ul>\n"))
    goto fail;
  if(in_olist && !md_append(&buf, "</ol>\n"))
    goto fail;
  if(in_table && !md_append(&buf, "</table>\n"))
    goto fail;
  if(in_blockquote && !md_append(&buf, "</blockquote>\n"))
    goto fail;
  if(in_codeblock && !md_append(&buf, "</code></pre>\n"))
    goto fail;

  free(lines);
  free(input);
  return buf.data;

fail:
  free(lines);
  free(input);
  free(buf.data);
  return NULL;
}
