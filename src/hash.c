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
#include "hash.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#else
#define _CRT_RAND_S // Must be defined before stdlib.h is first included.
#include <windows.h>
#endif

#include <stdlib.h>

// Number of characters unsafe_hash() actually writes: four %08x values.
#define UNSAFE_HASH_HEX_LEN 32

void unsafe_hash(const char* input, char* output) {
  unsigned int hash = 5381;
  int          c;
  while((c = *input++))
    hash = ((hash << 5) + hash) + c; // hash * 33 + c
  snprintf(output, HASH_LENGTH + 1, "%08x%08x%08x%08x", hash, hash * 3, hash * 5,
           hash * 7);
}

void generate_salt(char* salt, size_t length) {
  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  size_t filled = 0;

  // Salts must come from a CSPRNG. Never fall back to rand()/time-seeded
  // values: a predictable salt makes password hashes offline-regenerable, so
  // a failing OS entropy source is a hard error rather than a weak fallback.
#ifndef _WIN32
  int fd = open("/dev/urandom", O_RDONLY);
  if(fd < 0) {
    perror("Failed to open /dev/urandom for salt generation");
    exit(EXIT_FAILURE);
  }
  unsigned char buf[64];
  while(filled < length) {
    ssize_t n = read(fd, buf, sizeof(buf));
    if(n < 0) {
      perror("Failed to read /dev/urandom for salt generation");
      close(fd);
      exit(EXIT_FAILURE);
    }
    if(n == 0)
      continue; // block/retry instead of filling the salt with weak bytes
    for(ssize_t i = 0; i < n && filled < length; i++)
      salt[filled++] = alphabet[buf[i] % 62];
  }
  close(fd);
#else
  // Windows: rand_s is the CSPRNG. Fail hard rather than silently using rand().
  while(filled < length) {
    unsigned int r = 0;
    if(rand_s(&r) != 0) {
      perror("rand_s failed for salt generation");
      exit(EXIT_FAILURE);
    }
    salt[filled++] = alphabet[r % 62];
  }
#endif
  salt[length] = '\0';
}

// Cryptographically random bytes from the OS CSPRNG. This is the source shared
// by password salts (generate_salt above) and, since SQLite's sqlite3_randomness
// is a documented non-cryptographic PRNG, by session IDs and CSRF tokens.
void random_bytes_fill(unsigned char* buf, size_t len) {
#ifndef _WIN32
  int fd = open("/dev/urandom", O_RDONLY);
  if(fd < 0) {
    perror("Failed to open /dev/urandom for random bytes");
    exit(EXIT_FAILURE);
  }
  size_t got = 0;
  while(got < len) {
    ssize_t n = read(fd, buf + got, len - got);
    if(n < 0) {
      perror("Failed to read /dev/urandom for random bytes");
      close(fd);
      exit(EXIT_FAILURE);
    }
    if(n == 0)
      continue; // block/retry instead of returning weak bytes
    got += (size_t)n;
  }
  close(fd);
#else
  size_t filled = 0;
  while(filled < len) {
    unsigned int r = 0;
    if(rand_s(&r) != 0) {
      perror("rand_s failed for random bytes");
      exit(EXIT_FAILURE);
    }
    buf[filled++] = (unsigned char)(r & 0xFF);
  }
#endif
}

// Compares [n] bytes without an early exit. strcmp()/strncmp() stop at the
// first difference, leaking through timing how many leading bytes of a hash
// matched -- enough to reconstruct one byte at a time over many requests.
static int ct_equal(const void* a, const void* b, size_t n) {
#ifdef OPENSSL_OK
  return CRYPTO_memcmp(a, b, n) == 0;
#else
  const unsigned char* x = (const unsigned char*)a;
  const unsigned char* y = (const unsigned char*)b;
  unsigned char        diff = 0;
  for(size_t i = 0; i < n; i++)
    diff |= (unsigned char)(x[i] ^ y[i]);
  return diff == 0;
#endif
}

static void hex_encode(const unsigned char* in, size_t in_len, char* out) {
  static const char digits[] = "0123456789abcdef";
  for(size_t i = 0; i < in_len; i++) {
    out[i * 2] = digits[(in[i] >> 4) & 0xF];
    out[i * 2 + 1] = digits[in[i] & 0xF];
  }
  out[in_len * 2] = '\0';
}

// Returns 1 on success. The old code ignored sscanf()'s return value, so a
// short or non-hex stored salt left every byte of salt[16] uninitialized and it
// was then fed straight into the digest.
static int hex_decode(const char* hex, unsigned char* out, size_t out_len) {
  for(size_t i = 0; i < out_len; i++) {
    unsigned int byte;
    char         pair[3] = {hex[i * 2], hex[i * 2 + 1], '\0'};
    if(pair[0] == '\0' || pair[1] == '\0')
      return 0;
    if(sscanf(pair, "%2x", &byte) != 1)
      return 0;
    out[i] = (unsigned char)byte;
  }
  return 1;
}

#ifdef OPENSSL_OK
// Current format. PBKDF2-HMAC-SHA256 with a per-password random salt.
//
// The previous scheme was a single unsalted-iteration SHA-256 (or DJB2 without
// OpenSSL): a hashing function, not a password KDF. A commodity GPU tries
// billions of candidates per second against one round of SHA-256.
#define PBKDF2_PREFIX "pbkdf2$"
#define PBKDF2_ITERATIONS 600000 // OWASP 2023 floor for PBKDF2-HMAC-SHA256
#define PBKDF2_SALT_LEN 16
#define PBKDF2_HASH_LEN 32

static int pbkdf2_derive(const char* password, const unsigned char* salt,
                         unsigned int iterations, unsigned char* out) {
  return PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt, PBKDF2_SALT_LEN,
                           (int)iterations, EVP_sha256(), PBKDF2_HASH_LEN, out) == 1;
}

// One-shot SHA-256 of password||salt: only used to verify hashes written by
// older versions, never to create new ones.
static int legacy_sha256(const char* password, const unsigned char* salt,
                         size_t salt_len, unsigned char* out) {
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if(ctx == NULL) // was unchecked: a NULL here was dereferenced immediately
    return 0;
  unsigned int len = 0;
  int          ok = EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) == 1 &&
                    EVP_DigestUpdate(ctx, password, strlen(password)) == 1 &&
                    EVP_DigestUpdate(ctx, salt, salt_len) == 1 &&
                    EVP_DigestFinal_ex(ctx, out, &len) == 1 && len == 32;
  EVP_MD_CTX_free(ctx);
  return ok;
}
#endif

void hash_password(char* password, char* output) {
  output[0] = '\0';
  if(password == NULL)
    return;

#ifdef OPENSSL_OK
  unsigned char salt[PBKDF2_SALT_LEN];
  unsigned char hash[PBKDF2_HASH_LEN];
  char          salt_hex[PBKDF2_SALT_LEN * 2 + 1];
  char          hash_hex[PBKDF2_HASH_LEN * 2 + 1];

  // Return value checked: silently continuing with an unwritten salt buffer
  // would produce a hash over uninitialized stack memory.
  if(RAND_bytes(salt, sizeof(salt)) != 1) {
    perror("Failed to generate salt");
    return;
  }
  if(!pbkdf2_derive(password, salt, PBKDF2_ITERATIONS, hash)) {
    perror("PBKDF2 derivation failed");
    return;
  }
  hex_encode(salt, sizeof(salt), salt_hex);
  hex_encode(hash, sizeof(hash), hash_hex);
  snprintf(output, HASH_AND_SALT_LENGTH, PBKDF2_PREFIX "%d$%s$%s", PBKDF2_ITERATIONS,
           salt_hex, hash_hex);
#else
  // No OpenSSL: keep the legacy salted-DJB2 format so existing deployments
  // still work. This is not a password KDF; build with OpenSSL for PBKDF2.
  char salt[SALT_LENGTH + 1];
  generate_salt(salt, SALT_LENGTH);
  size_t salted_len = strlen(password) + SALT_LENGTH + 1;
  char*  saltedPassword = (char*)malloc(salted_len);
  if(saltedPassword == NULL) {
    perror("Failed to allocate salted password buffer");
    return;
  }
  snprintf(saltedPassword, salted_len, "%s%s", password, salt);
  char hash[HASH_LENGTH + 1];
  unsafe_hash(saltedPassword, hash);
  free(saltedPassword);
  snprintf(output, HASH_AND_SALT_LENGTH, "%s$%s", hash, salt);
#endif
}

int verify_password(char* password, char* hash_and_salt) {
  if(password == NULL || hash_and_salt == NULL)
    return 0;

  size_t stored_len = strlen(hash_and_salt);

#ifdef OPENSSL_OK
  // Current format: pbkdf2$<iterations>$<salt hex>$<hash hex>. Self-describing
  // so the iteration count can be raised without invalidating stored hashes.
  if(strncmp(hash_and_salt, PBKDF2_PREFIX, sizeof(PBKDF2_PREFIX) - 1) == 0) {
    const char* p = hash_and_salt + sizeof(PBKDF2_PREFIX) - 1;
    char*       end = NULL;
    long        iterations = strtol(p, &end, 10);
    if(end == p || *end != '$' || iterations <= 0 || iterations > 100000000L)
      return 0;
    const char* salt_hex = end + 1;
    const char* sep = strchr(salt_hex, '$');
    if(sep == NULL || (size_t)(sep - salt_hex) != PBKDF2_SALT_LEN * 2)
      return 0;
    const char* hash_hex = sep + 1;
    if(strlen(hash_hex) != PBKDF2_HASH_LEN * 2)
      return 0;

    unsigned char salt[PBKDF2_SALT_LEN];
    unsigned char stored[PBKDF2_HASH_LEN];
    unsigned char computed[PBKDF2_HASH_LEN];
    if(!hex_decode(salt_hex, salt, sizeof(salt)) ||
       !hex_decode(hash_hex, stored, sizeof(stored)))
      return 0;
    if(!pbkdf2_derive(password, salt, (unsigned int)iterations, computed))
      return 0;
    return ct_equal(computed, stored, sizeof(computed));
  }

  // Legacy format written by builds with OpenSSL: <64 hex hash>/<32 hex salt>.
  // The layout is validated up front instead of indexing at a hardcoded offset
  // 65 and hoping the string is long enough.
  if(stored_len == 64 + 1 + 32 && hash_and_salt[64] == '/') {
    unsigned char salt[16];
    unsigned char stored[32];
    unsigned char computed[32];
    if(!hex_decode(hash_and_salt + 65, salt, sizeof(salt)) ||
       !hex_decode(hash_and_salt, stored, sizeof(stored)))
      return 0;
    if(!legacy_sha256(password, salt, sizeof(salt), computed))
      return 0;
    return ct_equal(computed, stored, sizeof(computed));
  }
#else
  (void)stored_len;
#endif

  // Legacy format written by builds without OpenSSL: <32 hex hash>$<16 char
  // salt>. Verified on every build so a database can move between them.
  //
  // Note that the digest is UNSAFE_HASH_HEX_LEN (32) characters, not
  // HASH_LENGTH (64) -- unsafe_hash() prints four %08x values, so HASH_LENGTH
  // is only the size of the buffer it writes into. The old code compared with
  // strncmp(..., HASH_LENGTH), which worked only because both strings happen to
  // terminate at 32.
  {
    const char* sep = strchr(hash_and_salt, '$');
    if(sep == NULL || (size_t)(sep - hash_and_salt) != UNSAFE_HASH_HEX_LEN)
      return 0;
    const char* storedSalt = sep + 1;
    if(strlen(storedSalt) != SALT_LENGTH)
      return 0;

    size_t salted_len = strlen(password) + SALT_LENGTH + 1;
    char*  saltedPassword = (char*)malloc(salted_len);
    if(saltedPassword == NULL) {
      perror("Failed to allocate salted password buffer");
      return 0;
    }
    snprintf(saltedPassword, salted_len, "%s%s", password, storedSalt);
    char computedHash[HASH_LENGTH + 1];
    unsafe_hash(saltedPassword, computedHash);
    free(saltedPassword);
    return ct_equal(computedHash, hash_and_salt, UNSAFE_HASH_HEX_LEN);
  }
}
