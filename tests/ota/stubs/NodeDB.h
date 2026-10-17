#pragma once
#include <cstdint>
struct TestNodeDB { uint32_t getNodeNum(){return 0x11223344;} };
extern TestNodeDB *nodeDB;
