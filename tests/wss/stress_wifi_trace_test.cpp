#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
#define MALLOC_CAP_DMA 8
struct esp_backtrace_frame_t {uint32_t pc,sp,next_pc;const void *exc_frame;};
static unsigned remaining=4;
static void esp_backtrace_get_start(uint32_t *pc,uint32_t *sp,uint32_t *next){*pc=100;*sp=200;*next=101;}
static bool esp_backtrace_get_next_frame(esp_backtrace_frame_t *f){
    if(!remaining)return false;
    --remaining;++f->pc;f->next_pc=remaining?f->pc+1:0;return true;
}
#include "../../src/platform/esp32/mvt_stress_wifi_trace.inc"
int main(){
    uint32_t pcs[16]={},meta[3]={};
    mvt_stress_capture_wifi_trace(1,8,nullptr,1);
    mvt_stress_capture_wifi_trace(1,8,"tiT",1);
    mvt_stress_capture_wifi_trace(1,4,"wifi",1);
    assert(mvt_stress_wifi_trace_snapshot(pcs,meta)==0);
    mvt_stress_capture_wifi_trace(1590,2060,"wifi",123);
    assert(mvt_stress_wifi_trace_snapshot(pcs,meta)==5&&pcs[0]==100&&pcs[4]==104);
    assert(meta[0]==1590&&meta[1]==2060&&meta[2]==123);
    mvt_stress_capture_wifi_trace(999,2060,"wifi",456);
    assert(mvt_stress_wifi_trace_snapshot(pcs,meta)==5&&meta[0]==1590&&meta[2]==123);
    mvt_wifi_trace_state=0;mvt_wifi_trace_count=0;remaining=100;
    mvt_stress_capture_wifi_trace(1590,2060,"wifi",789);
    assert(mvt_stress_wifi_trace_snapshot(pcs,meta)==16&&pcs[15]==115);
    mvt_wifi_trace_state=0;mvt_wifi_trace_count=0;remaining=0;
    mvt_stress_capture_wifi_trace(1590,2060,"wifi",790);
    assert(mvt_stress_wifi_trace_snapshot(pcs,meta)==1);
    puts("PASS WiFi allocation trace: first failure retained, task/capability filter, valid/invalid stack end and 16-frame bound");
}
