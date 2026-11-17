#ifndef SHOW_ERRORS_H
#define SHOW_ERRORS_H

#include "bialet.h"

#define SHOW_ERRORS_KEY "BIALET_SHOW_ERRORS"

// Reads BIALET_SHOW_ERRORS from the database; [force_on] (bialet dev) enables
// it for this run without writing anything.
void  show_errors_init(int force_on);
int   show_errors_enabled(void);
void  show_errors_capture(const char* type, const char* module, int line,
                          const char* msg);
void  show_errors_clear(void);
char* show_errors_page(void);

#endif
