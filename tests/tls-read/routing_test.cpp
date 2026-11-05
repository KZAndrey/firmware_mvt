#include <cassert>
#include <cstdint>
#include <cstdio>
#include "platform/esp32/MvtTlsRead.h"
extern "C" int __wrap_esp_transport_read(esp_transport_handle_t,char*,int,int);
static uint32_t nowMs; static int calls;static void* expected;static char* expectedBuffer;
int64_t esp_timer_get_time(){return int64_t(nowMs)*1000;}
void vTaskDelay(int){++nowMs;}
extern "C" int __real_esp_transport_read(void* t,char* b,int n,int timeout){assert(t==expected&&b==expectedBuffer&&n==2&&timeout>=0&&timeout<=10);return ++calls%2?-0x6900:2;}
int main(){int parent,other;char b[2];expectedBuffer=b;
 expected=&other;assert(__wrap_esp_transport_read(&other,b,2,10)==-0x6900&&calls==1);
 assert(mvt_tls_read_register(&parent));assert(!mvt_tls_read_register(&other));
 expected=&parent;calls=0;assert(__wrap_esp_transport_read(&parent,b,2,10)==2&&calls==2);
 mvt_tls_read_unregister(&other);calls=0;assert(__wrap_esp_transport_read(&parent,b,2,10)==2&&calls==2);
 mvt_tls_read_unregister(&parent);calls=0;assert(__wrap_esp_transport_read(&parent,b,2,10)==-0x6900&&calls==1);
 assert(mvt_tls_read_register(&other));mvt_tls_read_unregister(&other);
 puts("Actual TLS read wrapper: parent-only routing, unchanged buffer, exclusive registration, unrelated cleanup and reuse: PASS");
}
