#ifndef OES_SHA256_H
#define OES_SHA256_H

#include <stddef.h>

void sha256_hex(const char* data, size_t len, char out[65]);
int sha256_self_test(void);

#endif
