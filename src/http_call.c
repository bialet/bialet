#include "http_call.h"

#include "utils.h"
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <curl/curl.h>
#endif

#ifdef _WIN32
#include <winsock2.h>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <stdio.h>
#include <ws2tcpip.h>

#endif

// Hard ceiling on a single outbound HTTP response body/header set. The
// write/header callbacks below refuse to grow past it, so a malicious or
// misbehaving server cannot exhaust heap memory (or, in the remote-module
// loader path, amplify disk usage) through an unbounded response.
#define MAX_HTTP_RESPONSE_SIZE (50 * 1024 * 1024)

struct memory {
  char*  response;
  size_t size;
  size_t max_size;
};

#ifndef _WIN32
static size_t write_callback(void* data, size_t size, size_t nmemb, void* clientp) {
  size_t         realsize = size * nmemb;
  struct memory* mem = (struct memory*)clientp;

  // Abort the transfer (curl treats a 0 return as a write error) once the
  // configured cap would be exceeded, instead of growing without bound.
  if(mem->size + realsize > mem->max_size)
    return 0;

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
  size_t         realsize = nitems * size;
  struct memory* mem = (struct memory*)userdata;

  if(mem->size + realsize > mem->max_size)
    return 0;

  char* ptr = realloc(mem->response, mem->size + realsize + 1);
  if(!ptr)
    return 0; /* out of memory! */

  mem->response = ptr;
  memcpy(&(mem->response[mem->size]), buffer, realsize);
  mem->size += realsize;
  mem->response[mem->size] = 0;

  return realsize;
}
#endif

#ifdef _WIN32

/* Values reported through HttpResponse.error. The Wren side only tests
 * error == 0, but distinct codes stay useful in logs. */
#define HTTP_ERR_URL 1
#define HTTP_ERR_RESOLVE 2
#define HTTP_ERR_SOCKET 3
#define HTTP_ERR_CONNECT 4
#define HTTP_ERR_TLS 5
#define HTTP_ERR_TLS_WRITE 6
#define HTTP_ERR_SEND 7
#define HTTP_ERR_RECV 8
#define HTTP_ERR_REQUEST 9

struct ParsedUrl {
  char host[256];
  char port[8];
  char path[1024];
  int  is_https;
};

/* Splits [url] into its parts without modifying it.
 *
 * Two bugs are fixed here. The old parse_url wrote NUL bytes into the caller's
 * buffer to terminate the host and the path -- and the caller is wren_core.c
 * passing AS_CSTRING(), i.e. the live bytes of an interned Wren string. And it
 * reported TLS by comparing the port to "443", so "https://host:8443/" was
 * sent as plaintext to a TLS listener, leaking any Authorization header or
 * post body. The scheme now decides. */
static int parse_url(const char* url, struct ParsedUrl* out) {
  const char* p;
  memset(out, 0, sizeof(*out));

  if(strncmp(url, "https://", 8) == 0) {
    out->is_https = 1;
    snprintf(out->port, sizeof(out->port), "%s", "443");
    p = url + 8;
  } else if(strncmp(url, "http://", 7) == 0) {
    out->is_https = 0;
    snprintf(out->port, sizeof(out->port), "%s", "80");
    p = url + 7;
  } else {
    return -1; /* only http/https, matching the libcurl allowlist */
  }

  const char* slash = strchr(p, '/');
  const char* colon = strchr(p, ':');
  const char* host_end = slash;
  if(colon != NULL && (slash == NULL || colon < slash)) {
    host_end = colon;
    size_t plen = slash ? (size_t)(slash - colon - 1) : strlen(colon + 1);
    if(plen == 0 || plen >= sizeof(out->port))
      return -1;
    memcpy(out->port, colon + 1, plen);
    out->port[plen] = '\0';
  }
  size_t hlen = host_end ? (size_t)(host_end - p) : strlen(p);
  if(hlen == 0 || hlen >= sizeof(out->host))
    return -1;
  memcpy(out->host, p, hlen);
  out->host[hlen] = '\0';

  int written = snprintf(out->path, sizeof(out->path), "%s", slash ? slash : "/");
  if(written < 0 || written >= (int)sizeof(out->path))
    return -1;
  return 0;
}

/* OpenSSL >= 1.1 initializes itself on first use, so SSL_load_error_strings()
 * and OpenSSL_add_ssl_algorithms() are no-ops and EVP_cleanup() is a deprecated
 * *process-wide* teardown. Calling that once per HTTP request tore down global
 * library state underneath any other user of OpenSSL in the process. Nothing is
 * initialized or cleaned up per request now.
 *
 * Verification is configured on the CTX here, i.e. BEFORE SSL_new(): an SSL
 * object snapshots the CTX verify mode at creation time, so the old code --
 * which called SSL_CTX_set_verify() and SSL_CTX_set_default_verify_paths()
 * *after* SSL_new() -- verified nothing at all. Every Windows HTTPS call,
 * including the remote-module loader that then executes what it downloads, was
 * trivially MITM-able. */
static SSL_CTX* create_context(void) {
  SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
  if(!ctx) {
    ERR_print_errors_fp(stderr);
    return NULL; /* was exit(EXIT_FAILURE) from inside a request */
  }
  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
  if(!SSL_CTX_set_default_verify_paths(ctx)) {
    ERR_print_errors_fp(stderr);
    SSL_CTX_free(ctx);
    return NULL;
  }
  SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
  return ctx;
}

/* 1 when [host] is a bare IPv4/IPv6 literal, which must not be sent as SNI. */
static int host_is_ip_literal(const char* host) {
  for(const char* c = host; *c; c++) {
    if(!((*c >= '0' && *c <= '9') || *c == '.' || *c == ':'))
      return 0;
  }
  return 1;
}

/* Splits the raw response at the CRLFCRLF header terminator and copies each
 * half verbatim.
 *
 * The old version ran strtok_r(fullResponse, "\n") over the whole buffer, which
 * collapsed consecutive newlines (so blank lines inside a body disappeared),
 * stopped at the first NUL, re-joined every line with a bare \n, and only
 * recognised a literal "HTTP/1.1 " status line. */
static void parse_http_response(struct HttpResponse* res, const char* raw,
                                size_t len) {
  /* Initialized so a malformed status line cannot leave it stale for the
   * caller (e.g. the remote-module loader, which gates on 2xx). */
  res->status = 0;

  /* Status line: accept any HTTP version, then the first 3-digit code. */
  if(len >= 12 && memcmp(raw, "HTTP/", 5) == 0) {
    const char* sp = memchr(raw, ' ', len);
    if(sp != NULL && (size_t)(sp - raw) + 4 <= len) {
      int code = 0;
      int digits = 0;
      for(const char* d = sp + 1; d < raw + len && *d >= '0' && *d <= '9'; d++) {
        code = code * 10 + (*d - '0');
        if(++digits == 3)
          break;
      }
      if(digits == 3)
        res->status = code;
    }
  }

  size_t hdr_len = len;
  size_t body_off = len;
  for(size_t i = 0; i + 4 <= len; i++) {
    if(memcmp(raw + i, "\r\n\r\n", 4) == 0) {
      hdr_len = i + 2; /* keep the CRLF that ends the last header line */
      body_off = i + 4;
      break;
    }
  }

  char*  headers = (char*)malloc(hdr_len + 1);
  size_t body_len = len - body_off;
  char*  body = (char*)malloc(body_len + 1);
  if(headers == NULL || body == NULL) {
    free(headers);
    free(body);
    return;
  }
  memcpy(headers, raw, hdr_len);
  headers[hdr_len] = '\0';
  memcpy(body, raw + body_off, body_len);
  body[body_len] = '\0';

  free(res->headers);
  free(res->body);
  res->headers = headers;
  res->body = body;
}

/* Builds the request into a heap buffer. The old code formatted into a fixed
 * char[1024] and failed the call outright for anything larger, and with an
 * empty raw_headers it emitted "...Host: h\r\n" + "" + "\r\n", terminating the
 * headers early so that "Content-Length: 0" landed in the body. */
static char* build_request(const struct ParsedUrl*   url,
                           const struct HttpRequest* request, size_t* out_len) {
  const char* method = request->method ? request->method : "GET";
  const char* raw_headers = request->raw_headers ? request->raw_headers : "";
  const char* postData = request->postData ? request->postData : "";
  size_t      post_len = strlen(postData);

  /* Ensure the caller's header block is CRLF-terminated before ours. */
  size_t hdr_len = strlen(raw_headers);
  int    needs_crlf =
      hdr_len > 0 && !(hdr_len >= 2 && raw_headers[hdr_len - 2] == '\r' &&
                       raw_headers[hdr_len - 1] == '\n');

  int head_len = snprintf(
      NULL, 0, "%s %s HTTP/1.1\r\nHost: %s\r\n%s%sContent-Length: %zu\r\n\r\n",
      method, url->path, url->host, raw_headers, needs_crlf ? "\r\n" : "", post_len);
  if(head_len < 0)
    return NULL;

  char* req = (char*)malloc((size_t)head_len + post_len + 1);
  if(req == NULL)
    return NULL;
  snprintf(req, (size_t)head_len + 1,
           "%s %s HTTP/1.1\r\nHost: %s\r\n%s%sContent-Length: %zu\r\n\r\n", method,
           url->path, url->host, raw_headers, needs_crlf ? "\r\n" : "", post_len);
  /* memcpy rather than a %s in the format: the body may contain NUL bytes. */
  memcpy(req + head_len, postData, post_len);
  *out_len = (size_t)head_len + post_len;
  req[*out_len] = '\0';
  return req;
}
#endif

void http_call_init(struct BialetConfig* config) {
  (void)config;
#ifndef _WIN32
  if(curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
    fprintf(stderr, "curl_global_init failed\n");
  }
#else
  /* Once at startup instead of once per request: the old code paired
   * WSAStartup/WSACleanup inside http_call_perform, tearing down Winsock
   * refcounts from a request path while the server socket was live. */
  WSADATA wsaData;
  if(WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    fprintf(stderr, "WSAStartup failed in http_call_init\n");
  }
#endif
}

void http_call_cleanup(void) {
#ifndef _WIN32
  /* Matching teardown for curl_global_init; previously never called. */
  curl_global_cleanup();
#else
  WSACleanup();
#endif
}

void http_call_perform(struct HttpRequest* request, struct HttpResponse* response) {
#ifndef _WIN32
  struct memory      chunk = {0, 0, MAX_HTTP_RESPONSE_SIZE};
  struct memory      header_chunk = {0, 0, MAX_HTTP_RESPONSE_SIZE};
  CURL*              handle;
  CURLcode           res;
  struct curl_slist* headers = NULL;
  long               http_code = 0;

  const char* url = request->url;
  const char* method = request->method ? request->method : "GET";
  /* NULL-safe: strdup(NULL) and strlen(NULL) are both undefined behavior, and
   * these fields come straight from Wren, where a caller can leave them unset. */
  const char* raw_headers = request->raw_headers ? request->raw_headers : "";
  const char* postData = request->postData ? request->postData : "";
  const char* basicAuth = request->basicAuth;
  long        timeout = request->timeout > 0 ? request->timeout : 20000L;
  long        connectTimeout =
      request->connectTimeout > 0 ? request->connectTimeout : 2000L;

  handle = curl_easy_init();
  if(!handle) {
    response->error = 1;
    response->error_message = string_safe_copy("Failed to init curl");
    return;
  }
  curl_easy_setopt(handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS);
  curl_easy_setopt(handle, CURLOPT_URL, url);
  curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method);
  /* Restrict both the request and any redirect to http/https. Without this a
   * Wren app could read local files through file://, and curl's redirect
   * defaults also permit ftp/ftps -- so a redirect could move a request onto a
   * scheme the app never asked for. Certificate and hostname verification are
   * stated explicitly rather than relied on as library defaults. */
  /* The *_STR forms arrived in libcurl 7.85; Ubuntu 22.04 still ships 7.81, so
   * fall back to the deprecated bitmask options there. */
#if LIBCURL_VERSION_NUM >= 0x075500
  curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "http,https");
  curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
  curl_easy_setopt(handle, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
  curl_easy_setopt(handle, CURLOPT_REDIR_PROTOCOLS,
                   CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
  curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);
  /* Headers */
  char* header_string = strdup(raw_headers);
  if(header_string == NULL) {
    response->error = 1;
    response->error_message = string_safe_copy("Out of memory building headers");
    curl_easy_cleanup(handle);
    return;
  }
  char* saveptr = NULL;
  char* header_line = strtok_r(header_string, "\n", &saveptr);
  while(header_line != NULL) {
    // Add each header line to the slist
    headers = curl_slist_append(headers, header_line);
    header_line = strtok_r(NULL, "\n", &saveptr);
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
  curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
  /* Redirects are restricted to http/https above, not here. */
  curl_easy_setopt(handle, CURLOPT_AUTOREFERER, 1L);
  curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 10L);
  /* each transfer needs to be done within this many milliseconds */
  curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, timeout);
  /* connect fast or fail */
  curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, connectTimeout);
  /* Speed up the connection using IPv4 only */
  curl_easy_setopt(handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, &chunk);
  curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, header_callback);
  curl_easy_setopt(handle, CURLOPT_HEADERDATA, &header_chunk);
  /* hard cap on the transfer size; the callbacks enforce it too */
  curl_easy_setopt(handle, CURLOPT_MAXFILESIZE_LARGE,
                   (curl_off_t)MAX_HTTP_RESPONSE_SIZE);

  res = curl_easy_perform(handle);

  response->body =
      chunk.response ? string_safe_copy(chunk.response) : string_safe_copy("");
  response->headers = header_chunk.response ? string_safe_copy(header_chunk.response)
                                            : string_safe_copy("");

  /* Get HTTP status code */
  curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &http_code);
  response->status = (int)http_code;

  /* Check for errors */
  if(res != CURLE_OK) {
    fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    response->error = 1;
    response->error_message = string_safe_copy(curl_easy_strerror(res));
  }

  /* always cleanup */
  free(chunk.response);
  free(header_chunk.response);
  curl_easy_cleanup(handle);
  curl_slist_free_all(headers);

#endif

#ifdef _WIN32
  struct ParsedUrl url;
  SOCKET           sockfd = INVALID_SOCKET;
  struct addrinfo  hints, *result = NULL, *ptr = NULL;
  SSL_CTX*         ctx = NULL;
  SSL*             ssl = NULL;
  char*            req = NULL;
  char*            raw = NULL;

  /* Every exit below funnels through `done:` so no socket, SSL object, CTX or
   * heap buffer is leaked on an error path. */
  if(parse_url(request->url, &url) != 0) {
    response->error = HTTP_ERR_URL;
    response->error_message = string_safe_copy("Unsupported or malformed URL");
    return;
  }

  ZeroMemory(&hints, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  if(getaddrinfo(url.host, url.port, &hints, &result) != 0) {
    response->error = HTTP_ERR_RESOLVE;
    response->error_message = string_safe_copy("Could not resolve host");
    return;
  }

  long connect_ms = request->connectTimeout > 0 ? request->connectTimeout : 2000L;
  long timeout_ms = request->timeout > 0 ? request->timeout : 20000L;

  /* Try every address getaddrinfo returned, not just the first. With
   * AF_UNSPEC the first record is often IPv6, so a v4-only host failed
   * outright before. */
  for(ptr = result; ptr != NULL; ptr = ptr->ai_next) {
    sockfd = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if(sockfd == INVALID_SOCKET)
      continue;

    /* Bound connect() by connectTimeout: a blocking connect can hang the
     * request-handling thread against an unreachable remote. */
    u_long nonblocking = 1;
    ioctlsocket(sockfd, FIONBIO, &nonblocking);
    int iResult = connect(sockfd, ptr->ai_addr, (int)ptr->ai_addrlen);
    if(iResult == SOCKET_ERROR) {
      if(WSAGetLastError() != WSAEWOULDBLOCK) {
        closesocket(sockfd);
        sockfd = INVALID_SOCKET;
        continue;
      }
      fd_set writefds, exceptfds;
      FD_ZERO(&writefds);
      FD_ZERO(&exceptfds);
      FD_SET(sockfd, &writefds);
      FD_SET(sockfd, &exceptfds);
      struct timeval tv;
      tv.tv_sec = connect_ms / 1000;
      tv.tv_usec = (connect_ms % 1000) * 1000;
      int sel = select(0, NULL, &writefds, &exceptfds, &tv);
      int so_error = 0;
      int err_len = (int)sizeof(so_error);
      if(sel > 0)
        getsockopt(sockfd, SOL_SOCKET, SO_ERROR, (char*)&so_error, &err_len);
      else
        so_error = -1; /* select() timed out or errored */
      if(so_error != 0) {
        closesocket(sockfd);
        sockfd = INVALID_SOCKET;
        continue;
      }
    }
    /* Restore blocking I/O and bound send/recv by the overall timeout, the
     * Windows equivalent of CURLOPT_TIMEOUT_MS. These also keep
     * SSL_connect()/SSL_read() from hanging on a stalled peer. */
    nonblocking = 0;
    ioctlsocket(sockfd, FIONBIO, &nonblocking);
    DWORD tmo = (DWORD)timeout_ms;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tmo, sizeof(tmo));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tmo, sizeof(tmo));
    break;
  }
  freeaddrinfo(result);
  result = NULL;

  if(sockfd == INVALID_SOCKET) {
    response->error = HTTP_ERR_CONNECT;
    response->error_message = string_safe_copy("Connection failed or timed out");
    goto done;
  }

  if(url.is_https) {
    ctx = create_context(); /* verify mode set on the CTX before SSL_new */
    if(ctx == NULL) {
      response->error = HTTP_ERR_TLS;
      response->error_message = string_safe_copy("Could not create TLS context");
      goto done;
    }
    ssl = SSL_new(ctx);
    if(ssl == NULL) {
      response->error = HTTP_ERR_TLS;
      response->error_message = string_safe_copy("Could not create TLS session");
      goto done;
    }
    SSL_set_fd(ssl, (int)sockfd);

    /* SNI: without it a virtual-hosted server cannot pick a certificate. */
    if(!host_is_ip_literal(url.host))
      SSL_set_tlsext_host_name(ssl, url.host);

    /* Hostname verification. Chain validity alone does not bind a certificate
     * to the host we asked for, so without this any CA-valid certificate for
     * any domain would be accepted. */
    SSL_set_hostflags(ssl, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
    if(!SSL_set1_host(ssl, url.host)) {
      response->error = HTTP_ERR_TLS;
      response->error_message = string_safe_copy("Could not set TLS hostname");
      goto done;
    }

    if(SSL_connect(ssl) != 1) {
      ERR_print_errors_fp(stderr); /* surface *why* the handshake failed */
      response->error = HTTP_ERR_TLS;
      response->error_message =
          string_safe_copy("TLS handshake or certificate verification failed");
      goto done;
    }
  }

  size_t req_len = 0;
  req = build_request(&url, request, &req_len);
  if(req == NULL) {
    response->error = HTTP_ERR_REQUEST;
    response->error_message = string_safe_copy("Could not build request");
    goto done;
  }

  /* Write the whole request: a single send()/SSL_write() may accept only part
   * of it, which silently truncated large post bodies. */
  size_t sent = 0;
  while(sent < req_len) {
    int n = url.is_https ? SSL_write(ssl, req + sent, (int)(req_len - sent))
                         : send(sockfd, req + sent, (int)(req_len - sent), 0);
    if(n <= 0) {
      if(url.is_https)
        ERR_print_errors_fp(stderr);
      response->error = url.is_https ? HTTP_ERR_TLS_WRITE : HTTP_ERR_SEND;
      response->error_message = string_safe_copy("Failed to send request");
      goto done;
    }
    sent += (size_t)n;
  }

  /* Read the whole response. A single SSL_read()/recv() returns at most one
   * record or segment, so every response larger than that was silently
   * truncated. Capped at MAX_HTTP_RESPONSE_SIZE, matching the libcurl path. */
  size_t cap = 16 * 1024;
  size_t len = 0;
  raw = (char*)malloc(cap);
  if(raw == NULL) {
    response->error = HTTP_ERR_RECV;
    response->error_message = string_safe_copy("Out of memory reading response");
    goto done;
  }
  for(;;) {
    if(len + 4096 + 1 > cap) {
      if(cap >= MAX_HTTP_RESPONSE_SIZE)
        break; /* hard ceiling reached; keep what we have */
      size_t ncap = cap * 2;
      if(ncap > MAX_HTTP_RESPONSE_SIZE)
        ncap = MAX_HTTP_RESPONSE_SIZE;
      char* tmp = (char*)realloc(raw, ncap);
      if(tmp == NULL)
        break;
      raw = tmp;
      cap = ncap;
    }
    int n = url.is_https ? SSL_read(ssl, raw + len, (int)(cap - len - 1))
                         : recv(sockfd, raw + len, (int)(cap - len - 1), 0);
    if(n <= 0)
      break; /* clean EOF, timeout or error */
    len += (size_t)n;
  }
  raw[len] = '\0';
  parse_http_response(response, raw, len);

done:
  free(raw);
  free(req);
  if(ssl != NULL) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
  }
  if(ctx != NULL)
    SSL_CTX_free(ctx);
  if(sockfd != INVALID_SOCKET)
    closesocket(sockfd);
  /* No WSACleanup() here: Winsock is started once in http_call_init(). */
  if(response->body == NULL)
    response->body = string_safe_copy("");
  if(response->headers == NULL)
    response->headers = string_safe_copy("");
#endif
}
