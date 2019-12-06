#ifndef OHOS_CRYPTO_H
#define OHOS_CRYPTO_H

#include <stddef.h>
#include "types.h"

#define CRYPTO_AES256_XTS_KEY_SIZE  64
#define CRYPTO_SECTOR_SIZE          512
#define CRYPTO_SALT_SIZE            32
#define CRYPTO_DERIVED_KEY_SIZE     64

void crypto_sha256(const void *data, size_t length, u8 digest[32]);
bool crypto_constant_time_equal(const void *left, const void *right, size_t length);

void crypto_init(void);

bool crypto_derive_key(const char *passphrase, size_t pass_len,
                       const u8 salt[CRYPTO_SALT_SIZE], u32 iterations,
                       u8 key[CRYPTO_DERIVED_KEY_SIZE]);

void crypto_encrypt_sector(const u8 key[CRYPTO_AES256_XTS_KEY_SIZE],
                           u8 sector[CRYPTO_SECTOR_SIZE], u64 sector_num);
void crypto_decrypt_sector(const u8 key[CRYPTO_AES256_XTS_KEY_SIZE],
                           u8 sector[CRYPTO_SECTOR_SIZE], u64 sector_num);

void crypto_secure_zero(void *ptr, size_t len);

#endif
