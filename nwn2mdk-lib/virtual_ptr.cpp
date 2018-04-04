#include <unordered_map>

#include "virtual_ptr.h"

static uint32_t counter = 0;

static std::unordered_map<uint32_t, void*>& decoding_map()
{
	static std::unordered_map<uint32_t, void*> m;
	return m;
}

void* decode_ptr(uint32_t encoded_ptr)
{
	auto it = decoding_map().find(encoded_ptr);
	if (it != decoding_map().end())
		return it->second;

	return nullptr;
}

uint32_t encode_ptr(void *p)
{
	static std::unordered_map<void*, uint32_t> encoding_map;

	auto it = encoding_map.find(p);
	if (it != encoding_map.end())
		return it->second;

	auto encoded_ptr = ++counter;	

	encoding_map.emplace(p, encoded_ptr);
	decoding_map().emplace(encoded_ptr, p);

	return encoded_ptr;
}