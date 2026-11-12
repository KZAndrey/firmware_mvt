#include "configuration.h"
#if HAS_MVT_WSS_GATEWAY && MVT_WSS_STRESS_TEST && defined(ESP_PLATFORM)
#include "modules/MvtWssStressControl.h"
#include "esp_wifi.h"
#include "esp_log.h"
extern "C" esp_err_t __real_esp_wifi_init(const wifi_init_config_t *);
extern "C" esp_err_t __wrap_esp_wifi_init(const wifi_init_config_t *original){
    if(!original)return __real_esp_wifi_init(original);
    auto config=*original;
    const unsigned limit=mvtStressTxLimit.load();
    // Change exactly one parameter for the USB A/B trial. Static TX and all
    // RX/AMPDU/crypto/NVS fields remain as supplied by the pinned Arduino SDK.
    if(limit==4&&config.tx_buf_type==1)config.dynamic_tx_buf_num=4;
    // Separate candidate: bound RX as well, with an RX BA window below the
    // dynamic count and >=6 static RX buffers as recommended for AMPDU.
    if(limit==8&&config.tx_buf_type==1){
        config.dynamic_tx_buf_num=4;
        config.dynamic_rx_buf_num=8;
        config.static_rx_buf_num=6;
        config.rx_ba_win=6;
    }
    if(limit==16){
        config.tx_buf_type=0;
        config.static_tx_buf_num=4;
        config.cache_tx_buf_num=4;
    }
    // Separate USB trial: two static TX buffers; preserve SDK cache count.
    if(limit==32){config.tx_buf_type=0;config.static_tx_buf_num=2;}
    ESP_LOGW("MVT_NET","config tx_type=%d tx_dynamic=%d rx_static=%d rx_dynamic=%d ba=%d limit=%u tx_static=%d tx_cache=%d",
        config.tx_buf_type,config.dynamic_tx_buf_num,config.static_rx_buf_num,config.dynamic_rx_buf_num,config.rx_ba_win,limit,
        config.static_tx_buf_num,config.cache_tx_buf_num);
    return __real_esp_wifi_init(&config);
}
#endif
