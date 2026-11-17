#ifndef LIVERELOAD_H
#define LIVERELOAD_H

#include "bialet.h"

#define LIVERELOAD_KEY "BIALET_LIVE_RELOAD"

// Reads BIALET_LIVE_RELOAD from the database; [force_on] (bialet dev) enables
// it for this run without writing anything.
void livereload_init(int force_on);
int  livereload_is_poll(const char* uri);
int  livereload_try_handle(const char* uri, struct BialetResponse* response);
void livereload_notify(void);
int  livereload_inject_response(struct BialetResponse* response);

#endif
