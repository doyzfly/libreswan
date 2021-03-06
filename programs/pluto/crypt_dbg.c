/*
 * SYMKEY debug functions, for libreswan
 *
 * Copyright (C) 2015, 2016 Andrew Cagney <cagney@gnu.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <http://www.fsf.org/copyleft/gpl.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "libreswan.h"
#include "lswalloc.h"
#include "lswlog.h"
#include "ike_alg.h"
#include "crypt_dbg.h"
#include "crypt_symkey.h"

#include "crypto.h"
#include "lswnss.h"
#include "lswfips.h"

static PK11SymKey *ephemeral_symkey(int debug)
{
	static int tried;
	static PK11SymKey *ephemeral_key;
	if (!tried) {
		tried = 1;
		/* get a secret key */
		PK11SlotInfo *slot = PK11_GetBestSlot(CKM_AES_KEY_GEN,
						      lsw_return_nss_password_file_info());
		if (slot == NULL) {
			loglog(RC_LOG_SERIOUS, "NSS: ephemeral slot error");
			return NULL;
		}
		ephemeral_key = PK11_KeyGen(slot, CKM_AES_KEY_GEN,
					    NULL, 128/8, NULL);
		PK11_FreeSlot(slot); /* reference counted */
	}
	DBG(debug, DBG_symkey("ephemeral_key", ephemeral_key));
	return ephemeral_key;
}

/*
 * For testing and debugging; return a byte array containing the symkey.
 */
static void *symkey_bytes(const char *name, PK11SymKey *symkey, int debug)
{
	SECStatus status;
	if (symkey == NULL) {
		DBG(debug, DBG_log("%s NULL key has no bytes", name));
		return NULL;
	}

	size_t sizeof_bytes = sizeof_symkey(symkey);
	DBG(debug, DBG_log("%s extracting all %zd bytes of symkey %p",
			     name, sizeof_bytes, symkey));
	DBG(debug, DBG_symkey("symkey", symkey));

	/* get a secret key */
	PK11SymKey *ephemeral_key = ephemeral_symkey(debug);
	if (ephemeral_key == NULL) {
		loglog(RC_LOG_SERIOUS, "%s NSS: ephemeral error", name);
		return NULL;
	}

	/* copy the source key to the secret slot */
	PK11SymKey *slot_key;
	{
		PK11SlotInfo *slot = PK11_GetSlotFromKey(ephemeral_key);
		slot_key = PK11_MoveSymKey(slot, CKA_UNWRAP, 0, 0, symkey);
		PK11_FreeSlot(slot); /* reference counted */
		if (slot_key == NULL) {
			loglog(RC_LOG_SERIOUS, "%s NSS: slot error", name);
			return NULL;
		}
	}
	DBG(debug, DBG_symkey("slot_key", slot_key));

	SECItem wrapped_key;
	/* Round up the wrapped key length to a 16-byte boundary.  */
	wrapped_key.len = (sizeof_bytes + 15) & ~15;
	wrapped_key.data = alloc_bytes(wrapped_key.len, name);
	DBG(debug, DBG_log("sizeof bytes %d", wrapped_key.len));
	status = PK11_WrapSymKey(CKM_AES_ECB, NULL, ephemeral_key, slot_key,
				 &wrapped_key);
	if (status != SECSuccess) {
		loglog(RC_LOG_SERIOUS, "%s NSS: containment error (%d)",
		       name, status);
		pfreeany(wrapped_key.data);
		free_any_symkey("slot_key:", &slot_key);
		return NULL;
	}
	DBG(debug, DBG_dump("wrapper:", wrapped_key.data, wrapped_key.len));

	void *bytes = alloc_bytes(wrapped_key.len, name);
	unsigned int out_len = 0;
	status = PK11_Decrypt(ephemeral_key, CKM_AES_ECB, NULL,
			      bytes, &out_len, wrapped_key.len,
			      wrapped_key.data, wrapped_key.len);
	pfreeany(wrapped_key.data);
	free_any_symkey("slot_key:", &slot_key);
	if (status != SECSuccess) {
		loglog(RC_LOG_SERIOUS, "%s NSS: error calculating contents (%d)",
		       name, status);
		return NULL;
	}
	passert(out_len >= sizeof_bytes);

	DBG(debug, DBG_log("%s extracted len %d bytes at %p", name, out_len, bytes));
	DBG(debug, DBG_dump("unwrapped:", bytes, out_len));

	return bytes;
}

/*
 * For testing/debugging, return a symkey.
 */
PK11SymKey *chunk_to_symkey(chunk_t raw_key)
{
	PK11SymKey *ephemeral_key = ephemeral_symkey(DBG_CRYPT);
	return symkey_from_chunk(ephemeral_key, raw_key);
}

/*
 * For on-wire algorithms.
 */
chunk_t chunk_from_symkey(const char *prefix, PK11SymKey *symkey)
{
	void *bytes = symkey_bytes(prefix, symkey, DBG_CRYPT);
	if (bytes == NULL) {
		return empty_chunk;
	}
	chunk_t chunk;
	setchunk(chunk, bytes, sizeof_symkey(symkey));
	DBG(DBG_PRIVATE, DBG_dump_chunk(prefix, chunk));
	return chunk;
}

void DBG_dump_symkey(const char *prefix, PK11SymKey *key)
{
	DBG_symkey(prefix, key);
	if (key != NULL) {
		if (DBGP(DBG_PRIVATE)) {
#ifdef FIPS_CHECK
			if (libreswan_fipsmode()) {
				DBG_log("%s secured by FIPS", prefix);
				return;
			}
#else
			void *bytes = symkey_bytes(prefix, key, 0);
			/* NULL suppresses the dump header */
			DBG_dump(NULL, bytes, sizeof_symkey(key));
			pfreeany(bytes);
#endif
		}
	}
}
