#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
using TickType_t=int;
constexpr int pdTRUE=1,pdFALSE=0,pdPASS=1,portMAX_DELAY=-1;
#define pdMS_TO_TICKS(n) (n)
struct Queue {size_t limit,itemSize;std::mutex mutex;std::deque<std::vector<char>> values;Queue(size_t l,size_t s):limit(l),itemSize(s){}};
using QueueHandle_t=Queue*;
inline QueueHandle_t xQueueCreate(int n,size_t s){return new Queue(n,s);}
inline int xQueueSend(Queue *q,const void *p,int){std::lock_guard<std::mutex> lock(q->mutex);if(q->values.size()==q->limit)return 0;q->values.emplace_back((const char*)p,(const char*)p+q->itemSize);return 1;}
inline int xQueueReceive(Queue *q,void *p,int){std::lock_guard<std::mutex> lock(q->mutex);if(q->values.empty())return 0;memcpy(p,q->values.front().data(),q->itemSize);q->values.pop_front();return 1;}
inline void vQueueDelete(Queue *q){delete q;}
using SemaphoreHandle_t=std::timed_mutex*;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return new std::timed_mutex;}
inline int xSemaphoreTake(SemaphoreHandle_t m,int wait){if(wait==portMAX_DELAY){m->lock();return 1;}return m->try_lock();}
inline void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
inline void vSemaphoreDelete(SemaphoreHandle_t m){delete m;}
using TaskHandle_t=void*;
inline int xTaskCreate(void(*fn)(void*),const char*,int,void *arg,int,TaskHandle_t *handle){*handle=(void*)1;std::thread(fn,arg).detach();return pdPASS;}
inline void vTaskDelay(int ms){std::this_thread::sleep_for(std::chrono::milliseconds(ms));}
inline void vTaskDelete(void*){}
using EventBits_t=unsigned;
struct Event {std::mutex mutex;std::condition_variable cv;unsigned bits=0;};
using EventGroupHandle_t=Event*;
inline Event *xEventGroupCreate(){return new Event;}
inline void xEventGroupSetBits(Event *e,unsigned bits){std::lock_guard<std::mutex> lock(e->mutex);e->bits|=bits;e->cv.notify_all();}
inline unsigned xEventGroupWaitBits(Event *e,unsigned bits,bool,bool,int){std::unique_lock<std::mutex> lock(e->mutex);e->cv.wait(lock,[&]{return (e->bits&bits)==bits;});return e->bits;}
inline void vEventGroupDelete(Event *e){delete e;}
