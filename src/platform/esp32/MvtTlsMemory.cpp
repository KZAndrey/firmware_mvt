#include "configuration.h"
#if HAS_MVT_WSS_GATEWAY
#include "MvtTlsMemory.h"
#include "MvtTlsBufferPool.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
extern "C" {
#include "mbedtls/platform.h"
#include "mbedtls/ssl_internal.h"
#include "esp_mem.h"
}

static_assert(MBEDTLS_SSL_IN_BUFFER_LEN == MBEDTLS_SSL_OUT_BUFFER_LEN,
              "Review reserved TLS buffers when changing the pinned SDK");
static MvtTlsBufferPool<MBEDTLS_SSL_IN_BUFFER_LEN> recordBuffers;
static TaskHandle_t websocketTask;

static void *tlsCalloc(size_t count, size_t size)
{
    if (size && count > SIZE_MAX / size) return nullptr;
    const size_t bytes = count * size;
    if (!xPortInIsrContext() && xTaskGetCurrentTaskHandle() == __atomic_load_n(&websocketTask, __ATOMIC_RELAXED)) {
        if (void *buffer = recordBuffers.allocate(bytes)) {
            return buffer;
        }
    }
    // Other allocations and TLS users retain the pinned SDK's internal-memory
    // allocator. Do not redirect them to PSRAM or weaken certificate checking.
    return esp_mbedtls_mem_calloc(count, size);
}

static void tlsFree(void *pointer)
{
    if (!pointer || recordBuffers.release(pointer)) return;
    esp_mbedtls_mem_free(pointer);
}

extern "C" bool mvt_tls_memory_init(void)
{
    // Called by the single gateway control task before creating its first TLS
    // worker. Buffers already exist in internal BSS and survive reconnects.
    static bool installed = false;
    if (!installed) installed = mbedtls_platform_set_calloc_free(tlsCalloc, tlsFree) == 0;
    return installed;
}

extern "C" void mvt_tls_memory_set_task(void *task)
{
    __atomic_store_n(&websocketTask, static_cast<TaskHandle_t>(task), __ATOMIC_RELAXED);
}
#endif
