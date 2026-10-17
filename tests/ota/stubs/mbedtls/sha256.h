#pragma once
#include <cstddef>
#include <cstdint>
struct mbedtls_sha256_context {};
int mbedtls_sha256_ret(const uint8_t *,size_t,uint8_t *,int);
void mbedtls_sha256_init(mbedtls_sha256_context *);
int mbedtls_sha256_starts_ret(mbedtls_sha256_context *,int);
int mbedtls_sha256_update_ret(mbedtls_sha256_context *,const uint8_t *,size_t);
int mbedtls_sha256_finish_ret(mbedtls_sha256_context *,uint8_t *);
void mbedtls_sha256_free(mbedtls_sha256_context *);
