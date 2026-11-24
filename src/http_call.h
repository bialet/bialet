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
  long  timeout;
  long  connectTimeout;
};

struct HttpResponse {
  int   status;
  int   error;
  char* headers;
  char* body;
  char* error_message;
};

void http_call_init(struct BialetConfig* config);
/* Matching teardown for http_call_init (curl_global_cleanup / WSACleanup). */
void http_call_cleanup(void);
void http_call_perform(struct HttpRequest* req, struct HttpResponse* resp);

/*
 * Test-only outbound HTTP stubs (the -T runner).
 *
 * The registry lives in C rather than in Wren because every Test.get() request
 * runs in a fresh VM: a Wren-side registry could not be seen by a route
 * handler's Http call. Only the Test primitive populates it, so a production
 * run keeps the list empty and http_call_perform() behaves exactly as before.
 *
 * http_stub_add() takes a whole-URL glob pattern ('*' matches zero or more
 * characters, everything else is literal, first registered rule wins) plus the
 * canned response. It returns 0 on success; on failure it returns -1 and
 * http_stub_last_error() describes why. Every string is copied and bounded.
 */
int         http_stub_add(const char* pattern, int status, const char* headers,
                          const char* body, int error, const char* error_message);
void        http_stub_clear(void);
int         http_stub_calls_count(void);
const char* http_stub_call_url(int index);
const char* http_stub_call_method(int index);
const char* http_stub_call_headers(int index);
const char* http_stub_call_body(int index);
const char* http_stub_last_error(void);

#endif
