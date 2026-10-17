// Compile the production OTA worker against fault-injection hardware adapters.
// Crypto is accepted by the adapter: this test exercises cancellation/cleanup,
// not cryptographic verification or physical flash/power-loss recovery.
#include "configuration.h"
#include "Arduino.h"
#include "NodeDB.h"
#include "modules/MvtOtaUpdater.h"
#include "mbedtls/pk.h"
#include "mbedtls/sha256.h"
#include <cassert>
#include <cstring>
#include <vector>
#include <string>

TestModuleConfig moduleConfig;
TestEsp ESP;
TestNodeDB db;
TestNodeDB *nodeDB=&db;
static const esp_partition_t running{0x330000},target{0x330000};
static void (*task)(void *)=nullptr;
static void *taskContext=nullptr;
static uint32_t clockMs=0;
static int mode=0,receives=0,opens=0,aborts=0,writes=0,boots=0,cryptoLive=0,shaLive=0,deletes=0;
struct Queue {size_t size=0;bool full=false;std::vector<uint8_t> data;};
static Queue queue;
struct Chunk {uint32_t offset;size_t length;uint8_t data[2048];};
uint32_t millis(){return ++clockMs;}
void vTaskDelay(unsigned n){clockMs+=n;}
int xTaskCreate(void (*f)(void *),const char *,unsigned,void *p,unsigned,TaskHandle_t *h){task=f;taskContext=p;*h=p;return pdPASS;}
void vTaskDelete(void *){++deletes;assert(cryptoLive==0&&shaLive==0&&opens==0);}
QueueHandle_t xQueueCreate(unsigned count,size_t size){assert(count==1&&size==sizeof(Chunk));queue.size=size;queue.data.resize(size);return &queue;}
int xQueueReset(QueueHandle_t){queue.full=false;return pdPASS;}
int xQueueOverwrite(QueueHandle_t,const void *p){memcpy(queue.data.data(),p,queue.size);queue.full=true;return pdPASS;}
int xQueueSend(QueueHandle_t,const void *p,unsigned){return xQueueOverwrite(&queue,p);}
int xQueueReceive(QueueHandle_t,void *p,unsigned){
    ++receives;
    if(mode==3&&receives==1){Chunk first{};first.length=2048;memcpy(p,&first,sizeof(first));return pdPASS;}
    mvtOtaUpdater.onDisconnect();
    assert(queue.full);memcpy(p,queue.data.data(),queue.size);queue.full=false;return pdPASS;
}
const esp_partition_t *esp_ota_get_next_update_partition(const void *){return &target;}
const esp_partition_t *esp_ota_get_running_partition(){return &running;}
int esp_ota_get_state_partition(const esp_partition_t *,int *state){*state=2;return ESP_OK;}
int esp_ota_begin(const esp_partition_t *p,size_t,esp_ota_handle_t *h){assert(p==&target);++opens;*h=1;return ESP_OK;}
int esp_ota_write(esp_ota_handle_t,const void *,size_t){assert(opens==1);++writes;return ESP_OK;}
int esp_ota_abort(esp_ota_handle_t){assert(opens==1);--opens;++aborts;return ESP_OK;}
int esp_ota_end(esp_ota_handle_t){--opens;return ESP_OK;}
int esp_ota_get_partition_description(const esp_partition_t *,esp_app_desc_t *){return ESP_OK;}
int esp_ota_set_boot_partition(const esp_partition_t *){++boots;return ESP_OK;}
int esp_ota_mark_app_valid_cancel_rollback(){return ESP_OK;}
int esp_ota_mark_app_invalid_rollback_and_reboot(){assert(false);return -1;}
void esp_restart(){assert(false);}
int mbedtls_base64_decode(uint8_t *out,size_t capacity,size_t *n,const uint8_t *,size_t){assert(capacity>=256);memset(out,0,256);*n=256;return 0;}
void mbedtls_pk_init(mbedtls_pk_context *){++cryptoLive;}
int mbedtls_pk_parse_public_key(mbedtls_pk_context *,const uint8_t *,size_t){return 0;}
int mbedtls_pk_verify(mbedtls_pk_context *,int,const uint8_t *,size_t,const uint8_t *,size_t){if(mode==1)mvtOtaUpdater.onDisconnect();return 0;}
void mbedtls_pk_free(mbedtls_pk_context *){--cryptoLive;}
int mbedtls_sha256_ret(const uint8_t *,size_t,uint8_t *out,int){memset(out,0,32);return 0;}
void mbedtls_sha256_init(mbedtls_sha256_context *){++shaLive;}
int mbedtls_sha256_starts_ret(mbedtls_sha256_context *,int){return 0;}
int mbedtls_sha256_update_ret(mbedtls_sha256_context *,const uint8_t *,size_t){return 0;}
int mbedtls_sha256_finish_ret(mbedtls_sha256_context *,uint8_t *out){memset(out,0,32);return 0;}
void mbedtls_sha256_free(mbedtls_sha256_context *){--shaLive;}

static cJSON *command(){
    cJSON *j=cJSON_CreateObject(),*m=cJSON_CreateObject();
    cJSON_AddStringToObject(j,"transfer","wss-chunks-v2");cJSON_AddStringToObject(j,"roomId","ROOM1");
    cJSON_AddNumberToObject(j,"nodeId",db.getNodeNum());cJSON_AddStringToObject(j,"jobId","test-job");
    cJSON_AddItemToObject(j,"manifest",m);cJSON_AddNumberToObject(m,"schema",1);
    cJSON_AddStringToObject(m,"model","heltec-v3");cJSON_AddStringToObject(m,"chip","esp32s3");
    cJSON_AddStringToObject(m,"releaseId","test-release");cJSON_AddStringToObject(m,"version","2.7.15.test");
    cJSON_AddNumberToObject(m,"generation",MVT_OTA_GENERATION+1);cJSON_AddNumberToObject(m,"size",4096);
    cJSON_AddStringToObject(m,"sha256",std::string(64,'0').c_str());
    cJSON_AddStringToObject(m,"signature",std::string(344,'A').c_str());
    cJSON_AddStringToObject(m,"imagePath","/firmware/heltec-v3/test-release.bin");return j;
}
int main(){
    mvtOtaUpdater.begin();
    for(mode=0;mode<4;++mode){
        receives=opens=aborts=writes=boots=cryptoLive=shaLive=deletes=0;
        cJSON *j=command();assert(mvtOtaUpdater.start(j,"ROOM1",db.getNodeNum()));
        assert(mvtOtaUpdater.workerRunning());
        assert(!mvtOtaUpdater.start(j,"ROOM1",db.getNodeNum()));
        if(mode==0)mvtOtaUpdater.onDisconnect();
        task(taskContext);
        assert(!mvtOtaUpdater.workerRunning()&&!mvtOtaUpdater.active());
        assert(opens==0&&boots==0&&cryptoLive==0&&shaLive==0&&deletes==1);
        assert(aborts==(mode>=2?1:0));assert(writes==(mode==3?1:0));
        cJSON *s=mvtOtaUpdater.status(db.getNodeNum());
        assert(strcmp(cJSON_GetObjectItemCaseSensitive(s,"error")->valuestring,"WSS_DISCONNECTED")==0);
        cJSON_Delete(s);cJSON_Delete(j);
    }
    puts("Production OTA cancellation: before start, crypto verification, waiting chunk, partial flash; resources released and boot unchanged: PASS");
}
