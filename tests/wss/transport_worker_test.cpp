#include <cassert>
#include <cstdio>
#include <functional>
#include "platform/esp32/MvtWssTransport.h"
std::atomic<FakeClient*> sdk{nullptr};
std::atomic<bool> writeEntered{false},allowWrite{false},stopEntered{false},allowStop{false};
std::atomic<int> writeResult{0},created{0},destroyed{0},callbacks{0},writeAttempts{0};
std::mutex sentMutex;std::vector<std::string> sent;
FakeClient *esp_websocket_client_init(const esp_websocket_client_config_t *cfg){assert(cfg->pingpong_timeout_sec==75);++created;return new FakeClient;}
int esp_websocket_register_events(FakeClient *c,int,void(*fn)(void*,void*,int32_t,void*),void *arg){c->event=fn;c->arg=arg;return 0;}
int esp_websocket_client_start(FakeClient *c){sdk=c;return 0;}
int esp_websocket_client_stop(FakeClient *c){stopEntered=true;while(!allowStop)vTaskDelay(1);c->event(c->arg,nullptr,WEBSOCKET_EVENT_CONNECTED,nullptr);return 0;}
int esp_websocket_client_destroy(FakeClient *c){sdk=nullptr;delete c;++destroyed;return 0;}
bool esp_websocket_client_is_connected(FakeClient*){return writeResult<0;}
int esp_websocket_client_send_text(FakeClient*,const char *data,int n,int timeout){assert(timeout==5000);++writeAttempts;writeEntered=true;while(!allowWrite)vTaskDelay(1);if(writeResult==1){std::lock_guard<std::mutex> lock(sentMutex);sent.emplace_back(data,n);return n;}return writeResult;}
void until(std::function<bool()> f){auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!f()){assert(std::chrono::steady_clock::now()<deadline);vTaskDelay(1);}}
void connect(MvtWssTransport &t,unsigned epoch){assert(t.start("wss://test/ws","test-ca",epoch));until([]{return sdk.load()!=nullptr;});auto *c=sdk.load();c->event(c->arg,nullptr,WEBSOCKET_EVENT_CONNECTED,nullptr);until([&]{return t.connected();});}
void stop(MvtWssTransport &t){t.stop();until([&]{return !t.active();});}
int main(){
 MvtWssTransport t([](void*,void*,int32_t,void*){++callbacks;},nullptr);
 connect(t,7);assert(t.send("first",false,true));until([]{return writeEntered.load();});
 for(int i=0;i<6;i++)assert(t.send("queued",false,true));assert(t.send("heartbeat",true,false));
 auto start=std::chrono::steady_clock::now();assert(!t.send("full",false,true));t.stop();
 assert(std::chrono::steady_clock::now()-start<std::chrono::milliseconds(100));assert(!t.start("wss://other/ws","ca",8));
 assert(!t.send("late",true,true));allowWrite=true;until([]{return stopEntered.load();});
 assert(t.active());int count=callbacks;allowStop=true;until([&]{return !t.active();});assert(callbacks==count); // late CONNECTED suppressed
 std::string failed;uint32_t epoch;int replayed=0;while(t.takeFailed(failed,epoch)){assert(epoch==7);++replayed;}assert(replayed==7);
 // Byte bound also covers the in-flight message; stop must free every slot.
 writeEntered=false;allowWrite=false;connect(t,8);assert(t.send(std::string(8192,'a'),false,true));until([]{return writeEntered.load();});
 assert(t.send(std::string(8192,'b'),false,true));assert(!t.send("over budget",false,true));
 t.stop();allowWrite=true;until([&]{return !t.active();});while(t.takeFailed(failed,epoch))assert(epoch==8);
 // Priority keepalives run before queued snapshots after the in-flight send.
 writeEntered=false;allowWrite=false;writeResult=1;connect(t,9);assert(t.send("data1",false,true));until([]{return writeEntered.load();});
 assert(t.send("data2",false,true));assert(t.send("keepalive",true,false));allowWrite=true;
 until([]{std::lock_guard<std::mutex> lock(sentMutex);return sent.size()==3;});
 {std::lock_guard<std::mutex> lock(sentMutex);assert((sent==std::vector<std::string>{"data1","keepalive","data2"}));}
 stop(t);
 // Safe failures before writing retain the in-flight item; no duplicate,
 // reconnect, or callback while the reader owns the SDK lock.
 writeResult=-1;const int attempts=writeAttempts, cb=callbacks, clients=created;
 size_t deliveredBefore;{std::lock_guard<std::mutex> lock(sentMutex);deliveredBefore=sent.size();}
 connect(t,10);assert(t.send("held-by-reader",false,true));
 until([&]{return writeAttempts>=attempts+3;});
 assert(callbacks==cb+1&&created==clients+1&&t.connected());
 writeResult=1;until([&]{std::lock_guard<std::mutex> lock(sentMutex);return sent.size()==deliveredBefore+1;});
 {std::lock_guard<std::mutex> lock(sentMutex);assert(sent.back()=="held-by-reader");}
 stop(t);assert(!t.takeFailed(failed,epoch));
 for(unsigned revision=10;revision<60;++revision){
  connect(t,revision);
  auto before=sent.size();assert(t.send("cycle",false,true));
  until([&]{std::lock_guard<std::mutex> lock(sentMutex);return sent.size()==before+1;});
  stop(t);assert(!t.takeFailed(failed,epoch));
 }
 t.shutdown();assert(created==destroyed);
 puts("PASS actual worker: blocked TLS/teardown never block caller, queue byte/count limits, stale callbacks, replay epoch, priority and reconnect");
}
