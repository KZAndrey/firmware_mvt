#pragma once
#include <cstdint>
typedef int esp_err_t;
struct wifi_init_config_t {int tx_buf_type,dynamic_tx_buf_num,static_rx_buf_num,dynamic_rx_buf_num,rx_ba_win,static_tx_buf_num,cache_tx_buf_num;uint32_t untouched[16];};
