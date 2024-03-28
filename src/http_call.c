#include "http_call.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

#if IS_UNIX
#include <curl/curl.h>
#endif

#if IS_WIN
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

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

#if IS_WIN

void init_openssl() {
  SSL_load_error_strings();
  OpenSSL_add_ssl_algorithms();
}

void cleanup_openssl() { EVP_cleanup(); }

SSL_CTX *create_context() {
  const SSL_METHOD *method;
  SSL_CTX *ctx;

  method = TLS_client_method();

  ctx = SSL_CTX_new(method);
  if (!ctx) {
    perror("Unable to create SSL context");
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
  }

  return ctx;
}
#endif

void http_call_init(struct BialetConfig *config) {

#if IS_UNIX
  curl_global_init(CURL_GLOBAL_ALL);
#endif
}

struct HttpResponse http_call_perform(struct HttpRequest request) {
  response_buffer[0] = '\0';

  struct HttpResponse response;
  response.error = 0;
  response.status = 200;
  response.headers = "Content-Type: text/json\r\n";
  response.body = "{}";

#if IS_UNIX
  CURL *handle;
  CURLcode res;
  struct curl_slist *headers = NULL;

  const char *url = request.url;
  const char *method = request.method;
  const char *raw_headers = request.raw_headers;
  const char *postData = request.postData;
  const char *basicAuth = request.basicAuth;

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

#if IS_WIN
  WSADATA wsaData;
  SOCKET sockfd;
  struct addrinfo hints, *servinfo, *p;
  SSL_CTX *ctx;
  SSL *ssl;
  int rv;
  char *host = "example.com";
  char *path = "/postdata";
  char *message = "name=Albo&project=HTTP_Request";
  char req[1024], res[4096];

  // Initialize Winsock
  WSAStartup(MAKEWORD(2, 2), &wsaData);

  // Initialize OpenSSL
  init_openssl();
  ctx = create_context();

  // Set up hints structure
  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  // Get server info
  if ((rv = getaddrinfo(host, "443", &hints, &servinfo)) != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    WSACleanup();
    response.error = 1;
    return response;
  }

  // Loop through all the results and connect to the first we can
  for (p = servinfo; p != NULL; p = p->ai_next) {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) ==
        INVALID_SOCKET) {
      perror("client: socket");
      continue;
    }

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) == SOCKET_ERROR) {
      closesocket(sockfd);
      perror("client: connect");
      continue;
    }

    break; // if we get here, we must have connected successfully
  }

  if (p == NULL) {
    fprintf(stderr, "client: failed to connect\n");
    response.error = 2;
    return response;
  }

  // Create an SSL connection and attach it to the socket
  ssl = SSL_new(ctx);
  SSL_set_fd(ssl, sockfd);
  if (SSL_connect(ssl) != 1) {
    ERR_print_errors_fp(stderr);
    closesocket(sockfd);
    SSL_CTX_free(ctx);
    cleanup_openssl();
    WSACleanup();
    response.error = 3;
    return response;
  }

  // Construct the HTTPS POST request
  sprintf(req,
          "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: "
          "application/x-www-form-urlencoded\r\nContent-Length: %ld\r\n\r\n%s",
          path, host, (long)strlen(message), message);

  // Send the request through SSL
  if (SSL_write(ssl, req, strlen(req)) <= 0) {
    ERR_print_errors_fp(stderr);
    SSL_free(ssl);
    closesocket(sockfd);
    SSL_CTX_free(ctx);
    cleanup_openssl();
    WSACleanup();
    response.error = 4;
    return response;
  }

  // Receive the response
  ZeroMemory(res, sizeof(res));
  if (SSL_read(ssl, res, sizeof(res)) <= 0) {
    ERR_print_errors_fp(stderr);
  } else {
    // copy the response_buffer in the response struct
    response.body = string_safe_copy(res);
  }

  // Clean up
  SSL_free(ssl);
  closesocket(sockfd);
  freeaddrinfo(servinfo);
  SSL_CTX_free(ctx);
  cleanup_openssl();
  WSACleanup(); //

#endif

  return response;
}
