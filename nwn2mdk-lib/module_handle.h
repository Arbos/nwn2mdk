#pragma once

#include <Windows.h>

/// A wrapper class for HMODULE.
class Module_handle {
public:
	/// @filename The name of the module.
	Module_handle(const char *filename);
	~Module_handle();

	/// Checks weather the module was successfully loaded.
	operator bool() const;

	FARPROC get_proc_address(const char *proc_name);
private:
	HMODULE hmodule;
};