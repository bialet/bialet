/* Converts a .wren source file into a .wren.inc file: a C source fragment
 * defining a `static const char*` holding the Wren source as a string
 * literal, meant to be #included by another .c file. Replaces the old
 * tools/wren_to_c_string.py so the build no longer depends on python3. */

/* strdup/strndup are POSIX, not standard C; make sure they're declared
 * regardless of what standard $(HOSTCC) defaults to. */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *readAll(const char *path, long *outLen) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    perror(path);
    exit(1);
  }
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *buf = malloc((size_t)len + 1);
  size_t read = fread(buf, 1, (size_t)len, f);
  buf[read] = '\0';
  fclose(f);

  if (outLen) *outLen = (long)read;
  return buf;
}

/* Removes every occurrence of `needle` from `str` (in place, str must have
 * room). */
static void removeAll(char *str, const char *needle) {
  size_t needleLen = strlen(needle);
  char *p = str;
  while ((p = strstr(p, needle)) != NULL) {
    memmove(p, p + needleLen, strlen(p + needleLen) + 1);
  }
}

static char *moduleNameFromPath(const char *path) {
  const char *base = strrchr(path, '/');
  base = base ? base + 1 : path;

  char *module = strdup(base);
  char *dot = strrchr(module, '.');
  if (dot) *dot = '\0';

  removeAll(module, "opt_");
  removeAll(module, "wren_");
  return module;
}

static int isBlankLine(const char *line) {
  return line[0] == '\0';
}

static int isCommentLine(const char *line) {
  while (*line == ' ' || *line == '\t') line++;
  return line[0] == '/' && line[1] == '/';
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <output.wren.inc> <input.wren>\n", argv[0]);
    return 1;
  }
  const char *outputPath = argv[1];
  const char *inputPath = argv[2];

  long len;
  char *source = readAll(inputPath, &len);

  char *module = moduleNameFromPath(inputPath);

  FILE *out = fopen(outputPath, "wb");
  if (!out) {
    perror(outputPath);
    return 1;
  }

  fprintf(out, "// Generated automatically from %s. Do not edit.\n", inputPath);
  fprintf(out, "static const char* %sModuleSource =\n", module);

  /* Escaped output for a single line is at most 4x its length (each byte
   * can become \\, \", or contribute to \n), plus the wrapping quotes. */
  char *escaped = malloc((size_t)len * 4 + 8);

  int first = 1;
  char *lineStart = source;
  while (*lineStart != '\0') {
    char *newline = strchr(lineStart, '\n');
    size_t lineLen = newline ? (size_t)(newline - lineStart) : strlen(lineStart);

    char *line = strndup(lineStart, lineLen);

    if (!isBlankLine(line) && !isCommentLine(line)) {
      char *w = escaped;
      *w++ = '"';
      for (size_t i = 0; i < lineLen; i++) {
        char c = line[i];
        if (c == '\\' || c == '"') *w++ = '\\';
        *w++ = c;
      }
      *w++ = '\\';
      *w++ = 'n';
      *w++ = '"';
      *w = '\0';

      if (!first) fputc('\n', out);
      first = 0;
      fputs(escaped, out);
    }

    free(line);
    lineStart = newline ? newline + 1 : lineStart + lineLen;
  }

  fputs(";\n", out);

  free(escaped);
  free(module);
  free(source);
  fclose(out);
  return 0;
}
