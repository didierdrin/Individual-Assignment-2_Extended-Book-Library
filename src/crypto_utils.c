/* ============================================================
 *  File: src/crypto_utils.c
 * ============================================================ */
#include "crypto_utils.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <openssl/sha.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>
#include <openssl/pem.h>
#include <openssl/err.h>

static EC_KEY *g_key = NULL;

/* ------------------------------------------------------------ */
void to_hex(const unsigned char *in, size_t len, char *out)
{
    static const char *H = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[2 * i]     = H[(in[i] >> 4) & 0x0F];
        out[2 * i + 1] = H[in[i] & 0x0F];
    }
    out[2 * len] = '\0';
}

void sha256_hex(const void *data, size_t len, char out[SHA256_HEX_LEN])
{
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char *)data, len, digest);
    to_hex(digest, SHA256_DIGEST_LENGTH, out);
}

size_t der_sig_len(const unsigned char *sig)
{
    if (!sig || sig[0] != 0x30) return 0;
    if (sig[1] & 0x80) {
        int n = sig[1] & 0x7F;
        if (n < 1 || n > 4) return 0;
        size_t len = 0;
        for (int i = 0; i < n; i++) len = (len << 8) | sig[2 + i];
        return len + 2 + (size_t)n;
    }
    return (size_t)sig[1] + 2;
}

/* ------------------------------------------------------------ */
int crypto_init(const char *key_dir)
{
    char priv_path[600], pub_path[600];
    snprintf(priv_path, sizeof priv_path, "%s/node_private.pem", key_dir);
    snprintf(pub_path,  sizeof pub_path,  "%s/node_public.pem",  key_dir);

    FILE *fp = fopen(priv_path, "rb");
    if (fp) {
        g_key = PEM_read_ECPrivateKey(fp, NULL, NULL, NULL);
        fclose(fp);
    }
    if (g_key) {
        printf("[crypto] loaded existing node key  : %s\n", priv_path);
        return 0;
    }

    /* First run: generate a fresh P-256 key pair and persist it. */
    g_key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
    if (!g_key) { ERR_print_errors_fp(stderr); return -1; }
    if (EC_KEY_generate_key(g_key) != 1) { ERR_print_errors_fp(stderr); return -1; }

    mkdir(key_dir, 0700);   /* ignore EEXIST */

    fp = fopen(priv_path, "wb");
    if (!fp) { perror("fopen(private key)"); return -1; }
    PEM_write_ECPrivateKey(fp, g_key, NULL, NULL, 0, NULL, NULL);
    fclose(fp);

    fp = fopen(pub_path, "wb");
    if (!fp) { perror("fopen(public key)"); return -1; }
    PEM_write_EC_PUBKEY(fp, g_key);
    fclose(fp);

    printf("[crypto] generated new ECDSA P-256 key pair in %s/\n", key_dir);
    return 0;
}

void crypto_cleanup(void)
{
    if (g_key) { EC_KEY_free(g_key); g_key = NULL; }
}

/* ------------------------------------------------------------ */
int ecdsa_sign(const unsigned char *msg, size_t msg_len,
               unsigned char sig[ECDSA_SIG_MAX], unsigned int *sig_len)
{
    unsigned char digest[SHA256_DIGEST_LENGTH];
    unsigned int  len = 0;

    if (!g_key) return -1;
    SHA256(msg, msg_len, digest);
    if (ECDSA_sign(0, digest, (int)sizeof digest, sig, &len, g_key) != 1) {
        ERR_print_errors_fp(stderr);
        return -1;
    }
    *sig_len = len;
    return 0;
}

int ecdsa_verify(const unsigned char *msg, size_t msg_len,
                 const unsigned char *sig)
{
    unsigned char digest[SHA256_DIGEST_LENGTH];
    unsigned int  len;

    if (!g_key) return 0;
    len = (unsigned int)der_sig_len(sig);
    if (len == 0 || len > ECDSA_SIG_MAX) return 0;

    SHA256(msg, msg_len, digest);
    return ECDSA_verify(0, digest, (int)sizeof digest,
                        sig, (int)len, g_key) == 1;
}