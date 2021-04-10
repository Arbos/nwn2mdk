#include "memstream.h"

Membuf::Membuf(char* begin, char* end)
        : begin(begin)
        , end(end)
{
	this->setg(this->begin, this->begin, this->end);
}

Membuf::pos_type Membuf::seekoff(off_type off, std::ios_base::seekdir dir,
                                 std::ios_base::openmode which)
{
	if (dir == std::ios_base::cur)
		gbump(off);
	else if (dir == std::ios_base::end)
		setg(begin, end + off, end);
	else if (dir == std::ios_base::beg)
		setg(begin, begin + off, end);

	return gptr() - eback();
}

Membuf::pos_type Membuf::seekpos(std::streampos pos,
                                 std::ios_base::openmode mode)
{
	return seekoff(pos - pos_type(off_type(0)), std::ios_base::beg, mode);
}

Memstream::Memstream(const void* begin, const void* end)
        : std::istream(nullptr)
        , membuf((char*)begin, (char*)end)
{
	rdbuf(&membuf);
}

Memstream::Memstream(const void* begin, size_t size)
	: Memstream(begin, (const char*)begin + size)
{
}
