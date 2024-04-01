#include "utils.h"
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

char *safe_malloc(size_t size) {
  char *p;

  p = (char *)malloc(size);
  if (p == 0) {
    exit(1);
  }
  return p;
}

char *string_safe_copy(const char *zSrc) {
  char *zDest;
  size_t size;

  if (zSrc == 0)
    return 0;
  size = strlen(zSrc) + 1;
  zDest = (char *)safe_malloc(size);
  strcpy(zDest, zSrc);
  return zDest;
}

char *string_append(char *zPrior, const char *zSep, const char *zSrc) {
  char *zDest;
  size_t size;
  size_t n0, n1, n2;

  if (zSrc == 0)
    return 0;
  if (zPrior == 0)
    return string_safe_copy(zSrc);
  n0 = strlen(zPrior);
  n1 = strlen(zSep);
  n2 = strlen(zSrc);
  size = n0 + n1 + n2 + 1;
  zDest = (char *)safe_malloc(size);
  memcpy(zDest, zPrior, n0);
  free(zPrior);
  memcpy(&zDest[n0], zSep, n1);
  memcpy(&zDest[n0 + n1], zSrc, n2 + 1);
  return zDest;
}

void trim(char *str) {
  char *end;

  // Trim leading space
  while (isspace((unsigned char)*str))
    str++;

  // All spaces?
  if (*str == 0) {
    *str = '\0'; // If the string only had spaces, set it to an empty string
    return;
  }

  // Trim trailing space
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;

  // Write new null terminator character
  end[1] = '\0';
}
