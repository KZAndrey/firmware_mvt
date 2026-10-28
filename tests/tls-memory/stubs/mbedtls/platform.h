#pragma once
#include <cstddef>
extern "C" {
int mbedtls_platform_set_calloc_free(void *(*alloc)(size_t, size_t), void (*release)(void *));
}
