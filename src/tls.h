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
#ifndef TLS_H
#define TLS_H

#include <stddef.h>
#include <sys/types.h>

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET tls_socket_t;
#else
typedef int tls_socket_t;
#endif

/* All functions take and return opaque handles so server.c does not need to
 * know about OpenSSL types. When the build has no OpenSSL (no HAVE_SSL), every
 * function degrades to a stub: context creation returns NULL and the server
 * refuses to start with TLS enabled. */
void*   tls_context_create(const char* cert_path, const char* key_path);
void    tls_context_free(void* ctx);
void*   tls_accept(void* ctx, tls_socket_t fd);
ssize_t tls_read(void* ssl, void* buf, size_t count);
ssize_t tls_write(void* ssl, const void* buf, size_t count);
int     tls_pending(void* ssl);
void    tls_shutdown(void* ssl);
void    tls_free(void* ssl);

#endif
