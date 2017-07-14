#include <iostream>
#include <assert.h>

#include "crc32.h"
#include "gr2_file.h"
#include "granny2dll_handle.h"

using namespace std;

const uint32_t magic0 = 0xcab067b8;
const uint32_t magic1 = 0xfb16df8;
const uint32_t magic2 = 0x7e8c7284;
const uint32_t magic3 = 0x1e00195e;

GR2_file::GR2_file(const char* path) : in(path, std::ios::in | std::ios::binary)
{
	is_good = true;

	if (!in) {
		is_good = false;
		error_string_ = "cannot open file";
		return;
	}

	read_header();
	if (!is_good)
		return;

	check_magic();
	if (!is_good)
		return;

	read_section_headers();
	if (!is_good)
		return;

	check_crc();
	if (!is_good)
		return;

	read_sections();
	read_relocations();
	apply_relocations();
	apply_marshalling();

	file_info = (GR2_file_info*)sections_data.data();
	type_definition =
	    (GR2_property_key*)(sections_data.data() + header.info.type_offset);
}

void GR2_file::apply_marshalling()
{
	for (unsigned i = 0; i < header.info.sections_count; ++i)
		apply_marshalling(i);
}

void GR2_file::apply_marshalling(unsigned index)
{
	Section_header& section = section_headers[index];

	if (section.marshallings_count <= 0)
		return;

	in.seekg(section.marshallings_offset);
	std::vector<Marshalling> marshallings(section.marshallings_count);
	in.read((char*)marshallings.data(),
	        section.marshallings_count * sizeof(Marshalling));
	for (unsigned i = 0; i < marshallings.size(); ++i)
		apply_marshalling(index, marshallings[i]);
}

void GR2_file::apply_marshalling(unsigned index, Marshalling& m)
{
	auto type_def = (GR2_property_key*)(sections_data.data() +
	                                    section_offsets[m.target_section] +
	                                    m.target_offset);
	auto p = sections_data.data() + section_offsets[index] + m.offset;
	if (type_def->type != GR2_type_inline) {
		cout << "WARNING: unhandled case\n";
		assert(false);
	}
	else if (type_def->keys) {
		auto p = type_def->keys;
		while (p->type != GR2_type_none) {
			if (p->type != GR2_type_uint8) {
				cout << "WARNING: unhandled case\n";
				assert(false);
			}
			++p;
		}
	}
}

void GR2_file::apply_relocations()
{
	for (unsigned i = 0; i < header.info.sections_count; ++i)
		apply_relocations(i);
}

void GR2_file::apply_relocations(unsigned index)
{
	for (const auto& relocation : relocations[index]) {
		unsigned source_offset =
		    section_offsets[index] + relocation.offset;
		unsigned target_offset =
		    section_offsets[relocation.target_section] +
		    relocation.target_offset;
		unsigned char* target_address =
		    sections_data.data() + target_offset;
		memcpy(&sections_data[source_offset], &target_address, 4);
	}
}

void GR2_file::check_crc()
{
	std::vector<unsigned char> buffer(header.info.file_size -
	                                  sizeof(Header));
	in.seekg(sizeof(Header));
	in.read((char*)buffer.data(), buffer.size());
	auto crc32 = crc32c(0, buffer.data(), buffer.size());
	if (header.info.crc32 != crc32) {
		is_good = false;
		error_string_ = "CRC32 error";
	}
}

void GR2_file::check_magic()
{
	if (header.magic[0] != magic0 || header.magic[1] != magic1 ||
	    header.magic[2] != magic2 || header.magic[3] != magic3) {
		is_good = false;
		error_string_ = "wrong magic numbers";
	}
}

void GR2_file::read_header()
{
	in.read((char*)&header, sizeof(Header));
	if (!in) {
		is_good = false;
		error_string_ = "cannot read header";
	}
}

void GR2_file::read_relocations()
{
	for (auto& section : section_headers)
		read_relocations(section);
}

void GR2_file::read_relocations(Section_header& section)
{
	in.seekg(section.relocations_offset);
	relocations.emplace_back(section.relocations_count);
	if (section.relocations_count > 0)
		in.read((char*)relocations.back().data(),
		        relocations.back().size() * sizeof(Relocation));
}

void GR2_file::read_section(unsigned index)
{
	Section_header& section = section_headers[index];

	if (section.data_size == 0)
		return;

	in.seekg(section.data_offset);
	vector<unsigned char> section_data(section.data_size + 4);
	in.read((char*)section_data.data(), section.data_size);
	if (in.eof()) {
		is_good = false;
		error_string_ = "unexpected end of file";
		return;
	}
	else if (!in) {
		is_good = false;
		error_string_ = "cannot read section";
		return;
	}

	static Granny2dll_handle granny2dll("granny2.dll");

	if (!granny2dll) {
		is_good = false;
		error_string_ = "cannot load library";
		return;
	}

	if (!granny2dll.GrannyDecompressData) {
		is_good = false;
		error_string_ = "cannot get address of GrannyDecompressData";
		return;
	}

	int ret = granny2dll.GrannyDecompressData(
	    section.compression, 0, section.data_size, section_data.data(),
	    section.first16bit, section.first8bit, section.decompressed_size,
	    sections_data.data() + section_offsets[index]);
	if (!ret) {
		is_good = false;
		error_string_ = "cannot decompress section";
		return;
	}
}

void GR2_file::read_section_headers()
{
	section_headers.resize(header.info.sections_count);
	in.read((char*)section_headers.data(),
	        sizeof(Section_header) * header.info.sections_count);
	if (!in) {
		is_good = false;
		error_string_ = "cannot read section headers";
		return;
	}
}

void GR2_file::read_sections()
{
	int total_size = 0;
	for (auto& h : section_headers) {
		section_offsets.push_back(total_size);
		total_size += h.decompressed_size;
	}

	sections_data.resize(total_size);

	for (unsigned i = 0; i < header.info.sections_count; ++i)
		read_section(i);
}

GR2_file::operator bool() const
{
	return is_good;
}

std::string GR2_file::error_string() const
{
	return error_string_;
}

void GR2_file::write(const char* filename)
{
	Header header;
	header.magic[0] = magic0;
	header.magic[1] = magic1;
	header.magic[2] = magic2;
	header.magic[3] = magic3;
	header.size = 352;
	header.format = 0;
	header.reserved[0] = header.reserved[1] = 0;
	header.info.version = 6;
	header.info.sections_offset = 56;
	header.info.sections_count = 6;
	header.info.type_section = 0;
	header.info.type_offset = this->header.info.type_offset;
	header.info.root_section = 0;
	header.info.root_offset = 0;
	header.info.tag = 0x80000015;
	header.info.extra[0] = 0;
	header.info.extra[1] = 0;
	header.info.extra[2] = 0;
	header.info.extra[3] = 0;

	unsigned offset = sizeof(Header) + 6 * sizeof(Section_header);
	std::vector<Section_header> sections(6);
	for (unsigned i = 0; i < 6; ++i) {
		Section_header& section = sections[i];
		section.compression = 0;
		section.relocations_offset = offset;
		section.relocations_count = relocations[i].size();
		offset += section.relocations_count * sizeof(Relocation);
		section.marshallings_offset = offset;
		section.marshallings_count = 0;
		offset += section.marshallings_count * sizeof(Marshalling);
		section.data_offset = offset;
		section.data_size = this->section_headers[i].decompressed_size;
		section.decompressed_size = section.data_size;
		section.alignment = 4;
		section.first16bit = 0;
		section.first8bit = 0;
		offset += section.data_size;
	}

	header.info.file_size = offset;

	header.info.crc32 = crc32c(0, (unsigned char*)sections.data(),
	                           sections.size() * sizeof(Section_header));
	for (unsigned i = 0; i < header.info.sections_count; ++i) {
		header.info.crc32 = crc32c(
		    header.info.crc32, (unsigned char*)relocations[i].data(),
		    relocations[i].size() * sizeof(Relocation));
		if (section_offsets[i] < sections_data.size())
			header.info.crc32 = crc32c(
			    header.info.crc32,
			    (unsigned char*)&sections_data[section_offsets[i]],
			    sections[i].data_size);
	}

	ofstream out(filename, std::ios::binary);
	out.write((char*)&header, sizeof(Header));
	out.write((char*)sections.data(),
	          sections.size() * sizeof(Section_header));

	for (unsigned i = 0; i < header.info.sections_count; ++i) {
		out.write((char*)relocations[i].data(),
		          relocations[i].size() * sizeof(Relocation));
		if (section_offsets[i] < sections_data.size())
			out.write((char*)&sections_data[section_offsets[i]],
			          sections[i].data_size);
	}
}
