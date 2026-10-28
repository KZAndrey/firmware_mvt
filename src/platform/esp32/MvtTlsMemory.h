#pragma once
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool mvt_tls_memory_init(void);
void mvt_tls_memory_set_task(void *task);
#ifdef __cplusplus
}
#endif
