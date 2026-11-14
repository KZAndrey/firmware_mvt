"""Exercise the actual serial logging method without touching response buffers."""
import pathlib,subprocess,sys,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
s=(root/'src/mesh/StreamAPI.cpp').read_text()
method=s[s.index('void StreamAPI::emitLogRecord('):s.index('/// Hookable to find out')]
harness=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>
const int MAX_STREAM_BUF_SIZE=1024,HEADER_LEN=4,START1=0x94,START2=0xc3;
using meshtastic_LogRecord_Level=int;
struct meshtastic_FromRadio {int which_payload_variant;struct {int level;uint32_t time;char source[32];char message[64];}log_record;};
const int meshtastic_FromRadio_log_record_tag=7,meshtastic_FromRadio_size=sizeof(meshtastic_FromRadio),meshtastic_FromRadio_msg=0;
enum class RTCQuality {RTCQualityDevice};
uint32_t getValidTime(RTCQuality,bool){return 1234;}
bool encodeFail=false;
size_t pb_encode_to_bytes(uint8_t*b,size_t,const int*,meshtastic_FromRadio*p){if(encodeFail)return 0;memcpy(b,p,sizeof(*p));return sizeof(*p);}
struct Stream {std::vector<uint8_t> sent;void write(uint8_t*b,size_t n){sent.assign(b,b+n);}void flush(){}};
struct StreamAPI {meshtastic_FromRadio fromRadioScratch;uint8_t txBuf[MAX_STREAM_BUF_SIZE];Stream *stream;
void emitLogRecord(int,const char*,const char*,va_list);
void log(const char*fmt,...){va_list args;va_start(args,fmt);emitLogRecord(2,"source",fmt,args);va_end(args);}};
'''
main=r'''
int main(){Stream output;StreamAPI api;api.stream=&output;memset(&api.fromRadioScratch,0x5a,sizeof(api.fromRadioScratch));memset(api.txBuf,0xa5,sizeof(api.txBuf));
for(const std::string text:{std::string("hello\n"),std::string(""),std::string(63,'x'),std::string(10000,'y')}){
 api.log("%s",text.c_str());assert(output.sent[0]==0x94&&output.sent[1]==0xc3);assert(output.sent.size()==sizeof(meshtastic_FromRadio)+4);
 meshtastic_FromRadio record;memcpy(&record,output.sent.data()+4,sizeof(record));assert(record.which_payload_variant==7);assert(record.log_record.time==1234);assert(strlen(record.log_record.message)<=63);
 if(text=="hello\n")assert(!strcmp(record.log_record.message,"hello"));
 for(auto b:api.txBuf)assert(b==0xa5);for(size_t i=0;i<sizeof(api.fromRadioScratch);i++)assert(((uint8_t*)&api.fromRadioScratch)[i]==0x5a);
}
output.sent.clear();encodeFail=true;api.log("failure");assert(output.sent.empty());puts("PASS: response isolation, long/empty/newline logs, encoding failure");}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'test.cpp').write_text(harness+method+main)
 subprocess.run(sys.argv[1:]+['-std=c++17',str(p/'test.cpp'),'-o',str(p/'test.exe')],check=True)
 subprocess.run([str(p/'test.exe')],check=True)
