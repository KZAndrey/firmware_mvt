#pragma once
#include <cstddef>
#define MALLOC_CAP_INTERNAL 0x800
#define MALLOC_CAP_DMA 8
size_t heap_caps_get_free_size(unsigned);
size_t heap_caps_get_largest_free_block(unsigned);
size_t heap_caps_get_minimum_free_size(unsigned);
