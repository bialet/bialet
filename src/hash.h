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

int  verify_password(const char* password, size_t password_len,
                     const char* hash_and_salt);
void hash_password(const char* password, size_t password_len, char* output);

// Zeroes [len] bytes at [ptr] in a way the optimizer cannot elide. Used for
// derived keys and for plaintext that turned out to be forged.
void secure_zero(void* ptr, size_t len);

// PBKDF2-HMAC-SHA256 over an arbitrary salt, for callers that need the raw
// derived bytes (encryption) rather than the self-describing password format.
// Returns 1 on success. No fallback: a build without OpenSSL returns 0, so
// encryption is unavailable rather than quietly weaker.
int pbkdf2_sha256(const char* password, size_t password_len,
                  const unsigned char* salt, size_t salt_len,
                  unsigned int iterations, unsigned char* out, size_t out_len);

// 1 when this build can encrypt at all (OpenSSL 3 or newer).
int crypto_available(void);

// AES-256-GCM with a random nonce and a per-message salt. Returns a malloc'd,
// NUL-terminated envelope:
//
//   bgcm1$<iterations>$<salt hex|->$<iv hex>$<ciphertext||tag hex>
//
// [secret] is either a 64-hex-character raw AES-256 key (iterations 0, no KDF)
// or a passphrase (PBKDF2, iterations recorded in the envelope). [aad] is
// authenticated but not stored; pass NULL/0 for none. Returns NULL on failure.
char* crypto_encrypt(const char* secret, size_t secret_len, const char* aad,
                     size_t aad_len, const char* plaintext, size_t plaintext_len);

// Verifies and decrypts an envelope from crypto_encrypt under the same [aad].
// Returns a malloc'd buffer with its length in [*out_len] (and a trailing NUL
// for convenience; the plaintext itself may contain NUL bytes). Returns NULL
// for a malformed envelope, a wrong secret, or a tag mismatch -- and gives the
// caller no way to tell which, so a failure is not an oracle.
unsigned char* crypto_decrypt(const char* secret, size_t secret_len, const char* aad,
                              size_t aad_len, const char* envelope,
                              size_t envelope_len, size_t* out_len);

// DJB2 hex digest of exactly [len] bytes, for callers that already know the
// length (a Wren string can hold NUL). unsafe_hash() is the strlen wrapper kept
// for the existing call sites.
void unsafe_hash(const char* input, char* output);
void unsafe_hash_len(const char* input, size_t len, char* output);

#endif
