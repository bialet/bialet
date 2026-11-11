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
#ifdef _WIN32
// Must precede *every* include: it is what makes rand_s visible, and mingw's
// stdlib.h only declares rand_s if this is already defined when stdlib.h is
// first pulled in. It used to sit further down, after hash.h had already
// included stdlib.h indirectly (through the OpenSSL headers), so rand_s was
// undeclared and the MinGW build failed with -Werror=implicit-function-
// declaration.
#define _CRT_RAND_S
#endif

#include "hash.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

#include <stdlib.h>

// Number of characters unsafe_hash() actually writes: four %08x values.
#define UNSAFE_HASH_HEX_LEN 32

// DJB2 over exactly [len] bytes, so a credential containing 0x00 is hashed in
// full instead of ending at the NUL. The byte keeps the promoted-char
// arithmetic of the original loop: an unsigned cast would change every digest
// for bytes >= 0x80 and break stored hashes.
static unsigned int djb2_update(unsigned int hash, const char* input, size_t len) {
  for(size_t i = 0; i < len; i++)
    hash = ((hash << 5) + hash) + input[i]; // hash * 33 + c
  return hash;
}

static void format_unsafe_hash(unsigned int hash, char* output) {
  snprintf(output, HASH_LENGTH + 1, "%08x%08x%08x%08x", hash, hash * 3, hash * 5,
           hash * 7);
}

void unsafe_hash_len(const char* input, size_t len, char* output) {
  format_unsafe_hash(djb2_update(5381, input, len), output);
}

void unsafe_hash(const char* input, char* output) {
  unsafe_hash_len(input, strlen(input), output);
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

// Zeroes [len] bytes at [ptr] so a derived key or a forged plaintext does not
// linger in freed heap. A plain memset() can be optimized away when the buffer
// is about to be freed, so the call has to be opaque to the compiler.
void secure_zero(void* ptr, size_t len) {
  if(ptr == NULL || len == 0)
    return;
#ifdef OPENSSL_OK
  OPENSSL_cleanse(ptr, len);
#else
  volatile unsigned char* p = (volatile unsigned char*)ptr;
  while(len-- > 0)
    *p++ = 0;
#endif
}

// ── SHA-256 ────────────────────────────────────────────────────────────────
// FIPS 180-4, self-contained. Deliberately not gated on OpenSSL: this backs
// OAuth 2.1 / PKCE (RFC 7636, S256) and JWT/OIDC, where "no OpenSSL" is not an
// acceptable reason to be unable to compute a digest (unlike PBKDF2, whose
// no-OpenSSL path degrades to the legacy password format on purpose).

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static uint32_t rotr32(uint32_t x, unsigned n) {
  return (x >> n) | (x << (32 - n));
}

static void sha256_block(uint32_t state[8], const unsigned char block[64]) {
  uint32_t w[64];
  for(int i = 0; i < 16; i++) {
    w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) |
           ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
  }
  for(int i = 16; i < 64; i++) {
    uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }

  uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
  uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
  for(int i = 0; i < 64; i++) {
    uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
    uint32_t ch = (e & f) ^ (~e & g);
    uint32_t temp1 = h + s1 + ch + sha256_k[i] + w[i];
    uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
    uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    uint32_t temp2 = s0 + maj;

    h = g;
    g = f;
    f = e;
    e = d + temp1;
    d = c;
    c = b;
    b = a;
    a = temp1 + temp2;
  }

  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  state[5] += f;
  state[6] += g;
  state[7] += h;
}

void sha256_raw(const char* data, size_t len, unsigned char* out) {
  uint32_t             state[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  const unsigned char* p = (const unsigned char*)data;

  size_t offset = 0;
  for(; offset + 64 <= len; offset += 64)
    sha256_block(state, p + offset);

  // Padding: 0x80, zeros, then the 64-bit big-endian bit length. The tail
  // spans one block when the remainder leaves room for the length (<= 55
  // bytes) and two otherwise.
  unsigned char tail[128];
  size_t        rem = len - offset;
  if(rem > 0)
    memcpy(tail, p + offset, rem);
  tail[rem] = 0x80;
  size_t pad_at;
  if(rem + 1 <= 56) {
    memset(tail + rem + 1, 0, 56 - (rem + 1));
    pad_at = 56;
  } else {
    memset(tail + rem + 1, 0, 128 - (rem + 1));
    pad_at = 120;
  }
  uint64_t bit_len = (uint64_t)len * 8;
  for(int i = 0; i < 8; i++)
    tail[pad_at + i] = (unsigned char)(bit_len >> (56 - i * 8));

  sha256_block(state, tail);
  if(pad_at == 120)
    sha256_block(state, tail + 64);

  for(int i = 0; i < 8; i++) {
    out[i * 4] = (unsigned char)(state[i] >> 24);
    out[i * 4 + 1] = (unsigned char)(state[i] >> 16);
    out[i * 4 + 2] = (unsigned char)(state[i] >> 8);
    out[i * 4 + 3] = (unsigned char)(state[i]);
  }
}

void sha256_hex(const char* data, size_t len, char* out) {
  static const char digits[] = "0123456789abcdef";
  unsigned char     digest[32];
  sha256_raw(data, len, digest);
  for(size_t i = 0; i < sizeof(digest); i++) {
    out[i * 2] = digits[(digest[i] >> 4) & 0xF];
    out[i * 2 + 1] = digits[digest[i] & 0xF];
  }
  out[64] = '\0';
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

#ifdef OPENSSL_OK
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
#endif /* OPENSSL_OK: the hex helpers are only used by the PBKDF2 and legacy        \
        * SHA-256 formats; the DJB2 fallback format stores its salt as ASCII. */

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

// Shared with the encryption envelope: same KDF, caller-chosen salt and output
// length. PKCS5_PBKDF2_HMAC takes int lengths and an int iteration count, so
// anything outside int range has to be refused rather than truncated into a
// different derivation.
int pbkdf2_sha256(const char* password, size_t password_len,
                  const unsigned char* salt, size_t salt_len,
                  unsigned int iterations, unsigned char* out, size_t out_len) {
  if(password == NULL || salt == NULL || out == NULL)
    return 0;
  if(password_len > (size_t)INT_MAX || salt_len > (size_t)INT_MAX ||
     out_len > (size_t)INT_MAX || salt_len == 0 || out_len == 0 || iterations == 0 ||
     iterations > (unsigned int)INT_MAX)
    return 0;
  return PKCS5_PBKDF2_HMAC(password, (int)password_len, salt, (int)salt_len,
                           (int)iterations, EVP_sha256(), (int)out_len, out) == 1;
}

static int pbkdf2_derive(const char* password, size_t password_len,
                         const unsigned char* salt, unsigned int iterations,
                         unsigned char* out) {
  // PKCS5_PBKDF2_HMAC takes an int length; a credential longer than INT_MAX
  // cannot be hashed correctly, so fail instead of truncating.
  if(password_len > (size_t)INT_MAX)
    return 0;
  return pbkdf2_sha256(password, password_len, salt, PBKDF2_SALT_LEN, iterations,
                       out, PBKDF2_HASH_LEN);
}

// One-shot SHA-256 of password||salt: only used to verify hashes written by
// older versions, never to create new ones.
static int legacy_sha256(const char* password, size_t password_len,
                         const unsigned char* salt, size_t salt_len,
                         unsigned char* out) {
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  if(ctx == NULL) // was unchecked: a NULL here was dereferenced immediately
    return 0;
  unsigned int len = 0;
  int          ok = EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) == 1 &&
                    EVP_DigestUpdate(ctx, password, password_len) == 1 &&
                    EVP_DigestUpdate(ctx, salt, salt_len) == 1 &&
                    EVP_DigestFinal_ex(ctx, out, &len) == 1 && len == 32;
  EVP_MD_CTX_free(ctx);
  return ok;
}
#else
// No OpenSSL: no PBKDF2 for encryption either. Returning 0 makes crypto_encrypt
// fail, which is the intended outcome -- a build without OpenSSL must not
// silently encrypt with something weaker.
int pbkdf2_sha256(const char* password, size_t password_len,
                  const unsigned char* salt, size_t salt_len,
                  unsigned int iterations, unsigned char* out, size_t out_len) {
  (void)password;
  (void)password_len;
  (void)salt;
  (void)salt_len;
  (void)iterations;
  (void)out;
  (void)out_len;
  return 0;
}
#endif

// password||salt for the DJB2 format over explicit lengths. For a credential
// with no NUL byte this is byte-identical to the old "%s%s" buffer up to its
// terminator, so existing stored hashes keep verifying unchanged; the only
// difference is that bytes after an embedded NUL now take part too.
static void hash_salted_djb2(const char* password, size_t password_len,
                             const char* salt, size_t salt_len, char* output) {
  unsigned int hash = djb2_update(5381, password, password_len);
  format_unsafe_hash(djb2_update(hash, salt, salt_len), output);
}

void hash_password(const char* password, size_t password_len, char* output) {
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
  if(!pbkdf2_derive(password, password_len, salt, PBKDF2_ITERATIONS, hash)) {
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
  char hash[HASH_LENGTH + 1];
  generate_salt(salt, SALT_LENGTH);
  hash_salted_djb2(password, password_len, salt, SALT_LENGTH, hash);
  snprintf(output, HASH_AND_SALT_LENGTH, "%s$%s", hash, salt);
#endif
}

int verify_password(const char* password, size_t password_len,
                    const char* hash_and_salt) {
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
    if(!pbkdf2_derive(password, password_len, salt, (unsigned int)iterations,
                      computed))
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
    if(!legacy_sha256(password, password_len, salt, sizeof(salt), computed))
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

    char computedHash[HASH_LENGTH + 1];
    hash_salted_djb2(password, password_len, storedSalt, SALT_LENGTH, computedHash);
    return ct_equal(computedHash, hash_and_salt, UNSAFE_HASH_HEX_LEN);
  }
}

// ── AES-256-GCM ────────────────────────────────────────────────────────────
//
// Envelope:
//
//   bgcm1$<iterations>$<salt hex|->$<iv hex>$<ciphertext||tag hex>
//
// Self-describing for the same reason the password format is: the iteration
// count travels with the value, so it can be raised later without invalidating
// stored ciphertext.
//
// Choices worth stating:
//
//   * GCM, not CBC/CTR: the tag is what turns a tampered value into a decryption
//     failure instead of a plausible-looking forgery.
//   * A fresh random 96-bit nonce per message, from the OS CSPRNG. Reusing a
//     nonce under one key breaks GCM catastrophically (XOR of plaintexts, plus
//     the authentication key), so it is never a counter held in memory.
//   * A 64-hex-character secret is a raw AES-256 key and skips the KDF -- the
//     path a deployment key file should take (`openssl rand -hex 32`). Only a
//     passphrase goes through PBKDF2. Nothing is ever stretched with a bare
//     SHA-256, and there is no unauthenticated mode.
//   * No OpenSSL means no encryption. Returning NULL is deliberate: a weaker
//     fallback that still round-trips would be worse than an obvious failure.

#ifdef OPENSSL_OK
#define ENC_PREFIX "bgcm1$"
#define ENC_PREFIX_LEN (sizeof(ENC_PREFIX) - 1)
#define ENC_KEY_LEN 32
#define ENC_SALT_LEN 16
#define ENC_IV_LEN 12
#define ENC_TAG_LEN 16
// OWASP's 2023 floor for PBKDF2-HMAC-SHA256. It only runs for passphrases.
#define ENC_ITERATIONS 600000u
// A tampered envelope must not be able to burn minutes of CPU. Anything above
// this is rejected before the KDF runs.
#define ENC_MAX_ITERATIONS 10000000L
#define ENC_MAX_PAYLOAD (64u * 1024u * 1024u)
#define ENC_MAX_ENVELOPE ((size_t)ENC_MAX_PAYLOAD * 2 + 256)

int crypto_available(void) {
  return 1;
}

// A raw key is exactly 32 bytes written as hex. Anything else -- including a
// 64-character passphrase -- is treated as a passphrase.
static int enc_is_raw_key(const char* secret, size_t len) {
  if(len != (size_t)ENC_KEY_LEN * 2)
    return 0;
  for(size_t i = 0; i < len; i++) {
    if(!isxdigit((unsigned char)secret[i]))
      return 0;
  }
  return 1;
}

// Derives the 32-byte key for one message. Returns 1 on success.
static int enc_derive_key(const char* secret, size_t secret_len,
                          const unsigned char* salt, unsigned int iterations,
                          unsigned char key[ENC_KEY_LEN]) {
  if(iterations == 0)
    return hex_decode(secret, key, ENC_KEY_LEN);
  return pbkdf2_sha256(secret, secret_len, salt, ENC_SALT_LEN, iterations, key,
                       ENC_KEY_LEN);
}

static int enc_gcm_encrypt(const unsigned char key[ENC_KEY_LEN],
                           const unsigned char iv[ENC_IV_LEN], const char* aad,
                           size_t aad_len, const unsigned char* plaintext,
                           size_t plaintext_len, unsigned char* ciphertext,
                           unsigned char tag[ENC_TAG_LEN]) {
  // EVP takes int lengths: refuse rather than truncate a huge value into a
  // different message.
  if(plaintext_len > (size_t)INT_MAX || aad_len > (size_t)INT_MAX)
    return 0;

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if(ctx == NULL)
    return 0;

  int ok = 0, len = 0, total = 0;
  if(EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
    goto done;
  if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, ENC_IV_LEN, NULL) != 1)
    goto done;
  if(EVP_EncryptInit_ex(ctx, NULL, NULL, key, iv) != 1)
    goto done;
  // AAD is authenticated but not stored; the caller supplies it again to
  // decrypt. Empty AAD is skipped (NULL would be fine too, but this keeps the
  // call sequence identical for both cases).
  if(aad_len > 0 && EVP_EncryptUpdate(ctx, NULL, &len, (const unsigned char*)aad,
                                      (int)aad_len) != 1)
    goto done;
  if(plaintext_len > 0) {
    if(EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, (int)plaintext_len) != 1)
      goto done;
    total = len;
  }
  if(EVP_EncryptFinal_ex(ctx, ciphertext + total, &len) != 1)
    goto done;
  total += len;
  if((size_t)total != plaintext_len) // GCM keeps the length; a mismatch is a bug
    goto done;
  if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, ENC_TAG_LEN, tag) != 1)
    goto done;
  ok = 1;
done:
  EVP_CIPHER_CTX_free(ctx);
  return ok;
}

static int enc_gcm_decrypt(const unsigned char key[ENC_KEY_LEN],
                           const unsigned char iv[ENC_IV_LEN], const char* aad,
                           size_t aad_len, const unsigned char* ciphertext,
                           size_t              ciphertext_len,
                           const unsigned char tag[ENC_TAG_LEN],
                           unsigned char*      plaintext) {
  if(ciphertext_len > (size_t)INT_MAX || aad_len > (size_t)INT_MAX)
    return 0;

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if(ctx == NULL)
    return 0;

  int ok = 0, len = 0, total = 0;
  if(EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), NULL, NULL, NULL) != 1)
    goto done;
  if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, ENC_IV_LEN, NULL) != 1)
    goto done;
  if(EVP_DecryptInit_ex(ctx, NULL, NULL, key, iv) != 1)
    goto done;
  if(aad_len > 0 && EVP_DecryptUpdate(ctx, NULL, &len, (const unsigned char*)aad,
                                      (int)aad_len) != 1)
    goto done;
  if(ciphertext_len > 0) {
    // GCM releases plaintext here, before the tag is checked. The caller wipes
    // the buffer when the final call fails.
    if(EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, (int)ciphertext_len) != 1)
      goto done;
    total = len;
  }
  // ctrl() takes the tag as void*; it is only read.
  if(EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, ENC_TAG_LEN, (void*)tag) != 1)
    goto done;
  // The only call that verifies the tag: 1 means authentic, anything else is a
  // wrong key or a modified value.
  if(EVP_DecryptFinal_ex(ctx, plaintext + total, &len) != 1)
    goto done;
  total += len;
  if((size_t)total != ciphertext_len)
    goto done;
  ok = 1;
done:
  EVP_CIPHER_CTX_free(ctx);
  return ok;
}

char* crypto_encrypt(const char* secret, size_t secret_len, const char* aad,
                     size_t aad_len, const char* plaintext, size_t plaintext_len) {
  if(secret == NULL || secret_len == 0 || plaintext == NULL)
    return NULL;
  if(plaintext_len > ENC_MAX_PAYLOAD || aad_len > ENC_MAX_PAYLOAD)
    return NULL;

  unsigned char  key[ENC_KEY_LEN];
  unsigned char  salt[ENC_SALT_LEN];
  unsigned char  iv[ENC_IV_LEN];
  char           salt_hex[ENC_SALT_LEN * 2 + 1];
  char           iv_hex[ENC_IV_LEN * 2 + 1];
  unsigned int   iterations = ENC_ITERATIONS;
  unsigned char* sealed = NULL;
  char*          out = NULL;

  if(enc_is_raw_key(secret, secret_len)) {
    // Nothing is being stretched, so a salt would add nothing but bytes.
    iterations = 0;
    salt_hex[0] = '-';
    salt_hex[1] = '\0';
  } else {
    random_bytes_fill(salt, sizeof(salt));
    hex_encode(salt, sizeof(salt), salt_hex);
  }

  if(!enc_derive_key(secret, secret_len, salt, iterations, key))
    goto done;

  random_bytes_fill(iv, sizeof(iv));
  hex_encode(iv, sizeof(iv), iv_hex);

  size_t sealed_len = plaintext_len + ENC_TAG_LEN; // ciphertext || tag
  sealed = (unsigned char*)malloc(sealed_len);
  if(sealed == NULL)
    goto done;
  if(!enc_gcm_encrypt(key, iv, aad, aad_len, (const unsigned char*)plaintext,
                      plaintext_len, sealed, sealed + plaintext_len))
    goto done;

  // Small fixed header: prefix, at most 8 digits, a 32- or 1-char salt, a
  // 24-char IV, and the separators. snprintf is measured, then the envelope is
  // allocated to fit exactly (no fixed buffer with dynamic content).
  char head[128];
  int  head_len = snprintf(head, sizeof(head), ENC_PREFIX "%u$%s$%s$", iterations,
                           salt_hex, iv_hex);
  if(head_len < 0 || (size_t)head_len >= sizeof(head))
    goto done;

  size_t hex_len = sealed_len * 2;
  out = (char*)malloc((size_t)head_len + hex_len + 1);
  if(out == NULL)
    goto done;
  memcpy(out, head, (size_t)head_len);
  hex_encode(sealed, sealed_len, out + head_len);

done:
  if(sealed != NULL) {
    secure_zero(sealed, plaintext_len + ENC_TAG_LEN);
    free(sealed);
  }
  secure_zero(key, sizeof(key));
  secure_zero(salt, sizeof(salt));
  return out;
}

unsigned char* crypto_decrypt(const char* secret, size_t secret_len, const char* aad,
                              size_t aad_len, const char* envelope,
                              size_t envelope_len, size_t* out_len) {
  if(out_len != NULL)
    *out_len = 0;
  if(secret == NULL || secret_len == 0 || envelope == NULL)
    return NULL;
  if(envelope_len < ENC_PREFIX_LEN + 4 || envelope_len > ENC_MAX_ENVELOPE)
    return NULL;
  if(memcmp(envelope, ENC_PREFIX, ENC_PREFIX_LEN) != 0)
    return NULL;
  // The parser walks the value with strtol/strchr, so an embedded NUL has to be
  // refused here instead of silently truncating the envelope.
  if(strlen(envelope) != envelope_len)
    return NULL;

  const char* p = envelope + ENC_PREFIX_LEN;
  char*       end = NULL;
  long        iterations = strtol(p, &end, 10);
  // A negative or absurd count is a tampered envelope, not a slow one: reject
  // before running the KDF. (strtol saturates at LONG_MAX on overflow, which
  // this bound rejects too.)
  if(end == p || *end != '$' || iterations < 0 || iterations > ENC_MAX_ITERATIONS)
    return NULL;
  // iteration 0 means "raw key", so the secret must actually be one: otherwise
  // hex_decode would read past the end of a shorter string.
  if(iterations == 0 && !enc_is_raw_key(secret, secret_len))
    return NULL;

  const char* salt_hex = end + 1;
  const char* sep = strchr(salt_hex, '$');
  if(sep == NULL)
    return NULL;
  size_t salt_hex_len = (size_t)(sep - salt_hex);

  const char* iv_hex = sep + 1;
  sep = strchr(iv_hex, '$');
  if(sep == NULL)
    return NULL;
  if((size_t)(sep - iv_hex) != (size_t)ENC_IV_LEN * 2)
    return NULL;

  const char* ct_hex = sep + 1;
  size_t      ct_hex_len = envelope_len - (size_t)(ct_hex - envelope);
  if(ct_hex_len % 2 != 0 || ct_hex_len < (size_t)ENC_TAG_LEN * 2)
    return NULL;
  size_t ct_len = ct_hex_len / 2; // ciphertext || tag
  size_t pt_len = ct_len - ENC_TAG_LEN;
  if(pt_len > ENC_MAX_PAYLOAD)
    return NULL;

  unsigned char  key[ENC_KEY_LEN];
  unsigned char  salt[ENC_SALT_LEN];
  unsigned char  iv[ENC_IV_LEN];
  unsigned char* ct = NULL;
  unsigned char* pt = NULL;
  unsigned char* result = NULL;

  if(iterations == 0) {
    if(salt_hex_len != 1 || salt_hex[0] != '-')
      goto done;
  } else if(salt_hex_len != (size_t)ENC_SALT_LEN * 2) {
    goto done;
  } else if(!hex_decode(salt_hex, salt, ENC_SALT_LEN)) {
    goto done;
  }
  if(!hex_decode(iv_hex, iv, ENC_IV_LEN))
    goto done;

  ct = (unsigned char*)malloc(ct_len);
  if(ct == NULL)
    goto done;
  if(!hex_decode(ct_hex, ct, ct_len))
    goto done;
  if(!enc_derive_key(secret, secret_len, salt, (unsigned int)iterations, key))
    goto done;

  // +1 so the result can also be handed to Wren as a C string; the real length
  // is returned separately because the plaintext may contain NUL bytes.
  pt = (unsigned char*)malloc(pt_len + 1);
  if(pt == NULL)
    goto done;
  if(!enc_gcm_decrypt(key, iv, aad, aad_len, ct, pt_len,
                      ct + pt_len, // the tag is appended to the ciphertext
                      pt))
    goto done;

  pt[pt_len] = '\0';
  if(out_len != NULL)
    *out_len = pt_len;
  result = pt;
  pt = NULL; // ownership moves to the caller

done:
  if(pt != NULL) {
    // GCM writes plaintext before it verifies the tag, so a forged envelope
    // leaves unauthenticated bytes in this buffer. Wipe before freeing.
    secure_zero(pt, pt_len);
    free(pt);
  }
  free(ct);
  secure_zero(key, sizeof(key));
  secure_zero(salt, sizeof(salt));
  return result;
}

#else /* !OPENSSL_OK */

int crypto_available(void) {
  return 0;
}

char* crypto_encrypt(const char* secret, size_t secret_len, const char* aad,
                     size_t aad_len, const char* plaintext, size_t plaintext_len) {
  (void)secret;
  (void)secret_len;
  (void)aad;
  (void)aad_len;
  (void)plaintext;
  (void)plaintext_len;
  return NULL;
}

unsigned char* crypto_decrypt(const char* secret, size_t secret_len, const char* aad,
                              size_t aad_len, const char* envelope,
                              size_t envelope_len, size_t* out_len) {
  (void)secret;
  (void)secret_len;
  (void)aad;
  (void)aad_len;
  (void)envelope;
  (void)envelope_len;
  if(out_len != NULL)
    *out_len = 0;
  return NULL;
}

#endif /* OPENSSL_OK */
