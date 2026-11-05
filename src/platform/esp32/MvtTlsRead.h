#pragma once
#include <stdbool.h>
#include "esp_transport.h"
#ifdef __cplusplus
extern "C" {
#endif
bool mvt_tls_read_register(esp_transport_handle_t transport);
void mvt_tls_read_unregister(esp_transport_handle_t transport);
#ifdef __cplusplus
}
#endif
