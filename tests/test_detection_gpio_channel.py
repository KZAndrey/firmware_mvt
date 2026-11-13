"""Compile the actual send methods against a small host radio stub.
Run: python tests/test_detection_gpio_channel.py <host-c++-compiler> [compiler args]
"""
import pathlib, subprocess, sys, tempfile
root=pathlib.Path(__file__).resolve().parents[1]
source=(root/'src/modules/DetectionSensorModule.cpp').read_text()
methods=source[source.index('void DetectionSensorModule::sendDetectionMessage()'):source.index('bool DetectionSensorModule::hasDetectionEvent()')]
stub=r'''
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdint>
#ifdef _WIN32
#define strcasecmp _stricmp
#endif
#define MOVETAIN_DETECTION_GPIO_CHANNEL 1
#define LOG_INFO(...) ((void)0)
#define LOG_DEBUG(...) ((void)0)
#define LOG_ERROR(...) ((void)0)
const int meshtastic_Channel_Role_DISABLED=0,meshtastic_Constants_DATA_PAYLOAD_LEN=237;
struct Channel { int role=1,index=0;struct {char name[12]="LongFast";} settings; };
struct Channels {static constexpr const char* gpioChannel="gpio";Channel ch;Channel& getByName(const char*){return ch;}} channels;
struct {struct {char name[21]="Motion";bool send_bell=false;} detection_sensor;} moduleConfig;
struct meshtastic_MeshPacket {int channel=0,id=1,to=0;bool want_ack=true;struct {struct {unsigned size=0;char bytes[238]={};}payload;}decoded;};
int allocated=0,sent=0;meshtastic_MeshPacket result;
struct Service {void sendToMesh(meshtastic_MeshPacket*p){result=*p;sent++;delete p;}} serviceObject,*service=&serviceObject;
unsigned millis(){return 1234;}
struct DetectionSensorModule {unsigned lastSentToMesh=0;meshtastic_MeshPacket*allocDataPacket(){allocated++;return new meshtastic_MeshPacket;}void sendDetectionMessage();void sendCurrentStateMessage(bool);};
'''
main=r'''
int main(){
 DetectionSensorModule m;
 m.sendDetectionMessage();m.sendCurrentStateMessage(false);assert(sent==0&&allocated==0);
 strcpy(channels.ch.settings.name,"gpio");channels.ch.role=0;
 m.sendDetectionMessage();m.sendCurrentStateMessage(true);assert(sent==0&&allocated==0);
 channels.ch.role=2;channels.ch.index=3;
 m.sendDetectionMessage();assert(sent==1&&result.channel==3&&!result.want_ack);assert(strcmp(result.decoded.payload.bytes,"Motion detected")==0);
 m.sendCurrentStateMessage(false);assert(sent==2&&result.channel==3);assert(strcmp(result.decoded.payload.bytes,"Motion state: 0")==0);
 m.sendCurrentStateMessage(true);assert(sent==3);assert(strcmp(result.decoded.payload.bytes,"Motion state: 1")==0);
 strcpy(channels.ch.settings.name,"GPIO");channels.ch.index=1;moduleConfig.detection_sensor.send_bell=true;
 m.sendDetectionMessage();assert(sent==4&&result.channel==1);assert(result.decoded.payload.bytes[result.decoded.payload.size-1]==7);
}
'''
with tempfile.TemporaryDirectory(prefix='detection-test-') as td:
 p=pathlib.Path(td);(p/'test.cpp').write_text(stub+methods+main)
 subprocess.run(sys.argv[1:]+['-std=c++17',str(p/'test.cpp'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
print('Firmware channel tests passed: missing, disabled, secondary, case, event, inactive, heartbeat, bell.')
