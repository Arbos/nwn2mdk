#include <iostream>

#include "log.h"

namespace Log {
	int error_count = 0;

	std::ostream& error()
	{
		++error_count;
		return std::cout << "ERROR: ";
	}
}