#ifndef BIALET_WREN_H
#define BIALET_WREN_H

struct BialetResponse {
  int status;
  char *header;
  char *body;
};

void bialetWrenInit();

struct BialetResponse runCode(char *module, char *code);

char* readFile(const char* path);

#endif
