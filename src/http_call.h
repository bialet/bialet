#ifndef HTTP_CALL_H
#define HTTP_CALL_H

#include "bialet.h"

#define MAX_RESPONSE_SIZE 4096

struct HttpRequest {
  char* raw_headers;
  char* url;
  char* method;
  char* postData;
  char* basicAuth;
};

struct HttpResponse {
  int   status;
  int   error;
  char* headers;
  char* body;
};

void httpCallInit(struct BialetConfig* config);
void httpCallPerform(struct HttpRequest* req, struct HttpResponse* resp);

#endif
