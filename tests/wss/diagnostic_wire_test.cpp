#include "platform/esp32/MvtWssDiagnosticJson.h"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <climits>
#include <initializer_list>
static MvtWssDiagnostic value{};
static MvtWssBootDiagnostic boot{};
static unsigned allocations=0,live=0,failAt=0;
static void *allocate(size_t size){++allocations;if(allocations==failAt)return nullptr;auto p=std::malloc(size);if(p)++live;return p;}
static void release(void *p){if(p){assert(live);--live;std::free(p);}}
bool mvt_wss_last_diagnostic(MvtWssDiagnostic *out){*out=value;return true;}
bool mvt_wss_latest_diagnostic(MvtWssDiagnostic *out){*out=value;return true;}
bool mvt_wss_boot_diagnostic(MvtWssBootDiagnostic *out){*out=boot;return true;}
static void equivalent(cJSON *tree,cJSON *wire){
 assert(tree&&wire);
 char *text=cJSON_PrintUnformatted(wire);assert(text);
 auto parsed=cJSON_Parse(text);assert(parsed&&cJSON_Compare(tree,parsed,true));
 cJSON_Delete(tree);cJSON_Delete(wire);cJSON_Delete(parsed);cJSON_free(text);
}
int main(){
 value.bootId=value.sequence=value.uptimeMs=value.freeHeap=value.largestBlock=value.minimumHeap=UINT32_MAX;
 value.ret=value.error=value.socketError=value.tlsCode=INT_MIN;
 value.tlsError=value.tlsFlags=value.opcode=value.payload=value.offset=value.duringSession=INT_MAX;
 boot={UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX};
 for(const char *stage:{"read","connect","quote\" backslash\\ newline\n\t","UTF8-\xc3\xa9"}){
  std::strncpy(value.stage,stage,sizeof(value.stage)-1);
  equivalent(mvtWssDiagnosticJson(),mvtWssDiagnosticJson(false,true));
  equivalent(mvtWssDiagnosticJson(true),mvtWssDiagnosticJson(true,true));
 }
 std::memset(value.stage,1,sizeof(value.stage)-1);value.stage[31]=0;
 equivalent(mvtWssDiagnosticJson(),mvtWssDiagnosticJson(false,true));
 equivalent(mvtWssBootJson(),mvtWssBootJson(true));
 MvtDiagnosticWire oversized;for(unsigned i=0;i<100;++i)oversized.number("longField",UINT32_MAX);
 assert(!oversized.finish());
 cJSON_Hooks hooks{allocate,release};cJSON_InitHooks(&hooks);
 for(unsigned mode=0;mode<3;++mode){
  auto make=[&](){return mode==2?mvtWssBootJson(true):mvtWssDiagnosticJson(mode==1,true);};
  allocations=0;auto p=make();assert(p);cJSON_Delete(p);assert(allocations==2&&live==0);
  for(unsigned fail=1;fail<=2;++fail){allocations=0;failAt=fail;assert(!make());assert(live==0);}failAt=0;
 }
 cJSON_InitHooks(nullptr);
 std::puts("Diagnostic wire: identical fields, escaping, integer bounds, overflow and two allocation failures PASS");
}
