#include "../include/score_crypto.h"

#include <string.h>

#include "../include/sodium_compat.h"

static const uint8_t FORMAT_MAGIC[8] = {'S', 'N', 'K', '1', '3', 'X', 'C', 'P'};

static void reconstruct_secret(uint8_t secret[5])
{
    /* Obfuskering hindrer kun at nøkkelfragmentet ligger som klartekst. */
    static volatile const uint8_t encoded_even[3] = {0xe6, 0x58, 0xa8};
    static volatile const uint8_t masks_even[3] = {0xa5, 0x3c, 0xf0};
    static volatile const uint8_t encoded_odd[2] = {0x34, 0xa2};
    static volatile const uint8_t masks_odd[2] = {0x5b, 0xc7};

    secret[0] = encoded_even[0] ^ masks_even[0];
    secret[2] = encoded_even[1] ^ masks_even[1];
    secret[4] = encoded_even[2] ^ masks_even[2];
    secret[1] = encoded_odd[0] ^ masks_odd[0];
    secret[3] = encoded_odd[1] ^ masks_odd[1];
}

static bool derive_key(uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES])
{
    static const uint8_t context[16] = {
        'S', 'n', 'a', 'k', 'e', ' ', 's', 'c',
        'o', 'r', 'e', 's', ' ', '1', '.', '3'
    };
    uint8_t secret[5];
    int result;

    reconstruct_secret(secret);
    result = crypto_generichash(key,
                                crypto_aead_xchacha20poly1305_ietf_KEYBYTES,
                                secret, sizeof(secret), context, sizeof(context));
    sodium_memzero(secret, sizeof(secret));
    return result == 0;
}

bool score_crypto_encrypt(const uint8_t *plaintext, size_t plaintext_length,
                          const uint8_t nonce[24], uint8_t *ciphertext,
                          unsigned long long *ciphertext_length)
{
    uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    int result;

    if (!derive_key(key)) return false;
    result = crypto_aead_xchacha20poly1305_ietf_encrypt(
        ciphertext, ciphertext_length, plaintext,
        (unsigned long long)plaintext_length, FORMAT_MAGIC,
        sizeof(FORMAT_MAGIC), NULL, nonce, key);
    sodium_memzero(key, sizeof(key));
    return result == 0;
}

bool score_crypto_decrypt(const uint8_t *ciphertext, size_t ciphertext_length,
                          const uint8_t nonce[24], uint8_t *plaintext,
                          unsigned long long *plaintext_length)
{
    uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    int result;

    if (!derive_key(key)) return false;
    result = crypto_aead_xchacha20poly1305_ietf_decrypt(
        plaintext, plaintext_length, NULL, ciphertext,
        (unsigned long long)ciphertext_length, FORMAT_MAGIC,
        sizeof(FORMAT_MAGIC), nonce, key);
    sodium_memzero(key, sizeof(key));
    return result == 0;
}
