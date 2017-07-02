#pragma once

#include "module_handle.h"

// Type definition for function contained in granny2.dll.
typedef int (__stdcall *GrannyDecompressData_t)(int Format,
	int FileIsByteReversed, int CompressedBytesSize,
	void *CompressedBytes, int Stop0, int Stop1, int Stop2,
	void *DecompressedBytes);

class Granny2dll_handle : public Module_handle {
public:
	/// @filename Path to granny2.dll.
	Granny2dll_handle(const char *filename);

	GrannyDecompressData_t GrannyDecompressData;
};