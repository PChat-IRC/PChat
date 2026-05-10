/* PChat
 * OpenSSL backend for the pchat_crypto abstraction.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#ifdef USE_OPENSSL

#include "pchat_crypto.h"

#include <string.h>
#include <glib.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

static const EVP_MD *
to_evp (pchat_hash_alg alg)
{
	switch (alg)
	{
	case PCHAT_HASH_SHA1:   return EVP_sha1 ();
	case PCHAT_HASH_SHA256: return EVP_sha256 ();
	case PCHAT_HASH_SHA512: return EVP_sha512 ();
	}
	return NULL;
}

pchat_hash_alg
pchat_hash_alg_by_name (const char *name)
{
	if (!name)
		return 0;
	if (!g_ascii_strcasecmp (name, "SHA1") || !g_ascii_strcasecmp (name, "SHA-1"))
		return PCHAT_HASH_SHA1;
	if (!g_ascii_strcasecmp (name, "SHA256") || !g_ascii_strcasecmp (name, "SHA-256"))
		return PCHAT_HASH_SHA256;
	if (!g_ascii_strcasecmp (name, "SHA512") || !g_ascii_strcasecmp (name, "SHA-512"))
		return PCHAT_HASH_SHA512;
	return 0;
}

size_t
pchat_hash_size (pchat_hash_alg alg)
{
	const EVP_MD *md = to_evp (alg);
	return md ? (size_t) EVP_MD_size (md) : 0;
}

size_t
pchat_hash_size_by_name (const char *name)
{
	return pchat_hash_size (pchat_hash_alg_by_name (name));
}

int
pchat_random_bytes (void *buf, size_t len)
{
	return RAND_bytes (buf, (int) len) == 1;
}

int
pchat_hash (pchat_hash_alg alg, const void *data, size_t len, unsigned char *out)
{
	const EVP_MD *md = to_evp (alg);
	EVP_MD_CTX *ctx;
	unsigned int olen = 0;
	int ok = 0;

	if (!md || !out)
		return 0;
	ctx = EVP_MD_CTX_new ();
	if (!ctx)
		return 0;
	if (EVP_DigestInit_ex (ctx, md, NULL)
		&& EVP_DigestUpdate (ctx, data, len)
		&& EVP_DigestFinal_ex (ctx, out, &olen))
	{
		ok = 1;
	}
	EVP_MD_CTX_free (ctx);
	return ok;
}

int
pchat_hmac (pchat_hash_alg alg,
            const void *key, size_t key_len,
            const void *data, size_t data_len,
            unsigned char *out)
{
	const EVP_MD *md = to_evp (alg);
	unsigned int olen = 0;
	if (!md || !out)
		return 0;
	if (HMAC (md, key, (int) key_len, data, data_len, out, &olen) == NULL)
		return 0;
	return olen > 0;
}

int
pchat_pbkdf2 (pchat_hash_alg alg,
              const char *password, size_t password_len,
              const unsigned char *salt, size_t salt_len,
              unsigned int iterations,
              unsigned char *out, size_t out_len)
{
	const EVP_MD *md = to_evp (alg);
	if (!md)
		return 0;
	return PKCS5_PBKDF2_HMAC (password, (int) password_len,
		salt, (int) salt_len, (int) iterations, md,
		(int) out_len, out) == 1;
}

#endif /* USE_OPENSSL */
