#ifndef OHOS_CRYPTO_H
#define OHOS_CRYPTO_H

#include <stddef.h>

#include "types.h"

void crypto_sha256(const void *data, size_t length, u8 digest[32]);
bool crypto_constant_time_equal(const void *left, const void *right, size_t length);

#endif
