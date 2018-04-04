#pragma once

#include <cstdint>

void* decode_ptr(uint32_t encoded_ptr);
uint32_t encode_ptr(void *p);

// A virtual pointer encodes a pointer of any length in a 32-bit number.
// This is useful in 64-bit machines to use raw data that contains 32-bit
// pointers as is, without the need of unpacking it.
template <class T>
class Virtual_ptr {
public:
	Virtual_ptr()
	{
	}

	Virtual_ptr(T* p)
	{
		encoded_ptr = encode_ptr(p);		
	}

	operator T*()
	{
		return reinterpret_cast<T*>(decode_ptr(encoded_ptr));		
	}

	T* operator->()
	{
		return *this;
	}

	T* get()
	{
		return *this;
	}
private:
	uint32_t encoded_ptr;	
};