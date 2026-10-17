#pragma once
#include <cstdint>
uint32_t millis();
struct TestEsp { unsigned getFreeHeap(){return 100000;} unsigned getMaxAllocHeap(){return 90000;} };
extern TestEsp ESP;
