/* PChat
 * Schannel/CNG (BCrypt) backend for the pchat_crypto abstraction.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#ifdef USE_SCHANNEL

#include "pchat_crypto.h"

#include <windows.h>
#include <bcrypt.h>
#include <string.h>
#include <glib.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(x) ((x) >= 0)
#endif

static LPCWSTR
to_alg_id (pchat_hash_alg alg)
{
	switch (alg)
	{
	case PCHAT_HASH_SHA1:   return BCRYPT_SHA1_ALGORITHM;
	case PCHAT_HASH_SHA256: return BCRYPT_SHA256_ALGORITHM;
	case PCHAT_HASH_SHA512: return BCRYPT_SHA512_ALGORITHM;
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
	switch (alg)
	{
	case PCHAT_HASH_SHA1:   return 20;
	case PCHAT_HASH_SHA256: return 32;
	case PCHAT_HASH_SHA512: return 64;
	}
	return 0;
}

size_t
pchat_hash_size_by_name (const char *name)
{
	return pchat_hash_size (pchat_hash_alg_by_name (name));
}

int
pchat_random_bytes (void *buf, size_t len)
{
	NTSTATUS s = BCryptGenRandom (NULL, (PUCHAR) buf, (ULONG) len,
		BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	return NT_SUCCESS (s);
}

static int
do_hash_or_hmac (pchat_hash_alg alg, int hmac,
                 const void *key, size_t key_len,
                 const void *data, size_t data_len,
                 unsigned char *out)
{
	BCRYPT_ALG_HANDLE halg = NULL;
	BCRYPT_HASH_HANDLE hh = NULL;
	NTSTATUS s;
	int ok = 0;
	LPCWSTR id = to_alg_id (alg);
	if (!id || !out)
		return 0;

	s = BCryptOpenAlgorithmProvider (&halg, id, NULL,
		hmac ? BCRYPT_ALG_HANDLE_HMAC_FLAG : 0);
	if (!NT_SUCCESS (s))
		return 0;

	s = BCryptCreateHash (halg, &hh, NULL, 0,
		hmac ? (PUCHAR) key : NULL,
		hmac ? (ULONG) key_len : 0, 0);
	if (!NT_SUCCESS (s))
		goto out;

	s = BCryptHashData (hh, (PUCHAR) data, (ULONG) data_len, 0);
	if (!NT_SUCCESS (s))
		goto out;

	s = BCryptFinishHash (hh, out, (ULONG) pchat_hash_size (alg), 0);
	if (NT_SUCCESS (s))
		ok = 1;

out:
	if (hh)
		BCryptDestroyHash (hh);
	BCryptCloseAlgorithmProvider (halg, 0);
	return ok;
}

int
pchat_hash (pchat_hash_alg alg, const void *data, size_t len, unsigned char *out)
{
	return do_hash_or_hmac (alg, 0, NULL, 0, data, len, out);
}

int
pchat_hmac (pchat_hash_alg alg,
            const void *key, size_t key_len,
            const void *data, size_t data_len,
            unsigned char *out)
{
	return do_hash_or_hmac (alg, 1, key, key_len, data, data_len, out);
}

int
pchat_pbkdf2 (pchat_hash_alg alg,
              const char *password, size_t password_len,
              const unsigned char *salt, size_t salt_len,
              unsigned int iterations,
              unsigned char *out, size_t out_len)
{
	BCRYPT_ALG_HANDLE halg = NULL;
	NTSTATUS s;
	LPCWSTR id = to_alg_id (alg);
	if (!id)
		return 0;

	s = BCryptOpenAlgorithmProvider (&halg, id, NULL,
		BCRYPT_ALG_HANDLE_HMAC_FLAG);
	if (!NT_SUCCESS (s))
		return 0;

	s = BCryptDeriveKeyPBKDF2 (halg,
		(PUCHAR) password, (ULONG) password_len,
		(PUCHAR) salt, (ULONG) salt_len,
		(ULONGLONG) iterations,
		out, (ULONG) out_len, 0);

	BCryptCloseAlgorithmProvider (halg, 0);
	return NT_SUCCESS (s);
}

#endif /* USE_SCHANNEL */
