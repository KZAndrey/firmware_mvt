#include <cassert>
#include <cstdio>
#include <initializer_list>
#include "../../src/platform/esp32/MvtStressWifiTx.cpp"
static int expected;
static char payload,reference;
extern "C" int __real_esp_wifi_internal_tx(wifi_interface_t i,void *p,uint16_t n){
    assert(i==7&&p==&payload&&n==123);return expected;
}
extern "C" esp_err_t __real_esp_wifi_internal_tx_by_ref(wifi_interface_t i,void *p,size_t n,void *b){
    assert(i==7&&p==&payload&&n==123&&b==&reference);return expected;
}
int main(){
    uint32_t v[4];mvt_stress_wifi_tx_snapshot(v);assert(v[0]==0);
    for(int rc:{ESP_OK,ESP_ERR_NO_MEM,-17}){
        expected=rc;
        assert(__wrap_esp_wifi_internal_tx(7,&payload,123)==rc);
        assert(__wrap_esp_wifi_internal_tx_by_ref(7,&payload,123,&reference)==rc);
    }
    mvt_stress_wifi_tx_snapshot(v);
    assert(v[0]==6&&v[1]==4&&v[2]==2&&v[3]==uint32_t(-17));
    puts("PASS WiFi transmit probe: exact arguments/results preserved; success, memory and other errors counted");
}
