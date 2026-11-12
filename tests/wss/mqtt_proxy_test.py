"""Compile the actual two MQTT publish overloads with a bounded host adapter."""
import subprocess

def run(root, build, zig, env):
    source=(root/'src/mqtt/MQTT.cpp').read_text()
    functions=[]
    for signature in ['bool MQTT::publish(const char *topic, const char *payload, bool retained)',
                      'bool MQTT::publish(const char *topic, const uint8_t *payload, size_t length, bool retained)']:
        start=source.index(signature); end=source.index('{',start); depth=1; end+=1
        while depth:
            if source[end]=='{': depth+=1
            elif source[end]=='}': depth-=1
            end+=1
        functions.append(source[start:end])
    fixture=r'''
#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstdio>
#define HAS_NETWORKING 0
enum {meshtastic_MqttClientProxyMessage_text_tag=1,meshtastic_MqttClientProxyMessage_data_tag=2};
struct meshtastic_MqttClientProxyMessage {
 int which_payload_variant; char topic[100]; bool retained;
 union {char text[200];struct {size_t size;uint8_t bytes[200];} data;} payload_variant;
};
struct Pool {bool reject=false;meshtastic_MqttClientProxyMessage value{};
 meshtastic_MqttClientProxyMessage *allocZeroed(){memset(&value,0,sizeof(value));return reject?nullptr:&value;}
} mqttClientProxyMessagePool;
struct Config {struct {bool proxy_to_client_enabled=true;} mqtt;} moduleConfig;
struct Service {unsigned sent=0;void sendMqttMessageToClientProxy(meshtastic_MqttClientProxyMessage *p){assert(p);++sent;}} serviceObject;
Service *service=&serviceObject;
size_t strlcpy(char *d,const char *s,size_t n){size_t len=strlen(s);if(n){size_t copy=len<n-1?len:n-1;memcpy(d,s,copy);d[copy]=0;}return len;}
class MQTT {public:bool publish(const char *,const char *,bool);bool publish(const char *,const uint8_t *,size_t,bool);};
'''
    main=r'''
int main(){
 MQTT m;uint8_t data[250]={1,2,3};
 mqttClientProxyMessagePool.reject=true;
 assert(!m.publish("topic","hello",false));
 assert(!m.publish("topic",data,sizeof(data),true));assert(!serviceObject.sent);
 mqttClientProxyMessagePool.reject=false;
 assert(m.publish("topic","hello",true));
 auto &v=mqttClientProxyMessagePool.value;
 assert(v.which_payload_variant==1&&v.retained&&!strcmp(v.payload_variant.text,"hello"));
 assert(m.publish("topic",data,sizeof(data),false));
 assert(v.which_payload_variant==2&&!v.retained&&v.payload_variant.data.size==200);
 assert(!memcmp(v.payload_variant.data.bytes,data,200)&&serviceObject.sent==2);
 moduleConfig.mqtt.proxy_to_client_enabled=false;
 assert(!m.publish("topic","hello",false));assert(serviceObject.sent==2);
 puts("PASS actual MQTT proxy publish: OOM/full-pool rejection, text, binary bounds, retained and disabled mode");
}
'''
    file=build/'mqtt_proxy_publish.cpp';file.write_text(fixture+'\n'.join(functions)+main)
    exe=build/'mqtt_proxy_publish.exe'
    subprocess.run([zig,'c++','-std=c++14','-O0',str(file),'-o',str(exe)],env=env,check=True)
    subprocess.run([str(exe)],check=True)
