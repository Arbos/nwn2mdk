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

MDB_file::MDB_file(const char* filename)
{
	std::ifstream in(filename, std::ios::in | std::ios::binary);

	is_good_ = false;

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

	for (auto& packet_key : packet_keys) {
		in.seekg(packet_key.offset);

		if (strncmp(packet_key.type, "COL2", 4) == 0) {
			packets.emplace_back(new Collision_mesh);
			packets.back()->type = COL2;
			packets.back()->read(in);
		}
		else if (strncmp(packet_key.type, "COL3", 4) == 0) {
			packets.emplace_back(new Collision_mesh);
			packets.back()->type = COL3;
			packets.back()->read(in);
		}
		else if (strncmp(packet_key.type, "HOOK", 4) == 0) {
			packets.emplace_back(new Hook);
			packets.back()->type = HOOK;
			packets.back()->read(in);
		}
		else if (strncmp(packet_key.type, "RIGD", 4) == 0) {
			packets.emplace_back(new Rigid_mesh);
			packets.back()->type = RIGD;
			packets.back()->read(in);
		}
		else if (strncmp(packet_key.type, "SKIN", 4) == 0) {
			packets.emplace_back(new Skin);
			packets.back()->type = SKIN;
			packets.back()->read(in);
		}
		else if (strncmp(packet_key.type, "WALK", 4) == 0) {
			packets.emplace_back(new Walk_mesh);
			packets.back()->type = WALK;
			packets.back()->read(in);
		}
		else
			packets.push_back(nullptr);
	}

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

void MDB_file::Collision_mesh::read(std::ifstream& in)
{
	::read(in, header);

	verts.resize(header.vertex_count);
	::read(in, verts);

	faces.resize(header.face_count);
	::read(in, faces);
}

void MDB_file::Rigid_mesh::read(std::ifstream& in)
{
	::read(in, header);

	verts.resize(header.vertex_count);
	::read(in, verts);

	faces.resize(header.face_count);
	::read(in, faces);
}

void MDB_file::Skin::read(std::ifstream& in)
{
	::read(in, header);

	verts.resize(header.vertex_count);
	::read(in, verts);

	faces.resize(header.face_count);
	::read(in, faces);
}

void MDB_file::Hook::read(std::ifstream& in)
{
	::read(in, header);
}

void MDB_file::Walk_mesh::read(std::ifstream& in)
{
	::read(in, header);

	verts.resize(header.vertex_count);
	::read(in, verts);

	faces.resize(header.face_count);
	::read(in, faces);
}
