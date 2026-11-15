"""Execute the production probe tick with deterministic clock/socket adapters."""
import subprocess


def run(root, build, zig, env):
    source = (root / 'src/modules/MvtWssStressNetwork.cpp').read_text(encoding='utf-8')
    tick = source.split('void mvtStressNetTick(){', 1)[1].split('void mvtStressNetCancel()', 1)[0]
    cpp = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cerrno>
using socklen_t=unsigned;
struct sockaddr {};
struct sockaddr_in {struct {uint32_t s_addr;} sin_addr; uint16_t sin_port;};
struct fd_set {};
struct timeval {int a,b;};
#define FD_ZERO(x) ((void)0)
#define FD_SET(x,y) ((void)0)
#define MSG_DONTWAIT 0
#define SOL_SOCKET 0
#define SO_ERROR 0
#define ESP_LOGW(...) ((void)0)
static int phase=0,probe=-1,starts=0,tcp=0,closes=0,reads=0,selects=0;
static uint32_t now=0,started=0,expires=0,dns=0,seen=0,lastAutomatic=0;
struct MvtWssDiagnostic {uint32_t sequence;};
uint32_t millis(){return now;}
bool mvt_wss_latest_diagnostic(MvtWssDiagnostic *d){d->sequence=now;return true;}
bool mvtStressBefore(uint32_t a,uint32_t b){return int32_t(a-b)<0;}
void mvtStressNetStart(){++starts;phase=1;probe=10;expires=now+5000;}
void closeProbe(){++closes;phase=0;probe=-1;}
void tcpStart(){++tcp;phase=2;expires=now+5000;}
int recvfrom(int,void*,unsigned,int,sockaddr*,socklen_t*){++reads;return -1;}
uint16_t ntohs(uint16_t n){return n;}
int select(int,void*,fd_set*,fd_set*,timeval*){++selects;return 0;}
int getsockopt(int,int,int,void*,socklen_t*){return 0;}
void mvtStressNetTick(){PRODUCTION_TICK
int main(){
 // Repeated errors over a day must not start a socket probe during observation.
 for(now=31000;now<86400000;now+=31001)mvtStressNetTick();
 assert(starts==0&&reads==0&&selects==0&&phase==0);
 // An explicit request still runs DNS -> TCP and releases the probe on timeout.
 now=100000;mvtStressNetStart();mvtStressNetTick();assert(reads==1&&tcp==0);
 now+=5000;mvtStressNetTick();assert(tcp==1&&phase==2);
 now+=4999;mvtStressNetTick();assert(phase==2);
 ++now;mvtStressNetTick();assert(phase==0&&closes==1);
 now+=60000;mvtStressNetTick();assert(starts==1&&phase==0);
 // Cancellation must stay cancelled despite newer error diagnostics.
 mvtStressNetStart();closeProbe();now+=60000;mvtStressNetTick();assert(starts==2&&phase==0);
 puts("PASS network probes: observation never starts traffic; explicit probe deadlines and cancellation");
}
'''.replace('PRODUCTION_TICK', tick)
    path = build / 'network_probe_test.cpp'
    path.write_text(cpp, encoding='utf-8')
    exe = build / 'network_probe_test.exe'
    subprocess.run([zig, 'c++', '-std=c++14', '-O0', str(path), '-o', str(exe)], env=env, check=True)
    subprocess.run([str(exe)], check=True)
