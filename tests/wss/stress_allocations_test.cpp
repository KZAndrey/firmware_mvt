#include <cstdint>
#include <cstring>
#include <cassert>
#include <thread>
#include <vector>
#include <cstdio>
static bool mvt_alloc_hook_registered=true;
#include "../../src/platform/esp32/mvt_stress_allocations.inc"
int main(){
 uint32_t values[8];char task[16];mvt_stress_alloc_snapshot(values,task);
 assert(values[0]==0&&values[7]==1&&task[0]==0);
 std::vector<std::thread> threads;
 for(unsigned i=0;i<4;++i)threads.emplace_back([i]{
  const char *name[]={"wifi","tiT","mvt_wss","mesh"};
  for(unsigned n=0;n<10000;++n)mvt_stress_record_alloc((i+1)*100,7,name[i],i==2,n);
 });
 for(unsigned n=0;n<10000;++n){
  mvt_stress_alloc_snapshot(values,task);
  assert(values[7]==1);
  if(task[0]){unsigned expected=!strcmp(task,"wifi")?100:!strcmp(task,"tiT")?200:!strcmp(task,"mvt_wss")?300:400;assert(values[4]==expected&&values[5]==7);}
 }
 for(auto &t:threads)t.join();
 mvt_stress_alloc_snapshot(values,task);
 assert(values[0]==40000&&values[1]==10000&&values[2]==10000&&values[3]==10000);
 mvt_stress_record_alloc(55,8,"a-task-name-longer-than-the-buffer",false,123);
 mvt_stress_alloc_snapshot(values,task);assert(task[15]==0&&values[4]==55&&values[6]==123);
 mvt_stress_record_alloc(66,9,nullptr,false,124);mvt_stress_alloc_snapshot(values,task);assert(task[0]==0&&values[0]==40002);
 puts("PASS global allocation diagnostics: 40000 concurrent failures, task attribution, coherent last record, bounded name and boot-lifetime counts");
}
