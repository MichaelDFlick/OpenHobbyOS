#include "crypto.h"

#include <stddef.h>

#include "memory.h"
#include "string.h"

#include <mbedtls/aes.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/platform.h>

static const u32 sha256_initial_state[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
};

static const u32 sha256_round_constants[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static u32 rotate_right(u32 value, unsigned int amount) {
    return (value >> amount) | (value << (32u - amount));
}

static u32 load_be32(const u8 *bytes) {
    return ((u32)bytes[0] << 24) |
           ((u32)bytes[1] << 16) |
           ((u32)bytes[2] << 8) |
           (u32)bytes[3];
}

static void store_be32(u8 *bytes, u32 value) {
    bytes[0] = (u8)(value >> 24);
    bytes[1] = (u8)(value >> 16);
    bytes[2] = (u8)(value >> 8);
    bytes[3] = (u8)value;
}

static void sha256_process_block(u32 state[8], const u8 block[64]) {
    u32 schedule[64];

    for (unsigned int index = 0; index < 16; ++index) {
        schedule[index] = load_be32(block + index * 4u);
    }

    for (unsigned int index = 16; index < 64; ++index) {
        u32 s0 = rotate_right(schedule[index - 15], 7u) ^
                 rotate_right(schedule[index - 15], 18u) ^
                 (schedule[index - 15] >> 3u);
        u32 s1 = rotate_right(schedule[index - 2], 17u) ^
                 rotate_right(schedule[index - 2], 19u) ^
                 (schedule[index - 2] >> 10u);
        schedule[index] = schedule[index - 16] + s0 + schedule[index - 7] + s1;
    }

    u32 a = state[0];
    u32 b = state[1];
    u32 c = state[2];
    u32 d = state[3];
    u32 e = state[4];
    u32 f = state[5];
    u32 g = state[6];
    u32 h = state[7];

    for (unsigned int index = 0; index < 64; ++index) {
        u32 s1 = rotate_right(e, 6u) ^ rotate_right(e, 11u) ^ rotate_right(e, 25u);
        u32 ch = (e & f) ^ ((~e) & g);
        u32 temp1 = h + s1 + ch + sha256_round_constants[index] + schedule[index];
        u32 s0 = rotate_right(a, 2u) ^ rotate_right(a, 13u) ^ rotate_right(a, 22u);
        u32 maj = (a & b) ^ (a & c) ^ (b & c);
        u32 temp2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

void crypto_sha256(const void *data, size_t length, u8 digest[32]) {
    const u8 *bytes = (const u8 *)data;
    u32 state[8];
    u8 block[64];
    size_t consumed = 0;

    for (unsigned int index = 0; index < 8; ++index) {
        state[index] = sha256_initial_state[index];
    }

    while (length - consumed >= 64u) {
        sha256_process_block(state, bytes + consumed);
        consumed += 64u;
    }

    size_t remaining = length - consumed;
    for (size_t index = 0; index < remaining; ++index) {
        block[index] = bytes[consumed + index];
    }

    block[remaining++] = 0x80u;
    while (remaining > 56u) {
        while (remaining < 64u) {
            block[remaining++] = 0x00u;
        }
        sha256_process_block(state, block);
        remaining = 0;
    }
    while (remaining < 56u) {
        block[remaining++] = 0x00u;
    }

    {
        u64 bit_length = (u64)length * 8u;
        for (int shift = 7; shift >= 0; --shift) {
            block[remaining++] = (u8)(bit_length >> (shift * 8));
        }
    }

    sha256_process_block(state, block);

    for (unsigned int index = 0; index < 8; ++index) {
        store_be32(digest + index * 4u, state[index]);
    }
}

bool crypto_constant_time_equal(const void *left, const void *right, size_t length) {
    const u8 *lhs = (const u8 *)left;
    const u8 *rhs = (const u8 *)right;
    u8 diff = 0;

    for (size_t index = 0; index < length; ++index) {
        diff |= (u8)(lhs[index] ^ rhs[index]);
    }

    return diff == 0;
}

static void *kmalloc_cb(size_t nmemb, size_t size) {
    return kcalloc(nmemb, size);
}

static void kfree_cb(void *ptr) {
    kfree(ptr);
}

void crypto_init(void) {
    mbedtls_platform_set_calloc_free(kmalloc_cb, kfree_cb);
}

bool crypto_derive_key(const char *passphrase, size_t pass_len,
                       const u8 salt[CRYPTO_SALT_SIZE], u32 iterations,
                       u8 key[CRYPTO_DERIVED_KEY_SIZE]) {
    return mbedtls_pkcs5_pbkdf2_hmac_ext(
        MBEDTLS_MD_SHA256,
        (const u8 *)passphrase, pass_len,
        salt, CRYPTO_SALT_SIZE,
        iterations,
        CRYPTO_DERIVED_KEY_SIZE,
        key
    ) == 0;
}

void crypto_encrypt_sector(const u8 key[CRYPTO_AES256_XTS_KEY_SIZE],
                           u8 sector[CRYPTO_SECTOR_SIZE], u64 sector_num) {
    mbedtls_aes_xts_context ctx;
    u8 data_unit[16];

    for (int i = 0; i < 8; i++) {
        data_unit[i] = (u8)(sector_num >> (i * 8));
    }
    for (int i = 8; i < 16; i++) {
        data_unit[i] = 0;
    }

    mbedtls_aes_xts_init(&ctx);
    mbedtls_aes_xts_setkey_enc(&ctx, key, CRYPTO_AES256_XTS_KEY_SIZE * 8);
    mbedtls_aes_crypt_xts(&ctx, MBEDTLS_AES_ENCRYPT, CRYPTO_SECTOR_SIZE,
                          data_unit, sector, sector);
    mbedtls_aes_xts_free(&ctx);
}

void crypto_decrypt_sector(const u8 key[CRYPTO_AES256_XTS_KEY_SIZE],
                           u8 sector[CRYPTO_SECTOR_SIZE], u64 sector_num) {
    mbedtls_aes_xts_context ctx;
    u8 data_unit[16];

    for (int i = 0; i < 8; i++) {
        data_unit[i] = (u8)(sector_num >> (i * 8));
    }
    for (int i = 8; i < 16; i++) {
        data_unit[i] = 0;
    }

    mbedtls_aes_xts_init(&ctx);
    mbedtls_aes_xts_setkey_dec(&ctx, key, CRYPTO_AES256_XTS_KEY_SIZE * 8);
    mbedtls_aes_crypt_xts(&ctx, MBEDTLS_AES_DECRYPT, CRYPTO_SECTOR_SIZE,
                          data_unit, sector, sector);
    mbedtls_aes_xts_free(&ctx);
}

void crypto_secure_zero(void *ptr, size_t len) {
    volatile u8 *p = (volatile u8 *)ptr;
    for (size_t i = 0; i < len; i++) {
        p[i] = 0;
    }
}
