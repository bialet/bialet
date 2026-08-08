#ifndef SHOW_ERRORS_H
#define SHOW_ERRORS_H

#include "bialet.h"

#define SHOW_ERRORS_KEY "BIALET_SHOW_ERRORS"

void  show_errors_init(void);
int   show_errors_enabled(void);
void  show_errors_capture(const char* type, const char* module, int line,
                          const char* msg);
void  show_errors_clear(void);
char* show_errors_page(void);

#endif
