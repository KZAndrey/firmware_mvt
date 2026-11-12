#include <cassert>
#include <cstring>
#include <cstdio>
#include "../../src/modules/MvtWssNetworkRecovery.h"
static MvtWssDiagnostic failure(unsigned sequence,int tls=0x8006,const char *stage="connect") {
 MvtWssDiagnostic d{};d.sequence=sequence;d.tlsError=tls;strcpy(d.stage,stage);return d;
}
int main(){
 MvtWssNetworkRecovery p;
 p.observe(failure(1),100);for(int i=0;i<100;++i)p.observe(failure(1),150000);
 assert(!p.due(150000)); // one diagnostic is not 100 failures
 p.observe(failure(2,0x8001),60100);p.observe(failure(3),120099);
 assert(!p.due(120099));assert(p.due(120100));
 p.recovered(120100);p.observe(failure(4),130000);assert(!p.due(9999999));
 p.healthy();p.observe(failure(5),200000);p.observe(failure(6),260000);p.observe(failure(7),320000);
 assert(!p.due(320000));assert(!p.due(720099));assert(p.due(720100)); // 10 min cooldown
 p.clear();assert(!p.due(9999999));
 MvtWssNetworkRecovery q;
 q.observe(failure(1),0);q.observe(failure(2),60000);
 q.observe(failure(3,0x8008,"read"),120000);assert(!q.due(120000));
 q.observe(failure(4),150000);q.observe(failure(5),210000);
 q.observe(failure(6,0x8010),270000);assert(!q.due(270000)); // certificate/other TLS errors
 auto d=failure(7);d.duringSession=1;q.observe(d,300000);assert(!q.due(900000));
 MvtWssNetworkRecovery wrap;const uint32_t start=UINT32_MAX-60000;
 wrap.observe(failure(1),start);wrap.observe(failure(2),start+60000u);wrap.observe(failure(3),start+120000u);
 assert(wrap.due(start+120000u));
 // A failed first rejoin must not disable recovery forever. Require fresh
 // failures, the full cooldown and a hard bound of two attempts per outage.
 MvtWssNetworkRecovery retry;
 for(unsigned n=1;n<=3;++n)retry.observe(failure(n),n*60000);
 assert(retry.due(180000));retry.recovered(180000);
 for(unsigned n=4;n<=6;++n)retry.observe(failure(n),n*60000);
 assert(!retry.due(779999));assert(retry.due(780000));
 retry.recovered(780000);
 for(unsigned n=7;n<=9;++n)retry.observe(failure(n),n*120000);
 assert(!retry.due(2000000));
 puts("WiFi recovery: unique network failures, delay, once per outage, cooldown, healthy reset, non-network exclusions, uptime wrap: PASS");
}
