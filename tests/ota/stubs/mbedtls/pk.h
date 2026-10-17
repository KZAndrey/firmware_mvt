#pragma once
#include <cstddef>
#include <cstdint>
constexpr int MBEDTLS_MD_SHA256=1;
struct mbedtls_pk_context {};
void mbedtls_pk_init(mbedtls_pk_context *);
int mbedtls_pk_parse_public_key(mbedtls_pk_context *,const uint8_t *,size_t);
int mbedtls_pk_verify(mbedtls_pk_context *,int,const uint8_t *,size_t,const uint8_t *,size_t);
void mbedtls_pk_free(mbedtls_pk_context *);
