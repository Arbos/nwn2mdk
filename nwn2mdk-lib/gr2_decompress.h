#pragma once

#include <cstdint>

void gr2_decompress(uint32_t compressed_size, uint8_t* compressed_buffer,
	uint32_t step1, uint32_t step2,
	uint32_t decompressed_size, uint8_t* decompressed_buffer);
