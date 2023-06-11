#include "granny2dll_handle.h"

Granny2dll_handle::Granny2dll_handle(const char* filename)
    : Module_handle(filename)
{
	GrannyDecompressData = (GrannyDecompressData_t)get_proc_address(
	    "_GrannyDecompressData@32");
	GrannyRecompressFile = (GrannyRecompressFile_t)get_proc_address(
		"_GrannyRecompressFile@16");
}
