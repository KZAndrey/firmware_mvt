#pragma once
#include <cstddef>
#include <cstdint>
int mbedtls_base64_decode(uint8_t *,size_t,size_t *,const uint8_t *,size_t);
