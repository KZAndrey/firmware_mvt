#include <cassert>
#include <cstring>
#include <cstdio>
#include <mutex>
#include <thread>
#include <atomic>
#include "../../src/platform/esp32/MvtWssDiagnostics.h"
using portMUX_TYPE=std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(m) (m)->lock()
#define portEXIT_CRITICAL(m) (m)->unlock()
uint32_t esp_random(){return 12345;}
#define RTC_NOINIT_ATTR
#include "../../src/platform/esp32/mvt_websocket_diagnostics.inc"
#include "../../src/modules/MvtWssNetworkRecovery.h"
int main(){
    MvtWssBootDiagnostic boot{};
    assert(!mvt_wss_boot_diagnostic(&boot));assert(!mvt_wss_boot_diagnostic(nullptr));
    mvt_wss_boot_init(1,0,false);assert(mvt_wss_boot_diagnostic(&boot));
    assert(boot.bootCount==1&&boot.resetReason==1&&boot.sleepIntent==0);
    mvt_wss_prepare_sleep(123456,60000);
    mvt_wss_boot_init(8,4,true);mvt_wss_boot_diagnostic(&boot);
    assert(boot.bootCount==2&&boot.wakeCause==4&&boot.sleepIntent==1);
    assert(boot.previousSleepUptimeMs==123456&&boot.previousSleepDurationMs==60000);
    mvt_wss_boot_init(3,0,true);mvt_wss_boot_diagnostic(&boot);
    assert(boot.bootCount==3&&boot.sleepIntent==0&&boot.previousSleepUptimeMs==0);
    mvt_retained_power.checksum^=1;
    mvt_wss_boot_init(3,0,true);mvt_wss_boot_diagnostic(&boot);assert(boot.bootCount==1);
    mvt_wss_prepare_sleep(1,0xffffffff);
    mvt_wss_boot_init(9,0,false);mvt_wss_boot_diagnostic(&boot);assert(boot.bootCount==1&&boot.sleepIntent==0);
    MvtWssDiagnostic d={},out={};assert(!mvt_wss_latest_diagnostic(&out));assert(!mvt_wss_latest_diagnostic(nullptr));assert(!mvt_wss_last_diagnostic(&out));assert(!mvt_wss_last_diagnostic(nullptr));
    strcpy(d.stage,"connect");d.tlsError=32774;mvt_wss_save_diagnostic(&d);
    assert(mvt_wss_last_diagnostic(&out)&&out.sequence==1&&out.bootId==12345);
    strcpy(d.stage,"write");d.duringSession=1;d.error=11;mvt_wss_save_diagnostic(&d);
    assert(mvt_wss_last_diagnostic(&out)&&out.sequence==2&&out.error==11);
    MvtWssDiagnostic handshake={};strcpy(handshake.stage,"connect");handshake.tlsError=32776;
    mvt_wss_save_diagnostic(&handshake);
    assert(mvt_wss_last_diagnostic(&out)&&out.sequence==2&&strcmp(out.stage,"write")==0);
    // Hardware reproduction: retained session error followed by failed connects.
    MvtWssNetworkRecovery recovery;
    for(unsigned attempt=0;attempt<3;++attempt){
        handshake.tlsError=0x8006;handshake.uptimeMs=attempt*60000;
        mvt_wss_save_diagnostic(&handshake);
        assert(mvt_wss_latest_diagnostic(&out));
        recovery.observe(out,attempt*60000);
    }
    assert(recovery.due(120000));
    assert(mvt_wss_last_diagnostic(&out)&&strcmp(out.stage,"write")==0&&out.sequence==2);
    std::atomic<bool> done{false};
    std::thread writer([&]{for(int i=0;i<10000;++i){d.ret=i;d.offset=i;d.payload=i;mvt_wss_save_diagnostic(&d);}done=true;});
    while(!done){mvt_wss_last_diagnostic(&out);assert(out.ret==out.offset&&out.offset==out.payload);}
    writer.join();mvt_wss_latest_diagnostic(&out);assert(out.sequence==10006);mvt_wss_last_diagnostic(&out);assert(out.sequence==10002&&out.bootId==12345);
    mvt_wss_startup_failure("client_init",77,20000,4096,19000);
    mvt_wss_latest_diagnostic(&out);assert(out.freeHeap==20000&&out.largestBlock==4096&&out.minimumHeap==19000&&out.uptimeMs==77);
    assert(!strcmp(out.stage,"client_init"));
    mvt_wss_last_diagnostic(&out);assert(!strcmp(out.stage,"write"));
    puts("PASS diagnostics: empty boot, sequence, retained session failure, 10000 concurrent snapshots");
}
