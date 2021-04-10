#pragma once

#include <istream>

class Membuf : public std::streambuf {
public:
	Membuf(char* begin, char* end);

	pos_type seekoff(off_type off, std::ios_base::seekdir dir,
	                 std::ios_base::openmode which
	                 = std::ios_base::in) override;
	pos_type seekpos(std::streampos pos,
	                 std::ios_base::openmode mode) override;

private:
	char *begin;
	char *end;
};

class Memstream : public std::istream {
public:
	Memstream(const void* begin, const void* end);
	Memstream(const void* begin, size_t size);

private:
	Membuf membuf;
};
