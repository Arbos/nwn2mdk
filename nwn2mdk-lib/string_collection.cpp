#include <iostream>

#include "string_collection.h"

using namespace std;

char* String_collection::get(const char* s)
{
	string str = s;
	auto it = strings.insert(str).first;
	return const_cast<char*>(it->c_str());
}

uint32_t String_collection::write(const char* s, std::ostream& out)
{
	if (!s) return -1;

	string str = s;
	auto it = string_map.find(str);
	if (it != string_map.end())
		return it->second;

	string_map[str] = uint32_t(out.tellp());

	out.write(s, str.length());

	unsigned pad_length = ((str.length() + 4) & ~0x03) - str.length();
	for (unsigned i = 0; i < pad_length; ++i)
		out.put(0);

	return string_map[str];
}