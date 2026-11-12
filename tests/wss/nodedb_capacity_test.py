"""Compile the actual NodeDB admission function with a bounded host database."""
from pathlib import Path
import subprocess

def run(root, build, zig, env):
    source = (root / 'src/mesh/NodeDB.cpp').read_text(encoding='utf8')
    start = source.index('meshtastic_NodeInfoLite *NodeDB::getOrCreateMeshNode(NodeNum n)')
    end = source.index('\n/// Sometimes', start)
    harness = r'''
#include <vector>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#define LOG_INFO(...) ((void)0)
#define LOG_WARN(...) ((void)0)
#define NODEINFO_BITFIELD_IS_KEY_MANUALLY_VERIFIED_MASK 1
using NodeNum=uint32_t;
struct meshtastic_NodeInfoLite {
 uint32_t num=0,last_heard=0,bitfield=0;
 bool is_favorite=false,is_ignored=false;
 struct {struct {unsigned size=0;} public_key;} user;
};
struct {unsigned getFreeHeap(){return 65536;}} memGet;
class NodeDB {
public:
 std::vector<meshtastic_NodeInfoLite> storage;
 std::vector<meshtastic_NodeInfoLite>*meshNodes=&storage;
 int numMeshNodes;
 NodeDB(int count):storage(count),numMeshNodes(count){
  for(int i=0;i<count;++i){storage[i].num=i+1;storage[i].last_heard=i+1;}
 }
 bool isFull(){return size_t(numMeshNodes)>=storage.size();}
 meshtastic_NodeInfoLite*getMeshNode(NodeNum n){for(auto &v:storage)if(v.num==n)return &v;return nullptr;}
 meshtastic_NodeInfoLite*getOrCreateMeshNode(NodeNum n);
};
'''
    cases = r'''
int main(){
 for(int capacity:{200,1000}){
  for(int protection=0;protection<3;++protection){
   NodeDB db(capacity);
   for(auto &v:db.storage){v.is_favorite=protection==0;v.is_ignored=protection==1;v.bitfield=protection==2?1:0;}
   assert(db.getOrCreateMeshNode(1)==&db.storage[0]);
   try {assert(db.getOrCreateMeshNode(9000)==nullptr);}
   catch(const std::out_of_range&){puts("FAIL: full protected NodeDB admission exceeds capacity");return 1;}
   assert(db.numMeshNodes==capacity);
   for(int i=0;i<capacity;++i)assert(db.storage[i].num==unsigned(i+1));
  }
  NodeDB churn(capacity);
  for(unsigned id=2000;id<4000;++id){
   auto *v=churn.getOrCreateMeshNode(id);assert(v&&v->num==id);
   assert(churn.numMeshNodes==capacity&&churn.storage[0].num==1);
   v->last_heard=id;
  }
 }
 puts("NodeDB actual admission: protected capacity, existing nodes and bounded churn PASS");
}
'''
    cpp=build/'nodedb_capacity_test.cpp'
    cpp.write_text(harness+source[start:end]+cases,encoding='utf8')
    exe=build/'nodedb_capacity_test.exe'
    subprocess.run([zig,'c++','-std=c++14','-O0',str(cpp),'-o',str(exe)],env=env,check=True)
    subprocess.run([str(exe)],check=True)
