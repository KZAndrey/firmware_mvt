#if HAS_MVT_WSS_GATEWAY
#include "MvtTlsRead.h"
#include "MvtTlsReadRetry.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cerrno>
// The firmware has one gateway client. Match its SSL parent only: never retry
// at the WS layer (partial headers), or alter unrelated HTTP/BLE/TLS callers.
static esp_transport_handle_t tlsParent;
extern "C" bool mvt_tls_read_register(esp_transport_handle_t t) {
    esp_transport_handle_t empty=nullptr;
    return t && __atomic_compare_exchange_n(&tlsParent,&empty,t,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
extern "C" void mvt_tls_read_unregister(esp_transport_handle_t t) {
    if(t) __atomic_compare_exchange_n(&tlsParent,&t,nullptr,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE);
}
extern "C" int __real_esp_transport_read(esp_transport_handle_t,char*,int,int);
// SDK --wrap references require a real externally callable entry point.
// The LTO build produced an entry inside websocket_task and a hardware panic.
// The pre-build rule also compiles this adapter without LTO; verify the ELF.
extern "C"
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((used, noinline, externally_visible))
#endif
int __wrap_esp_transport_read(esp_transport_handle_t t,char* buffer,int len,int timeout) {
    if(!t || t!=__atomic_load_n(&tlsParent,__ATOMIC_ACQUIRE)) return __real_esp_transport_read(t,buffer,len,timeout);
    const uint32_t started=uint32_t(esp_timer_get_time()/1000);
    unsigned retries=0;
    const int result=mvtTlsReadRetry(timeout,[&](int remaining){errno=0;return __real_esp_transport_read(t,buffer,len,remaining);},
        []{return uint32_t(esp_timer_get_time()/1000);},[]{vTaskDelay(1);},retries);
    if(retries || result==-0x6900) ESP_LOGI("MVT_TLS_READ","read retries=%u result=%d elapsed_ms=%u budget_ms=%d",retries,result,unsigned(uint32_t(esp_timer_get_time()/1000)-started),timeout);
    return result;
}
extern "C" int __real_esp_transport_write(esp_transport_handle_t,const char*,int,int);
extern "C"
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((used, noinline, externally_visible))
#endif
int __wrap_esp_transport_write(esp_transport_handle_t t,const char* buffer,int len,int timeout) {
    if(!t || t!=__atomic_load_n(&tlsParent,__ATOMIC_ACQUIRE)) return __real_esp_transport_write(t,buffer,len,timeout);
    const uint32_t started=uint32_t(esp_timer_get_time()/1000);
    unsigned retries=0;
    // mbedTLS requires identical buffer/length after WANT_WRITE or WANT_READ.
    // Stay below WS framing; never restart a header or append another frame.
    const int result=mvtTlsWriteRetry(timeout,[&](int remaining){errno=0;return __real_esp_transport_write(t,buffer,len,remaining);},
        []{return uint32_t(esp_timer_get_time()/1000);},[]{vTaskDelay(1);},retries);
    if(retries || result==-0x6880 || result==-0x6900) ESP_LOGI("MVT_TLS_WRITE","write retries=%u result=%d elapsed_ms=%u budget_ms=%d bytes=%d",retries,result,unsigned(uint32_t(esp_timer_get_time()/1000)-started),timeout,len);
    return result;
}
#endif
