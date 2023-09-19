#ifndef BIALET_WREN_H
#define BIALET_WREN_H

#include "mongoose.h"

struct BialetResponse {
  int status;
  char *header;
  char *body;
};

void bialet_init(char *db_path);

struct BialetResponse bialet_run(char *module, char *code, struct mg_http_message *hm);

char* bialet_read_file(const char* path);

#endif
