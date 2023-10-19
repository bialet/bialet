#ifndef BIALET_CONFIG_H
#define BIALET_CONFIG_H

#include <stdio.h>

struct BialetConfig {
  char *root_dir;
  char *host;
  int port;

  FILE *log_file;
  int debug;
  int output_color;

  int mem_soft_limit, mem_hard_limit, cpu_soft_limit, cpu_hard_limit;

  char *db_path;
};

#endif
