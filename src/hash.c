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

#include <string.h>

#ifndef OPENSSL_OK

#if defined(_WIN32)
#define _CRT_RAND_S
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

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
#if !defined(_WIN32)
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
#endif

void hash_password(char* password, char* output) {
#ifdef OPENSSL_OK
  unsigned char salt[16];
  if(!RAND_bytes(salt, sizeof(salt))) {
    perror("Failed to generate salt");
    return;
  }

  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int  hash_len;
  EVP_MD_CTX*   ctx = EVP_MD_CTX_new();
  const EVP_MD* md = EVP_sha256();

  EVP_DigestInit_ex(ctx, md, NULL);
  EVP_DigestUpdate(ctx, password, strlen(password));
  EVP_DigestUpdate(ctx, salt, sizeof(salt));
  EVP_DigestFinal_ex(ctx, hash, &hash_len);

  EVP_MD_CTX_free(ctx);

  static char result[HASH_AND_SALT_LENGTH];
  size_t      offset = 0;
  for(unsigned int i = 0; i < hash_len && offset + 2 < HASH_AND_SALT_LENGTH; i++) {
    snprintf(result + offset, HASH_AND_SALT_LENGTH - offset, "%02x", hash[i]);
    offset += 2;
  }

  if(offset + 1 < HASH_AND_SALT_LENGTH) {
    result[offset++] = '/';
    result[offset] = '\0';
  }

  for(size_t i = 0; i < sizeof(salt) && offset + 2 < HASH_AND_SALT_LENGTH; i++) {
    snprintf(result + offset, HASH_AND_SALT_LENGTH - offset, "%02x", salt[i]);
    offset += 2;
  }
#else
  char salt[SALT_LENGTH + 1];
  generate_salt(salt, SALT_LENGTH);
  size_t salted_len = strlen(password) + SALT_LENGTH + 1;
  char*  saltedPassword = (char*)malloc(salted_len);
  if(saltedPassword == NULL) {
    perror("Failed to allocate salted password buffer");
    output[0] = '\0';
    return;
  }
  snprintf(saltedPassword, salted_len, "%s%s", password, salt);
  char hash[HASH_LENGTH + 1];
  unsafe_hash(saltedPassword, hash);
  free(saltedPassword);
  char result[HASH_AND_SALT_LENGTH];
  snprintf(result, sizeof(result), "%s$%s", hash, salt); // Formato: hash$salt
#endif

  strncpy(output, result, HASH_AND_SALT_LENGTH - 1);
  output[HASH_AND_SALT_LENGTH - 1] = '\0';
}

int verify_password(char* password, char* hash_and_salt) {
  int result = 0;
#ifdef OPENSSL_OK

  char stored_hash[65], stored_salt[33];
  strncpy(stored_hash, hash_and_salt, 64);
  stored_hash[64] = 0;
  if(strlen(hash_and_salt) >= 65) {
    strncpy(stored_salt, hash_and_salt + 65, 32);
    stored_salt[32] = 0;
  } else {
    return 0; // Invalid hash format
  }

  unsigned char salt[16];
  for(int i = 0; i < 16; i++) {
    sscanf(stored_salt + i * 2, "%2hhx", &salt[i]);
  }

  unsigned char new_hash[EVP_MAX_MD_SIZE];
  unsigned int  new_hash_len;
  EVP_MD_CTX*   ctx = EVP_MD_CTX_new();
  const EVP_MD* md = EVP_sha256();

  EVP_DigestInit_ex(ctx, md, NULL);
  EVP_DigestUpdate(ctx, password, strlen(password));
  EVP_DigestUpdate(ctx, salt, sizeof(salt));
  EVP_DigestFinal_ex(ctx, new_hash, &new_hash_len);

  EVP_MD_CTX_free(ctx);

  char   new_hash_str[65];
  size_t offset = 0;
  for(unsigned int i = 0; i < new_hash_len && offset + 2 < sizeof(new_hash_str);
      i++) {
    snprintf(new_hash_str + offset, sizeof(new_hash_str) - offset, "%02x",
             new_hash[i]);
    offset += 2;
  }
  new_hash_str[64] = 0;
  result = strcmp(new_hash_str, stored_hash) == 0;
#else
  char storedHash[HASH_LENGTH + 1];
  char storedSalt[SALT_LENGTH + 1];
  if(sscanf(hash_and_salt, "%64[^$]$%16s", storedHash, storedSalt) != 2) {
    return 0;
  }
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
  result = (strncmp(computedHash, storedHash, HASH_LENGTH) == 0);
#endif
  return result;
}
