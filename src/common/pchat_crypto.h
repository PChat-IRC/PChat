/* PChat
 * Copyright (C) 2026 PChat-IRC contributors.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 * Tiny crypto abstraction used by SASL SCRAM and the QuakeNet
 * CHALLENGEAUTH helper. Two implementations exist:
 *   pchat_crypto_openssl.c  - OpenSSL EVP / HMAC / PBKDF2 / RAND
 *   pchat_crypto_schannel.c - Windows BCrypt (CNG)
 */

#ifndef PCHAT_CRYPTO_H
#define PCHAT_CRYPTO_H

#include "config.h"

#ifdef USE_SSL

#include <stddef.h>

typedef enum {
	PCHAT_HASH_SHA1 = 1,
	PCHAT_HASH_SHA256,
	PCHAT_HASH_SHA512
} pchat_hash_alg;

#define PCHAT_HASH_MAX_SIZE 64 /* SHA-512 */

/* Returns digest size in bytes, or 0 if name is unknown.
 * Accepted names (case-insensitive): "SHA1", "SHA-1", "SHA256", "SHA-256",
 * "SHA512", "SHA-512". */
size_t pchat_hash_size_by_name (const char *name);

/* Resolves a textual digest name to an algorithm id. Returns 0 on unknown. */
pchat_hash_alg pchat_hash_alg_by_name (const char *name);

/* Returns digest size in bytes for a known algorithm. */
size_t pchat_hash_size (pchat_hash_alg alg);

/* Fill buffer with cryptographically secure random bytes.
 * Returns 1 on success, 0 on failure. */
int pchat_random_bytes (void *buf, size_t len);

/* One-shot hash. Output buffer must be at least pchat_hash_size(alg) bytes.
 * Returns 1 on success, 0 on failure. */
int pchat_hash (pchat_hash_alg alg, const void *data, size_t len,
                unsigned char *out);

/* One-shot HMAC. Output buffer must be at least pchat_hash_size(alg) bytes.
 * Returns 1 on success, 0 on failure. */
int pchat_hmac (pchat_hash_alg alg,
                const void *key, size_t key_len,
                const void *data, size_t data_len,
                unsigned char *out);

/* PBKDF2-HMAC-<alg>. Returns 1 on success, 0 on failure. */
int pchat_pbkdf2 (pchat_hash_alg alg,
                  const char *password, size_t password_len,
                  const unsigned char *salt, size_t salt_len,
                  unsigned int iterations,
                  unsigned char *out, size_t out_len);

#endif /* USE_SSL */

#endif /* PCHAT_CRYPTO_H */
