"""Execute the real set_time_only handler against an already-synchronized RTC."""
import pathlib,subprocess,sys,tempfile
root=pathlib.Path(__file__).resolve().parents[1]
s=(root/'src/modules/AdminModule.cpp').read_text()
case=s[s.index('    case meshtastic_AdminMessage_set_time_only_tag:'):s.index('    case meshtastic_AdminMessage_enter_dfu_mode_request_tag:')]
harness=r'''
#include <cassert>
#include <cstdint>
struct timeval {long long tv_sec;long tv_usec;};
const int RTCQualityNTP=3,meshtastic_AdminMessage_set_time_only_tag=43;
#define LOG_INFO(...) ((void)0)
unsigned current=1000;bool forced=false;
void perhapsSetRTC(int,const timeval*tv,bool force){forced=force;if(force)current=tv->tv_sec;}
struct Request{unsigned set_time_only;};struct Packet{unsigned from;};
void handle(Packet mp,Request*r){switch(43){
'''+case+r'''
}}
int main(){Request r{900};handle({123},&r);assert(!forced&&current==1000);handle({0},&r);
#ifdef MOVETAIN_DETECTION_GPIO_CHANNEL
assert(forced&&current==900);
#else
assert(!forced&&current==1000);
#endif
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=pathlib.Path(tmp);(p/'test.cpp').write_text(harness)
 for flags in [[],['-DMOVETAIN_DETECTION_GPIO_CHANNEL']]:
  subprocess.run(sys.argv[1:]+['-std=c++17',*flags,str(p/'test.cpp'),'-o',str(p/'test.exe')],check=True);subprocess.run([str(p/'test.exe')],check=True)
 print('PASS: explicit local correction bypasses refresh gate only for GPIO; remote and stock behavior unchanged')
