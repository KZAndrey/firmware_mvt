#include <cassert>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include "platform/esp32/MvtTlsRead.h"
extern "C" int __wrap_esp_transport_read(esp_transport_handle_t,char*,int,int);
extern "C" int __wrap_esp_transport_write(esp_transport_handle_t,const char*,int,int);
static int writeCalls, writeResult=2;
static uint32_t nowMs; static int calls;static void* expected;static char* expectedBuffer;
int64_t esp_timer_get_time(){return int64_t(nowMs)*1000;}
void vTaskDelay(int){++nowMs;}
extern "C" int __real_esp_transport_read(void* t,char* b,int n,int timeout){assert(t==expected&&b==expectedBuffer&&n==2&&timeout>=0&&timeout<=10);return ++calls%2?-0x6900:2;}
extern "C" int __real_esp_transport_write(void* t,const char* b,int n,int timeout){
 assert(t==expected&&b==expectedBuffer&&n==2&&timeout>=0&&timeout<=10);
 ++writeCalls;
 if(writeResult!=2)return writeResult;
 return writeCalls==1?-0x6880:writeCalls==2?-0x6900:2;
}
int main(){int parent,other;char b[2];expectedBuffer=b;
 expected=&other;assert(__wrap_esp_transport_read(&other,b,2,10)==-0x6900&&calls==1);
 assert(mvt_tls_read_register(&parent));assert(!mvt_tls_read_register(&other));
 expected=&parent;calls=0;assert(__wrap_esp_transport_read(&parent,b,2,10)==2&&calls==2);
 mvt_tls_read_unregister(&other);calls=0;assert(__wrap_esp_transport_read(&parent,b,2,10)==2&&calls==2);
 // The pending TLS operation keeps exactly the same buffer and length.
 writeCalls=0;nowMs=0;assert(__wrap_esp_transport_write(&parent,b,2,10)==2&&writeCalls==3);
 writeResult=-0x6880;writeCalls=0;nowMs=0;
 assert(__wrap_esp_transport_write(&parent,b,2,3)==-0x6880&&writeCalls==3&&nowMs==3);
 for(int result:{0,-1,1}){writeResult=result;writeCalls=0;assert(__wrap_esp_transport_write(&parent,b,2,10)==result&&writeCalls==1);}
 writeResult=-0x6880;
 mvt_tls_read_unregister(&parent);calls=0;assert(__wrap_esp_transport_read(&parent,b,2,10)==-0x6900&&calls==1);
 writeCalls=0;assert(__wrap_esp_transport_write(&parent,b,2,10)==-0x6880&&writeCalls==1);
 assert(mvt_tls_read_register(&other));mvt_tls_read_unregister(&other);
 puts("Actual TLS read/write wrappers: parent-only routing, unchanged buffer, exclusive registration, unrelated cleanup and reuse: PASS");
}
