#include "configuration.h"
#if HAS_MVT_WSS_GATEWAY && MVT_WSS_STRESS_TEST && defined(ESP_PLATFORM)
#include "MvtWssStressNetwork.h"
#include "MvtWssStressControl.h"
#include "MvtWssStressMemory.h"
#include "platform/esp32/MvtWssDiagnostics.h"
#include "WiFi.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <unistd.h>
extern "C" void mvt_stress_alloc_snapshot(uint32_t values[8],char task[16]);
extern "C" void mvt_stress_wifi_tx_snapshot(uint32_t values[4]);
extern "C" unsigned mvt_stress_wifi_trace_snapshot(uint32_t pcs[16],uint32_t meta[3]);

namespace {
int probe=-1, phase=0;
uint32_t peer=0, dns=0, started=0, expires=0, lastAutomatic=0, seen=0;
const uint8_t query[]={0x4d,0x56,1,0,0,1,0,0,0,0,0,0,8,'m','o','v','e','t','a','i','n',3,'a','p','p',0,0,1,0,1};
void closeProbe(){if(probe>=0){close(probe);probe=-1;}phase=0;}
void tcpStart(){
    closeProbe();
    if(!peer){ESP_LOGW("MVT_NET","tcp skipped no_successful_peer");return;}
    probe=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(probe<0){ESP_LOGW("MVT_NET","tcp socket_failed errno=%d",errno);return;}
    if(fcntl(probe,F_SETFL,O_NONBLOCK)<0){ESP_LOGW("MVT_NET","tcp nonblock_failed errno=%d",errno);closeProbe();return;}
    sockaddr_in remote{};remote.sin_family=AF_INET;remote.sin_port=htons(443);remote.sin_addr.s_addr=peer;
    started=millis();expires=started+5000;phase=2;
    const int rc=connect(probe,reinterpret_cast<sockaddr*>(&remote),sizeof(remote));
    const int error=errno;
    ESP_LOGW("MVT_NET","tcp start peer_hex=%08x rc=%d errno=%d",unsigned(peer),rc,rc<0?error:0);
    if(rc==0){ESP_LOGW("MVT_NET","tcp success elapsed=0");closeProbe();}
    else if(error!=EINPROGRESS&&error!=EWOULDBLOCK){closeProbe();}
}
}
void mvtStressNetSnapshot(bool rememberPeer){
    mvtStressMemoryReport();
    const int saved=errno;
    unsigned sockets=0, connected=0;
    for(int fd=LWIP_SOCKET_OFFSET;fd<FD_SETSIZE;++fd){
        sockaddr_in local{},remote{};socklen_t n=sizeof(local);
        if(getsockname(fd,reinterpret_cast<sockaddr*>(&local),&n)!=0)continue;
        ++sockets;n=sizeof(remote);
        if(getpeername(fd,reinterpret_cast<sockaddr*>(&remote),&n)==0){
            ++connected;
            if(rememberPeer&&remote.sin_family==AF_INET&&ntohs(remote.sin_port)==443)peer=remote.sin_addr.s_addr;
        }
    }
    wifi_ps_type_t ps=WIFI_PS_NONE;const int psResult=esp_wifi_get_ps(&ps);
    wifi_ap_record_t ap{};const int apResult=esp_wifi_sta_get_ap_info(&ap);
    ESP_LOGW("MVT_NET","state uptime=%u wifi=%d ip_hex=%08x gw_hex=%08x dns_hex=%08x sockets=%u connected=%u",
        unsigned(millis()),int(WiFi.status()),unsigned(uint32_t(WiFi.localIP())),unsigned(uint32_t(WiFi.gatewayIP())),
        unsigned(uint32_t(WiFi.dnsIP())),sockets,connected);
    ESP_LOGW("MVT_NET","radio ap_rc=%d rssi=%d channel=%u ps_rc=%d ps=%d peer_hex=%08x",
        apResult,apResult==ESP_OK?ap.rssi:0,unsigned(ap.primary),psResult,int(ps),unsigned(peer));
    esp_netif_t *netif=esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_dhcp_status_t dhcp=ESP_NETIF_DHCP_INIT;
    const int dhcpResult=netif?esp_netif_dhcpc_get_status(netif,&dhcp):-1;
    ESP_LOGW("MVT_NET","interface present=%u up=%u dhcp_rc=%d dhcp=%d bssid_tail=%02x%02x%02x%02x",
        unsigned(netif!=nullptr),unsigned(netif&&esp_netif_is_netif_up(netif)),dhcpResult,int(dhcp),
        ap.bssid[2],ap.bssid[3],ap.bssid[4],ap.bssid[5]);
    uint32_t failures[8];char task[16];mvt_stress_alloc_snapshot(failures,task);
    ESP_LOGW("MVT_NET","alloc_total=%u wifi=%u tcpip=%u wss=%u registered=%u",unsigned(failures[0]),unsigned(failures[1]),unsigned(failures[2]),unsigned(failures[3]),unsigned(failures[7]));
    ESP_LOGW("MVT_NET","alloc_last bytes=%u caps=%u uptime=%u task=%s",unsigned(failures[4]),unsigned(failures[5]),unsigned(failures[6]),task);
    errno=saved;
    uint32_t tx[4];mvt_stress_wifi_tx_snapshot(tx);
    ESP_LOGW("MVT_NET","driver_tx calls=%u failures=%u no_mem=%u last_error=%u",unsigned(tx[0]),unsigned(tx[1]),unsigned(tx[2]),unsigned(tx[3]));
    constexpr uint32_t dmaCaps=MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA|MALLOC_CAP_8BIT;
    ESP_LOGW("MVT_NET","dma free=%u largest=%u minimum=%u",unsigned(heap_caps_get_free_size(dmaCaps)),unsigned(heap_caps_get_largest_free_block(dmaCaps)),unsigned(heap_caps_get_minimum_free_size(dmaCaps)));
    static bool tracePrinted=false;
    uint32_t pcs[16],meta[3];unsigned count=mvt_stress_wifi_trace_snapshot(pcs,meta);
    if(count&&!tracePrinted){
        tracePrinted=true;
        ESP_LOGW("MVT_NET","first_wifi_alloc bytes=%u caps=%u uptime=%u frames=%u",unsigned(meta[0]),unsigned(meta[1]),unsigned(meta[2]),count);
        for(unsigned i=0;i<count;++i)ESP_LOGW("MVT_NET","wifi_alloc_trace index=%u pc=%08x",i,unsigned(pcs[i]));
    }
    errno=saved;
}
void mvtStressNetStart(){
    if(phase)return;
    mvtStressNetSnapshot(false);
    if(WiFi.status()!=WL_CONNECTED){ESP_LOGW("MVT_NET","probe skipped wifi_not_connected");return;}
    dns=uint32_t(WiFi.dnsIP());
    probe=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(probe<0){ESP_LOGW("MVT_NET","dns socket_failed errno=%d",errno);tcpStart();return;}
    if(fcntl(probe,F_SETFL,O_NONBLOCK)<0){ESP_LOGW("MVT_NET","dns nonblock_failed errno=%d",errno);tcpStart();return;}
    sockaddr_in remote{};remote.sin_family=AF_INET;remote.sin_port=htons(53);remote.sin_addr.s_addr=dns;
    started=millis();expires=started+5000;phase=1;
    const int rc=sendto(probe,query,sizeof(query),0,reinterpret_cast<sockaddr*>(&remote),sizeof(remote));
    ESP_LOGW("MVT_NET","dns direct_query rc=%d errno=%d",rc,rc<0?errno:0);
    if(rc!=sizeof(query))tcpStart();
}
void mvtStressNetTick(){
    const uint32_t now=millis();
    if(!phase){
        MvtWssDiagnostic d{};
        if(mvt_wss_latest_diagnostic(&d)&&d.sequence!=seen&&uint32_t(now-lastAutomatic)>30000){
            seen=d.sequence;lastAutomatic=now;mvtStressNetStart();
        }
        return;
    }
    if(phase==1){
        uint8_t response[512];sockaddr_in source{};socklen_t n=sizeof(source);
        const int rc=recvfrom(probe,response,sizeof(response),MSG_DONTWAIT,reinterpret_cast<sockaddr*>(&source),&n);
        if(rc>=12&&source.sin_addr.s_addr==dns&&ntohs(source.sin_port)==53&&response[0]==0x4d&&response[1]==0x56&&(response[2]&0x80)){
            ESP_LOGW("MVT_NET","dns direct_reply bytes=%d rcode=%u answers=%u elapsed=%u",rc,unsigned(response[3]&15),unsigned((response[6]<<8)|response[7]),unsigned(now-started));tcpStart();return;
        }
        if(!mvtStressBefore(now,expires)){ESP_LOGW("MVT_NET","dns direct_timeout elapsed=%u",unsigned(now-started));tcpStart();}
    }else{
        fd_set w,e;FD_ZERO(&w);FD_ZERO(&e);FD_SET(probe,&w);FD_SET(probe,&e);timeval zero{};
        if(select(probe+1,nullptr,&w,&e,&zero)>0){
            int error=0;socklen_t n=sizeof(error);const int rc=getsockopt(probe,SOL_SOCKET,SO_ERROR,&error,&n);
            ESP_LOGW("MVT_NET","tcp result rc=%d errno=%d elapsed=%u",rc,rc<0?errno:error,unsigned(now-started));closeProbe();
        }else if(!mvtStressBefore(now,expires)){ESP_LOGW("MVT_NET","tcp timeout elapsed=%u",unsigned(now-started));closeProbe();}
    }
}
void mvtStressNetCancel(){closeProbe();}
#endif
