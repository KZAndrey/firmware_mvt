#include "../../src/mesh/BoundedHeapPool.h"
#include <cassert>
#include <thread>
#include <vector>
#include <cstdio>
struct Payload { char bytes[500]; };
static bool reject;
static unsigned allocations;
static void *allocate(size_t n){++allocations;return reject?nullptr:std::malloc(n);}
int main(){
    BoundedHeapPool<Payload,16,allocate> pool;
    assert(!allocations);reject=true;assert(!pool.allocate());reject=false;
    Payload *items[16];
    for(auto &p:items){p=pool.allocate();assert(p);}
    const unsigned count=allocations;assert(!pool.allocate()&&allocations==count);
    for(auto p:items)pool.release(p);
    pool.release(nullptr);auto p=pool.allocate();assert(p);pool.release(p);
    BoundedHeapPool<Payload,16> concurrent;
    std::atomic<unsigned> active{0};
    std::vector<std::thread> threads;
    for(unsigned t=0;t<24;++t)threads.emplace_back([&]{
        for(unsigned i=0;i<1000;++i)if(auto *p=concurrent.allocate()){
            assert(active.fetch_add(1)<16);p->bytes[0]='x';
            active.fetch_sub(1);concurrent.release(p);
        }
    });
    for(auto &t:threads)t.join();assert(!active.load());
    for(auto &p:items){p=concurrent.allocate();assert(p);}
    assert(!concurrent.allocate());for(auto p:items)concurrent.release(p);
    puts("PASS on-demand pool: zero idle payload RAM, 16-item cap, OOM rollback, reuse and concurrency");
}
