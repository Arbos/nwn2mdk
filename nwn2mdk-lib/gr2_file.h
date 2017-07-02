#pragma once

#include <fstream>
#include <vector>

#include "gr2.h"

class GR2_file {
public:
	struct Info {
		uint32_t version; // Always seems 6
		uint32_t file_size;
		uint32_t crc32;
		uint32_t sections_offset; // From 'version'
		uint32_t sections_count;
		uint32_t type_section;
		uint32_t type_offset;
		uint32_t root_section;
		uint32_t root_offset;
		uint32_t tag;
		uint32_t extra[4]; // Always seems 0
	};

	struct Header {
		uint32_t magic[4];
		uint32_t size;        // Size of the header
		uint32_t format;      // Seems always 0
		uint32_t reserved[2]; // Seems always 0
		Info info;
	};

	struct Section_header {
		uint32_t compression; // 0: no compression, 1: Oodle0, 2: Oodle1
		uint32_t data_offset; // From the start of the file
		uint32_t data_size;   // In bytes
		uint32_t decompressed_size; // In bytes
		uint32_t alignment;         // Seems always 4
		uint32_t first16bit;        // Stop0 for Oodle1
		uint32_t first8bit;         // Stop1 for Oodle1
		uint32_t relocations_offset;
		uint32_t relocations_count;
		uint32_t marshallings_offset;
		uint32_t marshallings_count;
	};

	struct Relocation {
		uint32_t offset;
		uint32_t target_section;
		uint32_t target_offset;
	};

	struct Marshalling {
		uint32_t count;
		uint32_t offset;
		uint32_t target_section;
		uint32_t target_offset;
	};

	Header header;
	std::vector<Section_header> section_headers;
	GR2_file_info* file_info;
	GR2_property_key* type_definition;

	GR2_file(const char* filename);

	operator bool() const;
	std::string error_string() const;
	void write(const char* filename);

private:
	static_assert(sizeof(Info) == 56, "");
	static_assert(sizeof(Header) == 32 + sizeof(Info), "");
	static_assert(sizeof(Section_header) == 44, "");
	static_assert(sizeof(Relocation) == 4 * 3, "");
	static_assert(sizeof(Marshalling) == 4 * 4, "");

	std::ifstream in;
	bool is_good;
	std::string error_string_;
	std::vector<unsigned char>
	    sections_data; // All sections' data in a contiguous buffer.
	std::vector<unsigned> section_offsets;
	std::vector<std::vector<Relocation>> relocations;

	void apply_marshalling();
	void apply_marshalling(unsigned index);
	void apply_marshalling(unsigned index, Marshalling& m);
	void apply_relocations();
	void apply_relocations(unsigned index);
	void check_crc();
	void check_magic();
	void read_header();
	void read_relocations();
	void read_relocations(Section_header& section);
	void read_section(unsigned index);
	void read_section_headers();
	void read_sections();
};
