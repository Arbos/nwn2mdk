#include <cstdint>
#include <map>
#include <set>

class String_collection {
public:
	char *get(const char* s);	
	uint32_t write(const char* s, std::ostream& out);
private:
	std::set<std::string> strings;
	std::map<std::string, uint32_t> string_map;
};
