#include "configuration.h"
#if HAS_MVT_WSS_GATEWAY && MVT_WSS_STRESS_TEST && defined(ESP_PLATFORM)
#include "esp_private/wifi.h"
#include <cstdint>
// USB-test instrumentation only: forward every call unchanged and count the
// driver's result. No logging, allocation or waiting on the transmit path.
static uint32_t calls, failures, noMemory;
static int lastError;
static void record(int result) {
    __atomic_add_fetch(&calls,1,__ATOMIC_RELAXED);
    if(result!=ESP_OK){
        __atomic_add_fetch(&failures,1,__ATOMIC_RELAXED);
        __atomic_store_n(&lastError,result,__ATOMIC_RELAXED);
        if(result==ESP_ERR_NO_MEM)__atomic_add_fetch(&noMemory,1,__ATOMIC_RELAXED);
    }
}
extern "C" int __real_esp_wifi_internal_tx(wifi_interface_t,void *,uint16_t);
extern "C" esp_err_t __real_esp_wifi_internal_tx_by_ref(wifi_interface_t,void *,size_t,void *);
extern "C" int __wrap_esp_wifi_internal_tx(wifi_interface_t i,void *p,uint16_t n){
    int result=__real_esp_wifi_internal_tx(i,p,n);record(result);return result;
}
extern "C" esp_err_t __wrap_esp_wifi_internal_tx_by_ref(wifi_interface_t i,void *p,size_t n,void *b){
    auto result=__real_esp_wifi_internal_tx_by_ref(i,p,n,b);record(result);return result;
}
extern "C" void mvt_stress_wifi_tx_snapshot(uint32_t values[4]){
    values[0]=__atomic_load_n(&calls,__ATOMIC_RELAXED);
    values[1]=__atomic_load_n(&failures,__ATOMIC_RELAXED);
    values[2]=__atomic_load_n(&noMemory,__ATOMIC_RELAXED);
    values[3]=uint32_t(__atomic_load_n(&lastError,__ATOMIC_RELAXED));
}
#endif
