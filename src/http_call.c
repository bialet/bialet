#include "http_call.h"
#include <stdlib.h>
#include <string.h>

#if IS_UNIX
#include <curl/curl.h>
#endif

char response_buffer[MAX_RESPONSE_SIZE];

#if IS_UNIX
static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             void *userp) {
  size_t total_size = size * nmemb;
  strncat((char *)userp, contents, total_size);
  return total_size;
}

static size_t header_callback(char *buffer, size_t size, size_t nitems,
                              void *userdata) {
  /* received header is nitems * size long in 'buffer' NOT ZERO TERMINATED */
  /* 'userdata' is set with CURLOPT_HEADERDATA */
  /* @TODO Get response headers and HTTP status */
  return nitems * size;
}
#endif

void http_call_init(struct BialetConfig *config) {

#if IS_UNIX
  curl_global_init(CURL_GLOBAL_ALL);
#endif
}

struct HttpResponse http_call_perform(struct HttpRequest req) {
  response_buffer[0] = '\0';

  struct HttpResponse response;
  response.status = 200;
  response.headers = "Content-Type: text/json\r\n";
  response.body = "{}";

#if IS_UNIX
  CURL *handle;
  CURLcode res;
  struct curl_slist *headers = NULL;

  const char *url = req.url;
  const char *method = req.method;
  const char *raw_headers = req.raw_headers;
  const char *postData = req.postData;
  const char *basicAuth = req.basicAuth;

  handle = curl_easy_init();
  if (handle) {
    curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method);
    /* Headers */
    char *header_string = strdup(raw_headers);
    char *header_line = strtok(header_string, "\n");
    while (header_line != NULL) {
      // Add each header line to the slist
      headers = curl_slist_append(headers, header_line);
      header_line = strtok(NULL, "\n");
    }
    free(header_string);
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

    if (basicAuth) {
      curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
      curl_easy_setopt(handle, CURLOPT_USERPWD, basicAuth);
    }

    if (strlen(postData) > 0) {
      curl_easy_setopt(handle, CURLOPT_POSTFIELDS, postData);
    }
    /* For completeness */
    curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
    /* only allow redirects to HTTP and HTTPS URLs */
    curl_easy_setopt(handle, CURLOPT_AUTOREFERER, 1L);
    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 10L);
    /* each transfer needs to be done within 20 seconds! */
    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, 20000L);
    /* connect fast or fail */
    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, 2000L);
    /* Speed up the connection using IPv4 only */
    curl_easy_setopt(handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response_buffer);
    curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, header_callback);

    res = curl_easy_perform(handle);
    /* Check for errors */
    if (res != CURLE_OK)
      fprintf(stderr, "curl_easy_perform() failed: %s\n",
              curl_easy_strerror(res));

    /* always cleanup */
    curl_easy_cleanup(handle);
    curl_slist_free_all(headers);
  }
#endif

  return response;
}
