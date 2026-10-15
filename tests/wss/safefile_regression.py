"""Compile production SafeFile with an in-memory filesystem and inject resets.

Usage: python tests/wss/safefile_regression.py PATH_TO_ZIG
The adapter models ESP32 LittleFS atomic rename, not physical flash faults.
"""
import os
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
os.chdir(root)
build = root / '.tools/wss-tests'
build.mkdir(parents=True, exist_ok=True)
zig = str(Path(sys.argv[1]).resolve())
env = dict(os.environ, ZIG_LOCAL_CACHE_DIR=str(build / 'cache'), ZIG_GLOBAL_CACHE_DIR=str(build / 'global-cache'))

preamble = r'''
#include <string>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
using String = std::string;
struct Print { virtual size_t write(uint8_t)=0; virtual size_t write(const uint8_t*,size_t)=0; };
struct Lock { void lock(){} void unlock(){} } lock;
Lock *spiLock=&lock;
namespace concurrency { struct LockGuard { LockGuard(Lock*){} }; }
#define LOG_DEBUG(...) ((void)0)
#define LOG_ERROR(...) ((void)0)
#define ARCH_ESP32 1
enum { FILE_O_READ, FILE_O_WRITE };
struct Reset {};
struct File {
 std::string name; size_t pos=0; bool valid=false, corrupt=false;
 explicit operator bool() const { return valid; }
 size_t write(uint8_t ch) { return write(&ch,1); }
 size_t write(const uint8_t*,size_t); int read(); void close();
};
struct Filesystem {
 std::map<std::string,std::string> files;
 int steps=0, cut=-1; bool failRename=false, corruptRead=false;
 void checkpoint(){ if(steps++==cut) throw Reset(); }
 bool exists(const char *p){ checkpoint();bool r=files.count(p);checkpoint();return r; }
 bool remove(const char *p){checkpoint();bool r=files.erase(p);checkpoint();return r;}
 File open(const char *p,int mode){
  checkpoint();File f;f.name=p;f.valid=mode==FILE_O_WRITE||files.count(p);
  if(mode==FILE_O_WRITE)files[p].clear();
  f.corrupt=mode==FILE_O_READ&&corruptRead;checkpoint();return f;
 }
 bool rename(const char *a,const char *b){
  checkpoint(); if(failRename||!files.count(a))return false;
  auto content=files.at(a);files[b]=content;files.erase(a);checkpoint();return true;
 }
} fs;
#define FSCom fs
size_t File::write(const uint8_t *p,size_t n){
 fs.checkpoint();if(!valid)return 0;fs.files[name].append((const char*)p,n);fs.checkpoint();return n;
}
int File::read(){fs.checkpoint();if(!valid||pos>=fs.files[name].size())return -1;
 int c=(uint8_t)fs.files[name][pos++];if(corrupt&&pos==1)c^=1;fs.checkpoint();return c;}
void File::close(){fs.checkpoint();valid=false;fs.checkpoint();}
bool renameFile(const char *a,const char *b){return fs.rename(a,b);}
'''
main = r'''
const std::string path="/prefs/device.proto", oldOwner="SLV WSS GTW|SLVA", newOwner="Movetain Gateway|MVT";
void check(bool v,const char *m){if(!v){fprintf(stderr,"FAIL: %s\n",m);exit(1);}}
void init(){fs=Filesystem{};fs.files[path]=oldOwner;}
bool save(const std::string &value){
 SafeFile f(path.c_str(),true);
 f.write((uint8_t)value[0]);
 f.write((const uint8_t*)value.data()+1,value.size()-1);
 return f.close();
}
int main(){
 init();check(save(newOwner),"successful write");check(fs.files.at(path)==newOwner,"new owner persisted");
 const int boundaries=fs.steps;int gaps=0;
 for(int cut=0;cut<boundaries;++cut){
  init();fs.cut=cut;try{save(newOwner);}catch(Reset&){}
  auto found=fs.files.find(path);
  if(found==fs.files.end()){++gaps;continue;}
  check(found->second==oldOwner||found->second==newOwner,"reset exposed partial owner");
 }
#ifdef EXPECT_LEGACY
 check(gaps>0,"baseline must reproduce missing owner file");
 printf("Legacy SafeFile reproduced missing device.proto at %d reset boundaries\n",gaps);
#else
 check(gaps==0,"reset lost owner file");
 init();fs.failRename=true;check(!save(newOwner),"rename failure reported");check(fs.files.at(path)==oldOwner,"rename failure retains owner");
 init();fs.corruptRead=true;check(!save(newOwner),"corrupt readback rejected");check(fs.files.at(path)==oldOwner,"corrupt temp retains owner");
 init();for(int i=0;i<1000;++i){auto v=std::string("Owner ")+std::to_string(i);check(save(v),"repeat save");check(fs.files.at(path)==v,"repeat readback");}
 printf("Fixed production SafeFile: %d reset boundaries, failed rename, corrupt temp and 1000 owner saves passed\n",boundaries);
#endif
}
'''

def stripped(source):
    return '\n'.join(line for line in source.splitlines() if not line.startswith(('#include', '#pragma once')))

header = stripped((root / 'src/SafeFile.h').read_text(encoding='utf-8-sig'))
legacy = subprocess.check_output(['git', 'show', '46abd25:src/SafeFile.cpp'], text=True)
current = (root / 'src/SafeFile.cpp').read_text(encoding='utf-8-sig')
for name, source, flags in [('legacy', legacy, ['-DEXPECT_LEGACY']), ('fixed', current, [])]:
    cpp = build / f'safefile_{name}.cpp'
    exe = build / f'safefile_{name}.exe'
    cpp.write_text(preamble + header + stripped(source) + main, encoding='utf-8')
    with (build / f'safefile-{name}-compile.log').open('w') as log:
        subprocess.run([zig, 'c++', '-target', 'x86_64-windows-gnu', '-std=c++17', '-O0', *flags,
                        str(cpp), '-o', str(exe)], env=env, stdout=log, stderr=log, check=True)
    subprocess.run([str(exe)], check=True)
