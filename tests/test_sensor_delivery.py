"""Compile actual delivery methods and Crypto HMAC on the host; no radio required.
Usage: python tests/test_sensor_delivery.py <C++ compiler> [compiler arguments]
"""
import pathlib, subprocess, sys, tempfile, hashlib, hmac
root = pathlib.Path(__file__).resolve().parents[1]
source = (root/'src/modules/DetectionSensorModule.cpp').read_text()
methods = source[source.index('static void deliveryMac'):source.rindex('#endif')]
triggers = source[source.index('typedef enum {'):source.index('int32_t DetectionSensorModule::runOnce()')]
trigger_enums = '''
enum { meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_LOGIC_LOW,
meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_LOGIC_HIGH,
meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_FALLING_EDGE,
meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_RISING_EDGE,
meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_EITHER_EDGE_ACTIVE_LOW,
meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_EITHER_EDGE_ACTIVE_HIGH,
_meshtastic_ModuleConfig_DetectionSensorConfig_TriggerType_MAX=5 };
'''
stub = r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include "SensorDelivery.h"
#include "SHA256.h"
#ifdef _WIN32
#define strcasecmp _stricmp
#endif
#define LOG_INFO(...) ((void)0)
enum class ProcessMessage { CONTINUE, STOP };
const int meshtastic_Channel_Role_DISABLED=0, RTCQualityFromNet=1;
struct Channel {int role=1,index=2;struct {char name[12]="gpio";struct {uint8_t bytes[32]={};size_t size=32;} psk;} settings;};
struct Channels {static constexpr const char* gpioChannel="gpio";Channel ch;Channel& getByName(const char*){return ch;}} channels;
struct {struct {bool enabled=true;unsigned state_broadcast_secs=900,monitor_pin=22;char name[21]="Motion";} detection_sensor;} moduleConfig;
struct meshtastic_MeshPacket {uint32_t channel=2,from=456,to=123;bool want_ack=true;struct {struct {unsigned size=0;uint8_t bytes[238]={};}payload;}decoded;};
unsigned tick=1000,utc=1794550000; unsigned millis(){return tick;} unsigned getValidTime(int){return utc;}
long random(){static long n=0;return ++n;}
struct DB {unsigned getNodeNum(){return 123;}} db,*nodeDB=&db;
int sent=0;meshtastic_MeshPacket result;
struct Service {void sendToMesh(meshtastic_MeshPacket*p){result=*p;sent++;delete p;}} serviceObject,*service=&serviceObject;
struct DetectionSensorModule {
 SensorDelivery::Queue delivery; uint8_t deliveryKey[32]={},deliveryKeySize=0,stream[8]={};
 uint32_t sequence=0,lastDelivery=0,lastSentToMesh=0;
 meshtastic_MeshPacket* allocDataPacket(){return new meshtastic_MeshPacket;}
 bool prepareDelivery();void queueEvent(bool,bool);void sendPending();ProcessMessage handleReceived(const meshtastic_MeshPacket&);
};
'''
main=r'''
int main(){
 memset(channels.ch.settings.psk.bytes,7,32);
 DetectionSensorModule m;
 strcpy(channels.ch.settings.name,"LongFast");m.queueEvent(true,true);assert(sent==0);
 strcpy(channels.ch.settings.name,"GPIO");channels.ch.role=0;m.queueEvent(true,true);assert(sent==0);
 channels.ch.role=2;channels.ch.settings.psk.size=1;m.queueEvent(true,true);assert(sent==0);
 channels.ch.settings.psk.size=32;m.queueEvent(true,true);assert(sent==1&&!result.want_ack&&result.channel==2);
 auto original=result.decoded.payload;
 for(unsigned i=0;i<original.size;i++)printf("%02x",original.bytes[i]);puts("");
 tick+=599999;m.sendPending();assert(sent==1);tick++;m.sendPending();assert(sent==2);
 assert(!memcmp(original.bytes,result.decoded.payload.bytes,original.size));
 meshtastic_MeshPacket a;auto*b=a.decoded.payload.bytes;a.decoded.payload.size=57;
 memcpy(b,"MDS2",4);b[4]=2;SensorDelivery::put32(b+5,456);SensorDelivery::put32(b+9,123);
 memcpy(b+13,original.bytes+9,8);memcpy(b+21,original.bytes+17,4);memcpy(b+25,original.bytes+original.size-16,16);
 deliveryMac(channels.ch.settings.psk.bytes,32,b,41,b+41);
 a.channel=3;m.handleReceived(a);assert(m.delivery.items[0].size);a.channel=2;
 a.from=457;m.handleReceived(a);assert(m.delivery.items[0].size);a.from=456;
 a.to=124;m.handleReceived(a);assert(m.delivery.items[0].size);a.to=123;
 for(unsigned i=0;i<57;i++){b[i]^=1;m.handleReceived(a);assert(m.delivery.items[0].size);b[i]^=1;}
 m.handleReceived(a);assert(!m.delivery.items[0].size);m.handleReceived(a);assert(!m.delivery.items[0].size);
 tick+=15000;utc=0;m.queueEvent(false,false);assert(result.decoded.payload.bytes[32]==0&&SensorDelivery::get32(result.decoded.payload.bytes+21)==0);
 auto &q=m.delivery;tick+=600000;m.sendPending();tick+=600000;m.sendPending();int stop=sent;tick+=599999;m.sendPending();assert(sent==stop);tick++;m.sendPending();assert(sent==stop&&q.dropped==1);
 q.clear();for(unsigned i=0;i<5;i++){auto&x=q.allocate(i);x.size=52;}assert(q.dropped==2);
 q.clear();auto&x=q.allocate(0xfffffff0);x.size=52;x.attempts=1;x.last=0xfffffff0;assert(!q.due(100));assert(q.due(uint32_t(0xfffffff0+600000u)));
 m.prepareDelivery();channels.ch.settings.psk.bytes[0]^=1;m.prepareDelivery();for(auto&i:q.items)assert(!i.size);
 // All six actual trigger handlers, all previous/current raw GPIO combinations.
 // Polarity normalization is the same expression as hasDetectionEvent().
 for(int mode=0;mode<6;mode++)for(int prev=0;prev<2;prev++)for(int current=0;current<2;current++){
   bool p=(mode&1)?prev:!prev,c=(mode&1)?current:!current;
   auto verdict=handlers[mode](p,c);
   auto expected=mode<2?(c?DetectionSensorVerdictDetected:DetectionSensorVerdictNoop):
     mode<4?(!p&&c?DetectionSensorVerdictDetected:DetectionSensorVerdictNoop):
     p==c?DetectionSensorVerdictNoop:c?DetectionSensorVerdictDetected:DetectionSensorVerdictSendState;
   assert(verdict==expected);
   if(verdict!=DetectionSensorVerdictNoop){
     q.clear();tick+=15000;m.queueEvent(c,verdict==DetectionSensorVerdictDetected);
     assert(result.decoded.payload.bytes[32]==c);
     assert(result.decoded.payload.bytes[33]==(verdict==DetectionSensorVerdictDetected));
   }
 }
 // Sustained high-frequency alerts remain bounded; an ACK for an evicted event
 // must not clear any newer item. No timer-driven infinite retry loop.
 q.clear();for(int i=0;i<100;i++){tick+=15000;m.queueEvent(true,true);}
 unsigned occupied=0;for(auto&i:q.items)occupied+=i.size!=0;assert(occupied==4);
 m.handleReceived(a);occupied=0;for(auto&i:q.items)occupied+=i.size!=0;assert(occupied==4);
 tick+=1800000;m.sendPending();for(auto&i:q.items)assert(!i.size);
 puts("PASS: queue bounds, retries, expiry, wraparound, key change, wrong ACK, exact ACK, channel selection");
 puts("PASS: six trigger modes, 24 raw transitions, sustained alerts, evicted ACK isolation");
}
'''
crypto=root/'.pio/libdeps/nrf52_promicro_diy_tcxo/Crypto'
with tempfile.TemporaryDirectory(prefix='gpio-v2-test-') as td:
 p=pathlib.Path(td);(p/'test.cpp').write_text(stub+trigger_enums+triggers+methods+main)
 subprocess.run(sys.argv[1:]+['-std=c++17','-I'+str(root/'src/modules'),'-I'+str(crypto),str(p/'test.cpp'),*[str(crypto/f) for f in ['SHA256.cpp','Hash.cpp','Crypto.cpp']],'-o',str(p/'test.exe')],check=True)
 out=subprocess.check_output([str(p/'test.exe')],text=True)
 packet=bytes.fromhex(out.splitlines()[0])
 assert packet[-16:]==hmac.new(bytes([7])*32,packet[:-16],hashlib.sha256).digest()[:16]
 assert packet[:5]==b'MDS2\1' and len(packet)==57 and packet[35:41]==b'Motion'
 print(out.splitlines()[-1]);print('PASS: real firmware HMAC matches independent Python SHA256')
