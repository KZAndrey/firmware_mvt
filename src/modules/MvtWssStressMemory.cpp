#include "configuration.h"
#if HAS_MVT_WSS_GATEWAY && MVT_WSS_STRESS_TEST && defined(ESP_PLATFORM)
#include "MvtWssStressMemory.h"
#include "MvtWssMemoryTrace.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
static MvtMemoryTrace trace;
void mvtStressMemoryMark(unsigned phase,unsigned bytes){
    if(trace.frozenAt.load())return;
    const unsigned caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA|MALLOC_CAP_8BIT;
    trace.record({phase,uint32_t(esp_timer_get_time()/1000),
        uint32_t(heap_caps_get_free_size(caps)),uint32_t(heap_caps_get_largest_free_block(caps)),bytes});
}
// Allocator diagnostics use FreeRTOS ticks, whose origin differs from millis.
// Use the same esp_timer clock as the phase samples for exact correlation.
extern "C" void mvt_stress_memory_freeze(uint32_t){
    if(trace.frozenAt.load())return;
    // ESP-IDF 4.4.7 invokes its failure callback after multi_heap_malloc has
    // released the heap lock. Read statistics only: no logging or allocation.
    // This is a post-failure snapshot, not an atomic view of all concurrent heaps.
    mvtStressMemoryMark(18);
    trace.freeze(uint32_t(esp_timer_get_time()/1000));
}
void mvtStressMemoryReport(){
    static bool printed=false;
    const uint32_t frozen=trace.frozenAt.load();
    if(!frozen||printed)return;
    printed=true;
    ESP_LOGW("MVT_NET","memory_trace frozen=%u storage=%u",unsigned(frozen),unsigned(sizeof(trace)));
    MvtMemoryStat s;
    for(unsigned i=0;i<20;++i)if(trace.readStat(i,s)&&s.calls)
        ESP_LOGW("MVT_NET","memory_stat phase=%u calls=%u min_dma=%u min_largest=%u max_bytes=%u",i,unsigned(s.calls),unsigned(s.free),unsigned(s.largest),unsigned(s.bytes));
    MvtMemoryPoint p;
    for(unsigned i=0;i<32;++i)if(trace.readPoint(i,p))
        ESP_LOGW("MVT_NET","memory_point phase=%u uptime=%u dma=%u largest=%u bytes=%u",unsigned(p.phase),unsigned(p.uptime),unsigned(p.free),unsigned(p.largest),unsigned(p.bytes));
}
#endif
