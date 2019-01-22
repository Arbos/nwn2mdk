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
#ifdef VIRTUAL_PTR
	auto it = decoding_map().find(encoded_ptr);
	if (it != decoding_map().end())
		return it->second;

	return nullptr;
#else
	return (void*)encoded_ptr;
#endif
}

uint32_t encode_ptr(const void *p)
{
#ifdef VIRTUAL_PTR
	if (!p)
		return 0;

	static std::unordered_map<const void*, uint32_t> encoding_map;

	auto it = encoding_map.find(p);
	if (it != encoding_map.end())
		return it->second;

	auto encoded_ptr = ++counter;	

	encoding_map.emplace(p, encoded_ptr);
	decoding_map().emplace(encoded_ptr, const_cast<void*>(p));

	return encoded_ptr;
#else
	return (uint32_t)p;
#endif
}
