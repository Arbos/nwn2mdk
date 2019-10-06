#include <iostream>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#endif

#include "redirect_output_handle.h"

using namespace std;

Redirect_output_handle::Redirect_output_handle()
{
	original_cerr_rdbuf = nullptr;
	original_cout_rdbuf = nullptr;

#ifdef _WIN32
	HWND hwnd = GetConsoleWindow();
	DWORD process_id;
	GetWindowThreadProcessId(hwnd, &process_id);
	if (GetCurrentProcessId() == process_id) {
		static ofstream out("log.txt");
		original_cerr_rdbuf = cerr.rdbuf(out.rdbuf());
		original_cout_rdbuf = cout.rdbuf(out.rdbuf());
	}
#endif
}

Redirect_output_handle::~Redirect_output_handle()
{
	if (original_cerr_rdbuf)
		cerr.rdbuf(original_cerr_rdbuf);

	if (original_cout_rdbuf)
		cout.rdbuf(original_cout_rdbuf);
}
