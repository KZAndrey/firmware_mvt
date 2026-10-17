#pragma once
#include <cstddef>
using esp_err_t=int;
constexpr int ESP_OK=0;
struct esp_partition_t {size_t size;};
struct esp_app_desc_t {};
using esp_ota_handle_t=unsigned;
using esp_ota_img_states_t=int;
constexpr int ESP_OTA_IMG_PENDING_VERIFY=1;
const esp_partition_t *esp_ota_get_next_update_partition(const void *);
const esp_partition_t *esp_ota_get_running_partition();
int esp_ota_get_state_partition(const esp_partition_t *,esp_ota_img_states_t *);
int esp_ota_begin(const esp_partition_t *,size_t,esp_ota_handle_t *);
int esp_ota_write(esp_ota_handle_t,const void *,size_t);
int esp_ota_abort(esp_ota_handle_t);
int esp_ota_end(esp_ota_handle_t);
int esp_ota_get_partition_description(const esp_partition_t *,esp_app_desc_t *);
int esp_ota_set_boot_partition(const esp_partition_t *);
int esp_ota_mark_app_valid_cancel_rollback();
int esp_ota_mark_app_invalid_rollback_and_reboot();
