#include <cassert>
#include <cstring>
#include <cstdio>
#include "../../src/platform/esp32/MvtStressWifiConfig.cpp"
std::atomic<unsigned> mvtStressTxLimit{0};
static wifi_init_config_t received;
static bool wasNull;
extern "C" esp_err_t __real_esp_wifi_init(const wifi_init_config_t *p){wasNull=!p;if(p)received=*p;return -123;}
int main(){
    wifi_init_config_t original{};original.tx_buf_type=1;original.dynamic_tx_buf_num=32;
    original.static_rx_buf_num=4;original.dynamic_rx_buf_num=32;original.rx_ba_win=6;
    for(unsigned i=0;i<16;++i)original.untouched[i]=0x12340000+i;
    auto saved=original;
    assert(__wrap_esp_wifi_init(&original)==-123&&!memcmp(&original,&received,sizeof(original)));
    mvtStressTxLimit=4;assert(__wrap_esp_wifi_init(&original)==-123);
    auto expected=original;expected.dynamic_tx_buf_num=4;
    assert(!memcmp(&received,&expected,sizeof(expected))&&!memcmp(&original,&saved,sizeof(saved)));
    mvtStressTxLimit=8;__wrap_esp_wifi_init(&original);
    expected.dynamic_rx_buf_num=8;expected.static_rx_buf_num=6;expected.rx_ba_win=6;
    assert(!memcmp(&received,&expected,sizeof(expected))&&!memcmp(&original,&saved,sizeof(saved)));
    original.tx_buf_type=0;__wrap_esp_wifi_init(&original);assert(!memcmp(&received,&original,sizeof(original)));
    original.tx_buf_type=1;mvtStressTxLimit=0;__wrap_esp_wifi_init(&original);assert(!memcmp(&received,&original,sizeof(original)));
    mvtStressTxLimit=16;__wrap_esp_wifi_init(&original);
    expected=original;expected.tx_buf_type=0;expected.static_tx_buf_num=4;expected.cache_tx_buf_num=4;
    assert(!memcmp(&received,&expected,sizeof(expected))&&!memcmp(&original,&saved,sizeof(saved)));
    mvtStressTxLimit=32;__wrap_esp_wifi_init(&original);
    expected=original;expected.tx_buf_type=0;expected.static_tx_buf_num=2;
    assert(!memcmp(&received,&expected,sizeof(expected))&&!memcmp(&original,&saved,sizeof(saved)));
    mvtStressTxLimit=0;__wrap_esp_wifi_init(&original);assert(!memcmp(&received,&original,sizeof(original)));
    assert(__wrap_esp_wifi_init(nullptr)==-123&&wasNull);
    puts("PASS WiFi profiles: TX-only and bounded RX/TX, original input, static TX, security fields, restore and return codes preserved");
}
