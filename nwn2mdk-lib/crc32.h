#pragma once

#include <cstddef>
#include <cstdint>

uint32_t crc32c(uint32_t crc, const unsigned char *buf, size_t len);
