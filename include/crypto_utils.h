/* ============================================================
 *  File: include/crypto_utils.h
 * ============================================================ */
#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <stddef.h>

#define SHA256_HEX_LEN 65   /* 64 hex chars + '\0'                 */
#define ECDSA_SIG_MAX  72   /* max DER-encoded P-256 ECDSA sig     */

/* Load (or generate, on first run) the node's ECDSA key pair. */
int  crypto_init(const char *key_dir);
void crypto_cleanup(void);

/* SHA-256 of an arbitrary buffer -> lowercase hex string (65 bytes). */
void sha256_hex(const void *data, size_t len, char out[SHA256_HEX_LEN]);

/* Generic binary -> lowercase hex. `out` must hold 2*len + 1 bytes. */
void to_hex(const unsigned char *in, size_t len, char *out);

/* Sign / verify. The message is hashed with SHA-256 internally. */
int ecdsa_sign(const unsigned char *msg, size_t msg_len,
               unsigned char sig[ECDSA_SIG_MAX], unsigned int *sig_len);
int ecdsa_verify(const unsigned char *msg, size_t msg_len,
                 const unsigned char *sig);

/* Length of a DER-encoded ECDSA signature (0 if malformed). */
size_t der_sig_len(const unsigned char *sig);

#endif /* CRYPTO_UTILS_H */