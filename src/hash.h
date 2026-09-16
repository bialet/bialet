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
#ifndef HASH_H
#define HASH_H

#include <stddef.h>

#ifdef HAVE_SSL
#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#define OPENSSL_OK 1
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#endif
#endif

#define SALT_LENGTH 16
#define HASH_LENGTH 64
/* Large enough for the current format, "pbkdf2$<iters>$<32 hex salt>$<64 hex
 * hash>" (111 bytes), as well as both legacy formats. */
#define HASH_AND_SALT_LENGTH 128

// Fills [buf] with [len] cryptographically random bytes from the OS CSPRNG
// (same source used for password salts). Hard-fails on entropy errors rather
// than degrading to a predictable fallback.
void random_bytes_fill(unsigned char* buf, size_t len);

// SHA-256 of [data] ([len] bytes, which may contain NUL). [out] must be at
// least 32 bytes. Self-contained, deliberately not gated on OpenSSL: PKCE and
// JWT/OIDC need it in every build, so it has no degraded fallback.
void sha256_raw(const char* data, size_t len, unsigned char* out);

// Lowercase hex SHA-256 of [data] ([len] bytes). [out] must be at least 65
// bytes. Hashes exactly [len] bytes (not strlen), so embedded NULs count and
// leading zeros are preserved (unlike Util.toHex).
void sha256_hex(const char* data, size_t len, char* out);

int  verify_password(char* password, char* hash_and_salt);
void hash_password(char* password, char* output);

#endif
