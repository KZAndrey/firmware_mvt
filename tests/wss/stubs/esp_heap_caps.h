#pragma once
#include <cstddef>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_DMA 4
extern size_t testDmaFree, testDmaLargest;
inline size_t heap_caps_get_free_size(int){return testDmaFree;}
inline size_t heap_caps_get_largest_free_block(int){return testDmaLargest;}
struct multi_heap_info_t {size_t total_free_bytes,largest_free_block,minimum_free_bytes,free_blocks,allocated_blocks;};
inline void heap_caps_get_info(multi_heap_info_t *out,int){*out={50000,20000,10000,10,20};}
extern bool testHeapIntegrity;
inline bool heap_caps_check_integrity_all(bool){return testHeapIntegrity;}
