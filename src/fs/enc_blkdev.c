#include "enc_blkdev.h"

#include "blkdev.h"
#include "console.h"
#include "crypto.h"
#include "memory.h"
#include "string.h"

typedef struct __attribute__((packed)) {
    u8 salt[CRYPTO_SALT_SIZE];
    u32 iterations;
    u8 key_checksum[32];
    u8 reserved[444];
} enc_header_t;

typedef struct {
    blkdev_t base;
    u8 key[CRYPTO_AES256_XTS_KEY_SIZE];
} enc_blkdev_priv_t;

static enc_blkdev_priv_t *g_priv = NULL;

static int enc_blkdev_read(blkdev_t *dev, u32 lba, u32 count, void *buffer) {
    enc_blkdev_priv_t *priv = (enc_blkdev_priv_t *)dev->private;

    if (lba + 1 + count > priv->base.total_sectors) {
        return -1;
    }

    if (priv->base.ops->read(&priv->base, lba + 1, count, buffer) != 0) {
        return -1;
    }

    for (u32 i = 0; i < count; i++) {
        crypto_decrypt_sector(priv->key,
                              (u8 *)buffer + i * CRYPTO_SECTOR_SIZE,
                              lba + i);
    }

    return 0;
}

static int enc_blkdev_write(blkdev_t *dev, u32 lba, u32 count, const void *buffer) {
    enc_blkdev_priv_t *priv = (enc_blkdev_priv_t *)dev->private;
    u8 *tmp = (u8 *)kmalloc(count * CRYPTO_SECTOR_SIZE);
    if (!tmp) return -1;
    memcpy(tmp, buffer, count * CRYPTO_SECTOR_SIZE);

    for (u32 i = 0; i < count; i++) {
        crypto_encrypt_sector(priv->key,
                              tmp + i * CRYPTO_SECTOR_SIZE,
                              lba + i);
    }

    int ret = priv->base.ops->write(&priv->base, lba + 1, count, tmp);
    kfree(tmp);
    return ret;
}

static const blkdev_ops_t enc_blkdev_ops = {
    .read = enc_blkdev_read,
    .write = enc_blkdev_write,
};

bool enc_blkdev_init(u32 base_id, const char *passphrase) {
    blkdev_t *base = blkdev_get(base_id);
    if (!base) {
        console_printf("[enc] base device %u not present\n", base_id);
        return false;
    }

    u8 raw_header[CRYPTO_SECTOR_SIZE];
    if (base->ops->read(base, 0, 1, raw_header) != 0) {
        console_printf("[enc] failed to read header sector\n");
        return false;
    }

    enc_header_t *hdr = (enc_header_t *)raw_header;
    u32 iterations = hdr->iterations;

    u8 key[CRYPTO_DERIVED_KEY_SIZE];
    if (!crypto_derive_key(passphrase, strlen(passphrase),
                           hdr->salt, iterations, key)) {
        console_printf("[enc] key derivation failed\n");
        return false;
    }

    u8 checksum[32];
    crypto_sha256(key, CRYPTO_DERIVED_KEY_SIZE, checksum);
    if (!crypto_constant_time_equal(checksum, hdr->key_checksum, 32)) {
        console_printf("[enc] wrong passphrase\n");
        crypto_secure_zero(key, sizeof(key));
        return false;
    }

    enc_blkdev_priv_t *priv = (enc_blkdev_priv_t *)kmalloc(sizeof(enc_blkdev_priv_t));
    if (!priv) {
        crypto_secure_zero(key, sizeof(key));
        return false;
    }

    memcpy(&priv->base, base, sizeof(blkdev_t));
    memcpy(priv->key, key, CRYPTO_AES256_XTS_KEY_SIZE);
    crypto_secure_zero(key, sizeof(key));

    g_priv = priv;

    blkdev_t *slot = blkdev_get(base_id);
    slot->present = true;
    slot->total_sectors = base->total_sectors - 1;
    slot->ops = &enc_blkdev_ops;
    slot->private = priv;

    console_printf("[enc] disk unlocked, %u sectors available\n",
                   (u32)slot->total_sectors);

    return true;
}
