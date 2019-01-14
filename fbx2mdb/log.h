// Very basic logging library

#pragma once

#include <iosfwd>

namespace Log {
	extern int error_count;

	std::ostream& error();
}
