#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>

char *safe_malloc(size_t size);
char *string_safe_copy(const char *zSrc);
char *string_append(char *zPrior, const char *zSep, const char *zSrc);

#endif
