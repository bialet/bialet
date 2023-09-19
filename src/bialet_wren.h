#ifndef BIALET_WREN_H
#define BIALET_WREN_H

struct BialetResponse {
  int status;
  char *header;
  char *body;
};

void bialet_init();

struct BialetResponse bialet_run(char *module, char *code);

char* bialet_read_file(const char* path);

#endif
