#pragma once

#include <iosfwd>

// When an instance of this class is created, if the current process
// is run directly (double-click, drag&drop, ...), std::cout and
// std::cerr will be redirected to log.txt. When the instance is
// destroyed, the redirection (if any) is undone.
class Redirect_output_handle {
public:
	Redirect_output_handle();
	~Redirect_output_handle();
private:
	std::streambuf* original_cerr_rdbuf;
	std::streambuf* original_cout_rdbuf;
};