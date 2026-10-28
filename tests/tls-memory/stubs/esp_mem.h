#pragma once
#include <cstddef>
extern "C" {
void *esp_mbedtls_mem_calloc(size_t count, size_t size);
void esp_mbedtls_mem_free(void *pointer);
}
