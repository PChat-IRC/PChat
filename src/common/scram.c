/* PChat
 * Copyright (C) 2023 Patrick Okraku
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "pchat.h"

#ifdef USE_SSL

#include "scram.h"
#include "pchat_crypto.h"

#include <string.h>
#include <glib.h>

#define NONCE_LENGTH 18
#define CLIENT_KEY "Client Key"
#define SERVER_KEY "Server Key"

scram_session *
scram_session_create (const char *digest, const char *username, const char *password)
{
	scram_session *session;
	pchat_hash_alg alg = pchat_hash_alg_by_name (digest);

	if (alg == 0)
		return NULL;

	session = g_new0 (scram_session, 1);
	session->digest = alg;
	session->digest_size = pchat_hash_size (alg);
	session->username = g_strdup (username);
	session->password = g_strdup (password);
	return session;
}

void
scram_session_free (scram_session *session)
{
	if (session == NULL)
		return;

	g_free (session->username);
	g_free (session->password);
	g_free (session->client_nonce_b64);
	g_free (session->client_first_message_bare);
	g_free (session->salted_password);
	g_free (session->auth_message);
	g_free (session->error);
	g_free (session);
}

static scram_status
process_client_first (scram_session *session, char **output, size_t *output_len)
{
	unsigned char nonce[NONCE_LENGTH];

	if (!pchat_random_bytes (nonce, NONCE_LENGTH))
	{
		session->error = g_strdup ("Could not create client nonce");
		return SCRAM_ERROR;
	}

	session->client_nonce_b64 = g_base64_encode (nonce, NONCE_LENGTH);
	*output = g_strdup_printf ("n,,n=%s,r=%s", session->username, session->client_nonce_b64);
	*output_len = strlen (*output);
	session->client_first_message_bare = g_strdup (*output + 3);
	session->step++;
	return SCRAM_IN_PROGRESS;
}

static scram_status
process_server_first (scram_session *session, const char *data, char **output,
                      size_t *output_len)
{
	char **params;
	char *client_final_message_without_proof = NULL;
	char *salt = NULL;
	char *server_nonce_b64 = NULL;
	char *client_proof_b64;
	unsigned char client_key[PCHAT_HASH_MAX_SIZE];
	unsigned char stored_key[PCHAT_HASH_MAX_SIZE];
	unsigned char client_signature[PCHAT_HASH_MAX_SIZE];
	unsigned char *client_proof;
	unsigned int i, param_count, iteration_count = 0;
	gsize salt_len = 0;
	size_t client_nonce_len;

	params = g_strsplit (data, ",", -1);
	param_count = g_strv_length (params);

	if (param_count < 3)
	{
		session->error = g_strdup_printf ("Invalid server-first-message: %s", data);
		g_strfreev (params);
		return SCRAM_ERROR;
	}

	for (i = 0; i < param_count; i++)
	{
		if (!strncmp (params[i], "r=", 2))
		{
			g_free (server_nonce_b64);
			server_nonce_b64 = g_strdup (params[i] + 2);
		}
		else if (!strncmp (params[i], "s=", 2))
		{
			g_free (salt);
			salt = g_strdup (params[i] + 2);
		}
		else if (!strncmp (params[i], "i=", 2))
		{
			iteration_count = strtoul (params[i] + 2, NULL, 10);
		}
	}
	g_strfreev (params);

	if (server_nonce_b64 == NULL || *server_nonce_b64 == '\0' || salt == NULL ||
	    *salt == '\0' || iteration_count == 0)
	{
		session->error = g_strdup_printf ("Invalid server-first-message: %s", data);
		g_free (server_nonce_b64);
		g_free (salt);
		return SCRAM_ERROR;
	}

	client_nonce_len = strlen (session->client_nonce_b64);
	if (strlen (server_nonce_b64) < client_nonce_len ||
	    strncmp (server_nonce_b64, session->client_nonce_b64, client_nonce_len))
	{
		session->error = g_strdup_printf ("Invalid server nonce: %s", server_nonce_b64);
		g_free (server_nonce_b64);
		g_free (salt);
		return SCRAM_ERROR;
	}

	g_base64_decode_inplace ((gchar *) salt, &salt_len);

	/* SaltedPassword := Hi(Normalize(password), salt, i) */
	session->salted_password = g_malloc (session->digest_size);
	if (!pchat_pbkdf2 (session->digest, session->password, strlen (session->password),
	                   (unsigned char *) salt, salt_len, iteration_count,
	                   session->salted_password, session->digest_size))
	{
		session->error = g_strdup ("PBKDF2 failed");
		g_free (server_nonce_b64);
		g_free (salt);
		return SCRAM_ERROR;
	}

	client_final_message_without_proof = g_strdup_printf ("c=biws,r=%s", server_nonce_b64);
	session->auth_message = g_strdup_printf ("%s,%s,%s",
		session->client_first_message_bare, data, client_final_message_without_proof);

	/* ClientKey := HMAC(SaltedPassword, "Client Key") */
	if (!pchat_hmac (session->digest, session->salted_password, session->digest_size,
	                 CLIENT_KEY, strlen (CLIENT_KEY), client_key))
	{
		session->error = g_strdup ("HMAC failed");
		g_free (server_nonce_b64);
		g_free (salt);
		g_free (client_final_message_without_proof);
		return SCRAM_ERROR;
	}

	/* StoredKey := H(ClientKey) */
	if (!pchat_hash (session->digest, client_key, session->digest_size, stored_key))
	{
		session->error = g_strdup ("Hash failed");
		g_free (server_nonce_b64);
		g_free (salt);
		g_free (client_final_message_without_proof);
		return SCRAM_ERROR;
	}

	/* ClientSignature := HMAC(StoredKey, AuthMessage) */
	if (!pchat_hmac (session->digest, stored_key, session->digest_size,
	                 session->auth_message, strlen (session->auth_message),
	                 client_signature))
	{
		session->error = g_strdup ("HMAC failed");
		g_free (server_nonce_b64);
		g_free (salt);
		g_free (client_final_message_without_proof);
		return SCRAM_ERROR;
	}

	/* ClientProof := ClientKey XOR ClientSignature */
	client_proof = g_malloc0 (session->digest_size);
	for (i = 0; i < session->digest_size; i++)
		client_proof[i] = client_key[i] ^ client_signature[i];

	client_proof_b64 = g_base64_encode (client_proof, session->digest_size);

	*output = g_strdup_printf ("%s,p=%s", client_final_message_without_proof, client_proof_b64);
	*output_len = strlen (*output);

	g_free (server_nonce_b64);
	g_free (salt);
	g_free (client_final_message_without_proof);
	g_free (client_proof);
	g_free (client_proof_b64);

	session->step++;
	return SCRAM_IN_PROGRESS;
}

static scram_status
process_server_final (scram_session *session, const char *data)
{
	char *verifier;
	unsigned char server_key[PCHAT_HASH_MAX_SIZE];
	unsigned char server_signature[PCHAT_HASH_MAX_SIZE];
	gsize verifier_len = 0;
	scram_status rv;

	if (strlen (data) < 3 || (data[0] != 'v' && data[1] != '='))
		return SCRAM_ERROR;

	verifier = g_strdup (data + 2);
	g_base64_decode_inplace (verifier, &verifier_len);

	if (!pchat_hmac (session->digest, session->salted_password, session->digest_size,
	                 SERVER_KEY, strlen (SERVER_KEY), server_key))
	{
		g_free (verifier);
		return SCRAM_ERROR;
	}

	if (!pchat_hmac (session->digest, server_key, session->digest_size,
	                 session->auth_message, strlen (session->auth_message),
	                 server_signature))
	{
		g_free (verifier);
		return SCRAM_ERROR;
	}

	rv = (verifier_len == session->digest_size &&
	      memcmp (verifier, server_signature, verifier_len) == 0)
		? SCRAM_SUCCESS : SCRAM_ERROR;
	g_free (verifier);
	return rv;
}

scram_status
scram_process (scram_session *session, const char *input, char **output, size_t *output_len)
{
	switch (session->step)
	{
	case 0: return process_client_first (session, output, output_len);
	case 1: return process_server_first (session, input, output, output_len);
	case 2: return process_server_final (session, input);
	default:
		*output = NULL;
		*output_len = 0;
		return SCRAM_ERROR;
	}
}

#endif /* USE_SSL */
