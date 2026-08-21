#ifndef SODIUM_COMPAT_H
#define SODIUM_COMPAT_H

#include <stddef.h>

#define crypto_aead_xchacha20poly1305_ietf_KEYBYTES 32U
#define crypto_aead_xchacha20poly1305_ietf_NPUBBYTES 24U
#define crypto_aead_xchacha20poly1305_ietf_ABYTES 16U

int sodium_init(void);
void sodium_memzero(void *buffer, size_t length);
void randombytes_buf(void *buffer, size_t size);
int crypto_generichash(unsigned char *out, size_t outlen,
                       const unsigned char *in, unsigned long long inlen,
                       const unsigned char *key, size_t keylen);
int crypto_aead_xchacha20poly1305_ietf_encrypt(
    unsigned char *ciphertext, unsigned long long *ciphertext_length,
    const unsigned char *message, unsigned long long message_length,
    const unsigned char *additional_data,
    unsigned long long additional_data_length, const unsigned char *secret_nonce,
    const unsigned char *public_nonce, const unsigned char *key);
int crypto_aead_xchacha20poly1305_ietf_decrypt(
    unsigned char *message, unsigned long long *message_length,
    unsigned char *secret_nonce, const unsigned char *ciphertext,
    unsigned long long ciphertext_length, const unsigned char *additional_data,
    unsigned long long additional_data_length, const unsigned char *public_nonce,
    const unsigned char *key);

#endif
