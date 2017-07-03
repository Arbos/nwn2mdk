// Copyright 2017 Jose M. Arbos
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <string.h>

#include "mdb_file.h"

template <typename T>
static void read(std::istream& in, T& x)
{
	in.read((char*)&x, sizeof(T));
}

template <typename T>
static void read(std::istream& in, std::vector<T>& v)
{
	in.read((char*)v.data(), sizeof(T) * v.size());
}

MDB_file::MDB_file()
{
	is_good_ = true;

	memcpy(header.signature, "NWN2", 4);
	header.major_version = 1;
	header.minor_version = 12;
	header.packet_count = 0;
}

MDB_file::MDB_file(const char* filename)
{
	is_good_ = false;

	std::ifstream in(filename, std::ios::in | std::ios::binary);
	if (!in) {
		error_str_ = "can't open file";
		return;
	}

	read(in, header);

	if (strncmp(header.signature, "NWN2", 4) != 0) {
		error_str_ = "invalid file type";
		return;
	}

	packet_keys.resize(header.packet_count);
	read(in, packet_keys);

	read_packets(in);

	is_good_ = true;
}

const char* MDB_file::error_str() const
{
	return error_str_.c_str();
}

uint16_t MDB_file::major_version() const
{
	return header.major_version;
}

uint16_t MDB_file::minor_version() const
{
	return header.minor_version;
}

MDB_file::Packet* MDB_file::packet(uint32_t packet_index) const
{
	if (packet_index >= packet_keys.size())
		return nullptr;

	return packets[packet_index].get();
}

uint32_t MDB_file::packet_count() const
{
	return header.packet_count;
}

void MDB_file::read_packets(std::ifstream& in)
{
	for (auto& packet_key : packet_keys)
		read_packet(packet_key, in);
}

void MDB_file::read_packet(Packet_key& packet_key, std::ifstream& in)
{
	in.seekg(packet_key.offset);

	if (strncmp(packet_key.type, "COL2", 4) == 0)
		packets.emplace_back(new Collision_mesh(in));
	else if (strncmp(packet_key.type, "COL3", 4) == 0)
		packets.emplace_back(new Collision_mesh(in));
	else if (strncmp(packet_key.type, "HOOK", 4) == 0)
		packets.emplace_back(new Hook(in));
	else if (strncmp(packet_key.type, "RIGD", 4) == 0)
		packets.emplace_back(new Rigid_mesh(in));
	else if (strncmp(packet_key.type, "SKIN", 4) == 0)
		packets.emplace_back(new Skin(in));
	else if (strncmp(packet_key.type, "WALK", 4) == 0)
		packets.emplace_back(new Walk_mesh(in));
	else
		packets.push_back(nullptr);
}

void MDB_file::save(const char* filename)
{
	std::ofstream out(filename, std::ios::binary);

	header.packet_count = packets.size();

	out.write((char*)&header, sizeof(Header));
}

MDB_file::operator bool() const
{
	return is_good_;
}

std::string MDB_file::Packet::type_str() const
{
	switch(type) {
	case MDB_file::COL2:
		return "COL2";
	case MDB_file::COL3:
		return "COL3";
	case MDB_file::COLS:
		return "COLS";
	case MDB_file::HAIR:
		return "HAIR";
	case MDB_file::HELM:
		return "HELM";
	case MDB_file::HOOK:
		return "HOOK";
	case MDB_file::RIGD:
		return "RIGD";
	case MDB_file::SKIN:
		return "SKIN";
	case MDB_file::TRRN:
		return "TRRN";
	case MDB_file::WALK:
		return "WALK";
	}

	return "UNKNOWN";
}

MDB_file::Collision_mesh::Collision_mesh(std::ifstream& in)
{
	read(in);
}

void MDB_file::Collision_mesh::read(std::ifstream& in)
{
	::read(in, header);

	if(strncmp(header.type, "COL2", 4) == 0)
		type = COL2;
	else
		type = COL3;

	verts.resize(header.vertex_count);
	::read(in, verts);

	faces.resize(header.face_count);
	::read(in, faces);
}

MDB_file::Rigid_mesh::Rigid_mesh(std::ifstream& in)
{
	read(in);
}

void MDB_file::Rigid_mesh::read(std::ifstream& in)
{
	type = RIGD;

	::read(in, header);

	verts.resize(header.vertex_count);
	::read(in, verts);

	faces.resize(header.face_count);
	::read(in, faces);
}

MDB_file::Skin::Skin(std::ifstream& in)
{
	read(in);
}

void MDB_file::Skin::read(std::ifstream& in)
{
	type = SKIN;

	::read(in, header);

	verts.resize(header.vertex_count);
	::read(in, verts);

	faces.resize(header.face_count);
	::read(in, faces);
}

MDB_file::Hook::Hook(std::ifstream& in)
{
	read(in);
}

void MDB_file::Hook::read(std::ifstream& in)
{
	type = HOOK;

	::read(in, header);
}

MDB_file::Walk_mesh::Walk_mesh(std::ifstream& in)
{
	read(in);
}

void MDB_file::Walk_mesh::read(std::ifstream& in)
{
	type = WALK;

	::read(in, header);

	verts.resize(header.vertex_count);
	::read(in, verts);

	faces.resize(header.face_count);
	::read(in, faces);
}
