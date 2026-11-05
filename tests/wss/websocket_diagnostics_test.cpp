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
#include "../../src/platform/esp32/mvt_websocket_diagnostics.inc"
#include "../../src/modules/MvtWssNetworkRecovery.h"
int main(){
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
    puts("PASS diagnostics: empty boot, sequence, retained session failure, 10000 concurrent snapshots");
}
