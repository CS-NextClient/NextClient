#pragma once

#include <cstddef>

void SpoofMemoryStruct(uint32_t base, uint32_t replace, size_t struct_size, uint32_t start, uint32_t end);
