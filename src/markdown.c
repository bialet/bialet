#include "markdown.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_OUTPUT 2 * 1024 * 1024

static char* escapeHtml(const char* src, char* out) {
  while(*src) {
    switch(*src) {
      case '&':
        out += sprintf(out, "&amp;");
        break;
      case '<':
        out += sprintf(out, "&lt;");
        break;
      case '>':
        out += sprintf(out, "&gt;");
        break;
      default:
        *out++ = *src;
        break;
    }
    src++;
  }
  return out;
}

static char* renderInline(const char* src, char* out) {
  while(*src) {
    if(strncmp(src, "**", 2) == 0) {
      out += sprintf(out, "<strong>");
      src += 2;
      const char* end = strstr(src, "**");
      if(end) {
        out += sprintf(out, "%.*s</strong>", (int)(end - src), src);
        src = end + 2;
        continue;
      }
    } else if(*src == '*' && src[1] != '*') {
      out += sprintf(out, "<em>");
      src++;
      const char* end = strchr(src, '*');
      if(end) {
        out += sprintf(out, "%.*s</em>", (int)(end - src), src);
        src = end + 1;
        continue;
      }
    } else if(*src == '`') {
      src++;
      const char* end = strchr(src, '`');
      if(end) {
        out += sprintf(out, "<code>");
        out = escapeHtml(src, out);
        out += sprintf(out, "</code>");
        src = end + 1;
        continue;
      }
    } else if(strncmp(src, "![", 2) == 0) {
      const char* alt_end = strchr(src + 2, ']');
      const char* url_start = strchr(alt_end, '(');
      const char* url_end = strchr(url_start, ')');
      if(alt_end && url_start && url_end) {
        out += sprintf(out, "<img alt=\"%.*s\" src=\"%.*s\">",
                       (int)(alt_end - (src + 2)), src + 2,
                       (int)(url_end - url_start - 1), url_start + 1);
        src = url_end + 1;
        continue;
      }
    } else if(*src == '[') {
      const char* text_end = strchr(src, ']');
      const char* url_start = strchr(text_end, '(');
      const char* url_end = strchr(url_start, ')');
      if(text_end && url_start && url_end) {
        out +=
            sprintf(out, "<a href=\"%.*s\">%.*s</a>", (int)(url_end - url_start - 1),
                    url_start + 1, (int)(text_end - (src + 1)), src + 1);
        src = url_end + 1;
        continue;
      }
    }
    *out++ = *src++;
  }
  return out;
}

char* markdownToHtml(const char* markdown) {
  char* html = calloc(1, MAX_OUTPUT);
  char* out = html;
  char* input = strdup(markdown);
  char* line = strtok(input, "\n");

  bool in_list = false, in_blockquote = false, in_codeblock = false,
       in_table = false;
  bool table_header_parsed = false;

  while(line) {
    while(*line == ' ' && !in_codeblock)
      line++;

    if(strncmp(line, "```", 3) == 0) {
      if(!in_codeblock) {
        out += sprintf(out, "<pre><code>");
        in_codeblock = true;
      } else {
        out += sprintf(out, "</code></pre>\n");
        in_codeblock = false;
      }
      line = strtok(NULL, "\n");
      continue;
    }

    if(in_codeblock) {
      out = escapeHtml(line, out);
      out += sprintf(out, "\n");
      line = strtok(NULL, "\n");
      continue;
    }

    if(line[0] == '>') {
      if(!in_blockquote) {
        out += sprintf(out, "<blockquote>\n");
        in_blockquote = true;
      }
      out = renderInline(line + 2, out);
      out += sprintf(out, "<br>\n");
      line = strtok(NULL, "\n");
      continue;
    } else if(in_blockquote) {
      out += sprintf(out, "</blockquote>\n");
      in_blockquote = false;
    }

    if(line[0] == '|' && strchr(line + 1, '|')) {
      if(!in_table) {
        out += sprintf(out, "<table>\n");
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
        line = strtok(NULL, "\n");
        continue;
      }

      out += sprintf(out, "<tr>");
      char* row = strdup(line);
      char* cell = strtok(row, "|");
      while(cell) {
        while(*cell == ' ')
          cell++;
        char* end = cell + strlen(cell) - 1;
        while(end > cell && *end == ' ')
          *end-- = '\0';

        out += sprintf(out, table_header_parsed ? "<td>" : "<th>");
        out = renderInline(cell, out);
        out += sprintf(out, table_header_parsed ? "</td>" : "</th>");
        cell = strtok(NULL, "|");
      }
      free(row);
      out += sprintf(out, "</tr>\n");
      table_header_parsed = true;
      line = strtok(NULL, "\n");
      continue;
    } else if(in_table) {
      out += sprintf(out, "</table>\n");
      in_table = false;
    }

    if(strncmp(line, "- ", 2) == 0 || strncmp(line, "* ", 2) == 0) {
      if(!in_list) {
        out += sprintf(out, "<ul>\n");
        in_list = true;
      }
      out += sprintf(out, "<li>");
      out = renderInline(line + 2, out);
      out += sprintf(out, "</li>\n");
      line = strtok(NULL, "\n");
      continue;
    } else if(in_list) {
      out += sprintf(out, "</ul>\n");
      in_list = false;
    }

    if(line[0] == '#') {
      int level = 0;
      while(line[level] == '#' && level < 6)
        level++;
      if(line[level] == ' ') {
        out += sprintf(out, "<h%d>", level);
        out = renderInline(line + level + 1, out);
        out += sprintf(out, "</h%d>\n", level);
        line = strtok(NULL, "\n");
        continue;
      }
    }

    if(*line != '\0') {
      out += sprintf(out, "<p>");
      out = renderInline(line, out);
      out += sprintf(out, "</p>\n");
    }

    line = strtok(NULL, "\n");
  }

  if(in_list)
    out += sprintf(out, "</ul>\n");
  if(in_table)
    out += sprintf(out, "</table>\n");
  if(in_blockquote)
    out += sprintf(out, "</blockquote>\n");
  if(in_codeblock)
    out += sprintf(out, "</code></pre>\n");

  free(input);
  return html;
}
