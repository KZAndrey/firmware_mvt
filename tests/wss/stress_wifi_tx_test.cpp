#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "../../src/platform/esp32/MvtStressWifiTx.cpp"
static int expected;
static char payload,reference;
static unsigned heapReads;
size_t heap_caps_get_free_size(unsigned caps){assert(caps==0x808);++heapReads;errno=99;return 12000;}
size_t heap_caps_get_largest_free_block(unsigned caps){assert(caps==0x808);++heapReads;return 3000;}
size_t heap_caps_get_minimum_free_size(unsigned caps){assert(caps==0x808);++heapReads;return 100;}
int64_t esp_timer_get_time(){return 123456789;}
extern "C" void mvt_stress_alloc_snapshot(uint32_t v[8],char task[16]){
    for(unsigned i=0;i<8;++i)v[i]=i+10;task[0]=0;
    uint32_t pending[10];assert(!mvt_wifi_tx_first_failure(pending));
}
extern "C" int __real_esp_wifi_internal_tx(wifi_interface_t i,void *p,uint16_t n){
    assert(i==7&&p==&payload&&n==123);errno=37;return expected;
}
extern "C" esp_err_t __real_esp_wifi_internal_tx_by_ref(wifi_interface_t i,void *p,size_t n,void *b){
    assert(i==7&&p==&payload&&n==123&&b==&reference);errno=38;return expected;
}
int main(){
    uint32_t v[4];mvt_stress_wifi_tx_snapshot(v);assert(v[0]==0);
    uint32_t first[10];assert(!mvt_wifi_tx_first_failure(first));
    for(int rc:{ESP_OK,ESP_ERR_NO_MEM,-17}){
        expected=rc;
        assert(__wrap_esp_wifi_internal_tx(7,&payload,123)==rc);
        assert(errno==37);
        assert(__wrap_esp_wifi_internal_tx_by_ref(7,&payload,123,&reference)==rc);
        assert(errno==38);
    }
    mvt_stress_wifi_tx_snapshot(v);
    assert(v[0]==6&&v[1]==4&&v[2]==2&&v[3]==uint32_t(-17));
    assert(mvt_wifi_tx_first_failure(first));
    assert(first[0]==123456&&first[1]==12000&&first[2]==3000&&first[3]==100);
    assert(first[4]==10&&first[7]==13&&first[8]==16&&first[9]==17);
    assert(heapReads==3); // Repeated failures must not repeatedly inspect heaps.
    puts("PASS WiFi transmit probe: exact arguments/results preserved; success, memory and other errors counted");
}
