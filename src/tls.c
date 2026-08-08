/*
 * This file is part of Bialet, which is licensed under the
 * MIT License.
 *
 * Copyright (c) 2023-2026 Rodrigo Arce
 *
 * SPDX-License-Identifier: MIT
 *
 * For full license text, see LICENSE.md.
 */
#include "tls.h"

#include <stdio.h>

#ifdef HAVE_SSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

// Native TLS (HTTPS) support backed by OpenSSL.
//
// The Makefile defines HAVE_SSL when a usable OpenSSL is found at build time.
// Without it these functions degrade to stubs: tls_context_create() returns
// NULL and start_server() refuses to launch with -s, so the binary still
// builds and runs in plain HTTP mode on systems without OpenSSL.

void* tls_context_create(const char* cert_path, const char* key_path) {
#ifdef HAVE_SSL
  if(cert_path == NULL || key_path == NULL)
    return NULL;
  SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
  if(ctx == NULL)
    return NULL;
  if(SSL_CTX_use_certificate_chain_file(ctx, cert_path) != 1) {
    fprintf(stderr, "Error: cannot load TLS certificate: %s\n", cert_path);
    SSL_CTX_free(ctx);
    return NULL;
  }
  if(SSL_CTX_use_PrivateKey_file(ctx, key_path, SSL_FILETYPE_PEM) != 1) {
    fprintf(stderr, "Error: cannot load TLS private key: %s\n", key_path);
    SSL_CTX_free(ctx);
    return NULL;
  }
  if(SSL_CTX_check_private_key(ctx) != 1) {
    fprintf(stderr, "Error: TLS private key does not match certificate: %s\n",
            cert_path);
    SSL_CTX_free(ctx);
    return NULL;
  }
  return ctx;
#else
  (void)cert_path;
  (void)key_path;
  return NULL;
#endif
}

void tls_context_free(void* ctx) {
#ifdef HAVE_SSL
  if(ctx != NULL)
    SSL_CTX_free((SSL_CTX*)ctx);
#else
  (void)ctx;
#endif
}

// Performs the TLS handshake on an accepted socket. The socket already has
// SO_RCVTIMEO/SO_SNDTIMEO applied by the caller, so a stalled handshake is
// bounded instead of parking the single-threaded accept loop.
void* tls_accept(void* ctx_v, tls_socket_t fd) {
#ifdef HAVE_SSL
  SSL_CTX* ctx = (SSL_CTX*)ctx_v;
  if(ctx == NULL)
    return NULL;
  SSL* ssl = SSL_new(ctx);
  if(ssl == NULL)
    return NULL;
  if(SSL_set_fd(ssl, (int)fd) != 1) {
    SSL_free(ssl);
    return NULL;
  }
  if(SSL_accept(ssl) != 1) {
    // Log one concise line instead of the full OpenSSL error stack. A plain
    // HTTP request to the HTTPS port (old browser tab, health check, port
    // scan) would otherwise spam every line of ssl/record/*.c on each retry.
    unsigned long err = ERR_peek_error();
    if(err != 0) {
      char err_buf[256];
      ERR_error_string_n(err, err_buf, sizeof(err_buf));
      fprintf(stderr, "TLS handshake failed: %s\n", err_buf);
    } else {
      fprintf(stderr, "TLS handshake failed\n");
    }
    SSL_free(ssl);
    return NULL;
  }
  return ssl;
#else
  (void)ctx_v;
  (void)fd;
  return NULL;
#endif
}

ssize_t tls_read(void* ssl_v, void* buf, size_t count) {
#ifdef HAVE_SSL
  return SSL_read((SSL*)ssl_v, buf, (int)count);
#else
  (void)ssl_v;
  (void)buf;
  (void)count;
  return -1;
#endif
}

ssize_t tls_write(void* ssl_v, const void* buf, size_t count) {
#ifdef HAVE_SSL
  return SSL_write((SSL*)ssl_v, buf, (int)count);
#else
  (void)ssl_v;
  (void)buf;
  (void)count;
  return -1;
#endif
}

int tls_pending(void* ssl_v) {
#ifdef HAVE_SSL
  return SSL_pending((SSL*)ssl_v);
#else
  (void)ssl_v;
  return 0;
#endif
}

void tls_shutdown(void* ssl_v) {
#ifdef HAVE_SSL
  if(ssl_v != NULL)
    SSL_shutdown((SSL*)ssl_v);
#else
  (void)ssl_v;
#endif
}

void tls_free(void* ssl_v) {
#ifdef HAVE_SSL
  if(ssl_v != NULL)
    SSL_free((SSL*)ssl_v);
#else
  (void)ssl_v;
#endif
}
