#include "configuration.h"
#if HAS_MVT_WSS_GATEWAY && (MVT_WSS_STRESS_TEST || MVT_WSS_NETWORK_OBSERVER) && defined(ESP_PLATFORM)
#include "esp_private/wifi.h"
#include <cstdint>
#include <cerrno>
#include "esp_heap_caps.h"
#include "esp_timer.h"
// Optional USB observation: forward every call unchanged and count the
// driver's result. No logging or allocation on the transmit path. Only the
// first no-memory result inspects heaps (using the SDK's heap locks).
static uint32_t calls, failures, noMemory;
static int lastError;
extern "C" void mvt_stress_alloc_snapshot(uint32_t values[8],char task[16]);
// Publish once per boot. Heap inspection is deliberately limited to the first
// no-memory result, outside the allocator hook. No payloads or credentials.
static uint32_t firstNoMemory[10];
static unsigned firstState; // 0 empty, 1 writer owns record, 2 published
extern "C" bool mvt_wifi_tx_first_failure(uint32_t values[10]) {
    if (__atomic_load_n(&firstState,__ATOMIC_ACQUIRE)!=2) return false;
    for(unsigned i=0;i<10;++i) values[i]=firstNoMemory[i];
    return true;
}
static void captureFirstFailure() {
    unsigned empty=0;
    if(!__atomic_compare_exchange_n(&firstState,&empty,1,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return;
    uint32_t alloc[8];char task[16];
    mvt_stress_alloc_snapshot(alloc,task);
    firstNoMemory[0]=uint32_t(esp_timer_get_time()/1000);
    firstNoMemory[1]=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
    firstNoMemory[2]=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
    firstNoMemory[3]=heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA);
    for(unsigned i=0;i<4;++i)firstNoMemory[4+i]=alloc[i];
    firstNoMemory[8]=alloc[6]; // Last allocation failure uptime, may be absent on contention.
    firstNoMemory[9]=alloc[7]; // Hook registration: zero counters alone do not prove no failures.
    __atomic_store_n(&firstState,2,__ATOMIC_RELEASE);
}
static void record(int result) {
    __atomic_add_fetch(&calls,1,__ATOMIC_RELAXED);
    if(result!=ESP_OK){
        __atomic_add_fetch(&failures,1,__ATOMIC_RELAXED);
        __atomic_store_n(&lastError,result,__ATOMIC_RELAXED);
        if(result==ESP_ERR_NO_MEM){
            __atomic_add_fetch(&noMemory,1,__ATOMIC_RELAXED);
            captureFirstFailure();
        }
    }
}
extern "C" int __real_esp_wifi_internal_tx(wifi_interface_t,void *,uint16_t);
extern "C" esp_err_t __real_esp_wifi_internal_tx_by_ref(wifi_interface_t,void *,size_t,void *);
extern "C" int __wrap_esp_wifi_internal_tx(wifi_interface_t i,void *p,uint16_t n){
    int result=__real_esp_wifi_internal_tx(i,p,n);const int saved=errno;record(result);errno=saved;return result;
}
extern "C" esp_err_t __wrap_esp_wifi_internal_tx_by_ref(wifi_interface_t i,void *p,size_t n,void *b){
    auto result=__real_esp_wifi_internal_tx_by_ref(i,p,n,b);const int saved=errno;record(result);errno=saved;return result;
}
extern "C" void mvt_stress_wifi_tx_snapshot(uint32_t values[4]){
    values[0]=__atomic_load_n(&calls,__ATOMIC_RELAXED);
    values[1]=__atomic_load_n(&failures,__ATOMIC_RELAXED);
    values[2]=__atomic_load_n(&noMemory,__ATOMIC_RELAXED);
    values[3]=uint32_t(__atomic_load_n(&lastError,__ATOMIC_RELAXED));
}
#endif
