#ifndef OHOS_ENC_BLKDEV_H
#define OHOS_ENC_BLKDEV_H

#include "types.h"

#define ENC_BLKDEV_DEFAULT_ITERATIONS 100000

bool enc_blkdev_init(u32 base_id, const char *passphrase);

#endif
