#ifndef SNAKE_SCORE_CRYPTO_H
#define SNAKE_SCORE_CRYPTO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Det aktive topplisteformatet bruker autentisert XChaCha20-Poly1305. */
bool score_crypto_encrypt(const uint8_t *plaintext, size_t plaintext_length,
                          const uint8_t nonce[24], uint8_t *ciphertext,
                          unsigned long long *ciphertext_length);
bool score_crypto_decrypt(const uint8_t *ciphertext, size_t ciphertext_length,
                          const uint8_t nonce[24], uint8_t *plaintext,
                          unsigned long long *plaintext_length);

#endif
