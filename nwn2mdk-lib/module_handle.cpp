#include "module_handle.h"

Module_handle::Module_handle(const char *filename)
{
	hmodule = LoadLibraryA(filename);
}

Module_handle::~Module_handle()
{
	if(hmodule)
		FreeLibrary(hmodule);
}

Module_handle::operator bool() const
{
	return hmodule != NULL;
}

FARPROC Module_handle::get_proc_address(const char *proc_name)
{
	return GetProcAddress(hmodule, proc_name);
}