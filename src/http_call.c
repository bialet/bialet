#include "http_call.h"

#include "utils.h"
#include <stdlib.h>
#include <string.h>

#if IS_UNIX
#include <curl/curl.h>
#endif

#if IS_WIN
#include <winsock2.h>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <ws2tcpip.h>

#endif

struct memory {
  char*  response;
  size_t size;
};

#if IS_UNIX
static size_t write_callback(void* data, size_t size, size_t nmemb, void* clientp) {
  size_t         realsize = size * nmemb;
  struct memory* mem = (struct memory*)clientp;

  char* ptr = realloc(mem->response, mem->size + realsize + 1);
  if(!ptr)
    return 0; /* out of memory! */

  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), data, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;

  return realsize;
}

static size_t header_callback(char* buffer, size_t size, size_t nitems,
                              void* userdata) {
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

void cleanup_openssl() {
  EVP_cleanup();
}

SSL_CTX* create_context() {
  const SSL_METHOD* method;
  SSL_CTX*          ctx;

  method = TLS_client_method();

  ctx = SSL_CTX_new(method);
  if(!ctx) {
    perror("Unable to create SSL context");
    ERR_print_errors_fp(stderr);
    exit(EXIT_FAILURE);
  }

  return ctx;
}

int parse_url(char* url, char** hostname, char** port, char** path) {
  char* p;
  *hostname = url;

  // Check for https:// or http:// prefix
  if(strncmp(url, "https://", 8) == 0) {
    *port = "443";
    *hostname += 8;
  } else if(strncmp(url, "http://", 7) == 0) {
    *port = "80";
    *hostname += 7;
  } else {
    return -1; // Unsupported protocol
  }

  p = strchr(*hostname, '/');
  if(p) {
    *p = '\0'; // Null-terminate hostname and set path
    *path = p + 1;
  } else {
    *path = "";
  }

  // Check for port in hostname
  p = strchr(*hostname, ':');
  if(p) {
    *p = '\0'; // Null-terminate hostname
    *port = p + 1;
  }

  return 0;
}

void parse_http_response(struct HttpResponse* res, char* fullResponse) {
  char* line;
  int   isBody = 0;
  char* headers = calloc(1, 1); // Allocate a single byte for null termination
  char* body = calloc(1, 1);    // Allocate a single byte for null termination

  // Use strtok to split the response by newlines
  line = strtok(fullResponse, "\n");
  while(line != NULL) {
    if(!isBody) {
      // Parse status line
      if(strstr(line, "HTTP") == line) { // This is the status line
        sscanf(line, "HTTP/1.1 %d", &res->status);
      } else if(strlen(line) <= 1) { // Empty line: headers end, body begins
        isBody = 1;
      } else { // Header line
        trim(line);
        headers = realloc(headers, strlen(headers) + strlen(line) +
                                       2); // +2 for newline and null terminator
        strcat(headers, line);
        strcat(headers, "\n");
      }
    } else {
      // Parse body
      body = realloc(body, strlen(body) + strlen(line) +
                               2); // +2 for newline and null terminator
      strcat(body, line);
      strcat(body, "\n");
    }
    line = strtok(NULL, "\n");
  }
  res->headers = headers;
  res->body = body;
}
#endif

void httpCallInit(struct BialetConfig* config) {
#if IS_UNIX
  curl_global_init(CURL_GLOBAL_ALL);
#endif
}

void httpCallPerform(struct HttpRequest* request, struct HttpResponse* response) {
#if IS_UNIX
  struct memory      chunk = {0};
  CURL*              handle;
  CURLcode           res;
  struct curl_slist* headers = NULL;

  const char* url = request->url;
  const char* method = request->method;
  const char* raw_headers = request->raw_headers;
  const char* postData = request->postData;
  const char* basicAuth = request->basicAuth;

  handle = curl_easy_init();
  if(!handle) {
    response->error = 1;
    return;
  }
  curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
  curl_easy_setopt(handle, CURLOPT_URL, url);
  curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method);
  /* Headers */
  char* header_string = strdup(raw_headers);
  char* header_line = strtok(header_string, "\n");
  while(header_line != NULL) {
    // Add each header line to the slist
    headers = curl_slist_append(headers, header_line);
    header_line = strtok(NULL, "\n");
  }
  free(header_string);
  curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

  if(basicAuth) {
    curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
    curl_easy_setopt(handle, CURLOPT_USERPWD, basicAuth);
  }

  if(strlen(postData) > 0) {
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
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, &chunk);
  curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, header_callback);

  res = curl_easy_perform(handle);

  response->body = string_safe_copy(chunk.response);

  /* Check for errors */
  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    response->error = 1;
  }

  /* always cleanup */
  free(chunk.response);
  curl_easy_cleanup(handle);
  curl_slist_free_all(headers);

#endif

#if IS_WIN
  char *hostname, *port, *path;
  if(parse_url(request->url, &hostname, &port, &path) != 0) {
    response->error = 1; // Error parsing URL
    return;
  }

  int use_ssl = strcmp(port, "443") == 0;

  // Initialize Winsock
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);

  // Create a SOCKET for connecting to server
  SOCKET          sockfd = INVALID_SOCKET;
  struct addrinfo hints, *result = NULL, *ptr = NULL;

  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  // Resolve the server address and port
  int iResult = getaddrinfo(hostname, port, &hints, &result);
  if(iResult != 0) {
    response->error = 2; // Error resolving hostname
    WSACleanup();
    return;
  }

  // Attempt to connect to the first address returned by
  // the call to getaddrinfo
  ptr = result;

  // Create a SOCKET for connecting to server
  sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
  if(sockfd == INVALID_SOCKET) {
    response->error = 3; // Socket creation failed
    freeaddrinfo(result);
    WSACleanup();
    return;
  }

  // Connect to server.
  iResult = connect(sockfd, ptr->ai_addr, (int)ptr->ai_addrlen);
  if(iResult == SOCKET_ERROR) {
    closesocket(sockfd);
    sockfd = INVALID_SOCKET;
    response->error = 4; // Connection failed
    freeaddrinfo(result);
    WSACleanup();
    return;
  }

  freeaddrinfo(result);

  SSL_CTX* ctx = NULL;
  SSL*     ssl = NULL;

  if(use_ssl) {
    // Initialize OpenSSL
    init_openssl();
    ctx = create_context();

    // Create an SSL connection and attach it to the socket
    ssl = SSL_new(ctx);
    if(ssl == NULL) {
      printf("SSL_new failed\n");
      response->error = 5; // SSL creation failed
      SSL_CTX_free(ctx);
      cleanup_openssl();
      WSACleanup();
      return;
    }
    SSL_set_fd(ssl, sockfd);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    int result = SSL_connect(ssl);
    if(result != 1) {
      int ssl_err = errno;
      printf("SSL_connect failed with error code: %d\n", SSL_get_error(ssl, result));
      printf("Captured errno: %d\n", ssl_err);
      response->error = 5; // SSL connection failed
      SSL_free(ssl);
      closesocket(sockfd);
      SSL_CTX_free(ctx);
      cleanup_openssl();
      WSACleanup();
      return;
    }
  }

  // Send an initial buffer
  char req[1024];
  sprintf(req, "%s /%s HTTP/1.1\r\nHost: %s\r\n%s\r\nContent-Length: %d\r\n\r\n%s",
          request->method, path, hostname,
          request->raw_headers ? request->raw_headers : "",
          request->postData ? (int)strlen(request->postData) : 0,
          request->postData ? request->postData : "");

  if(use_ssl) {
    if(SSL_write(ssl, req, strlen(req)) <= 0) {
      ERR_print_errors_fp(stderr);
      response->error = 6; // SSL write failed
    }
  } else {
    if(send(sockfd, req, (int)strlen(req), 0) == SOCKET_ERROR) {
      response->error = 7; // Send failed
    }
  }

  char res[4096 * 2];
  int  bytes_received;

  if(use_ssl) {
    bytes_received = SSL_read(ssl, res, sizeof(res) - 1);
  } else {
    bytes_received = recv(sockfd, res, sizeof(res) - 1, 0);
  }

  if(bytes_received < 0) {
    perror("recv failed");
    response->error = 8; // Receive failed
  } else {
    // Null-terminate the response
    res[bytes_received] = '\0';
    parse_http_response(response, res);
  }

  // Shutdown the connection since no more data will be sent
  if(use_ssl) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    cleanup_openssl();
  }

  closesocket(sockfd);
  WSACleanup();
#endif
}
