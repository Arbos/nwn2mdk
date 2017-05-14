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

#pragma once

#include <fstream>
#include <memory>
#include <vector>

/// Represents a MDB file.
class MDB_file {
public:
	/// 3D vector.
	struct Vector3 {
		float x;
		float y;
		float z;
	};

	struct Rigid_mesh_vertex {
		Vector3 position;
		Vector3 normal;
		Vector3 tangent;
		Vector3 binormal;
		Vector3 uvw; ///< Texture coordinates.
	};

	struct Collision_mesh_vertex {
		Vector3 position;
		Vector3 normal;
		Vector3 uvw; ///< Texture coordinates.
	};

	struct Skin_vertex {
		Vector3 position;
		Vector3 normal;
		float bone_weights[4];
		unsigned char bone_indices[4];
		Vector3 tangent;
		Vector3 binormal;
		Vector3 uvw; ///< Texture coordinates.
		float bone_count;
	};

	/// Face of a mesh. It's always a triangle.
	struct Face {
		uint16_t vertex_indices[3];
	};

	struct Walk_mesh_face : Face {
		/// @todo Create a class for the flags?
		uint16_t flags[2];
	};

	struct Material {
		/// Same as filename but without extension. Don't assume it's null terminated.
		char diffuse_map_name[32];
		/// Same as filename but without extension. Don't assume it's null terminated.
		char normal_map_name[32];
		/// Same as filename but without extension. Don't assume it's null terminated.
		char tint_map_name[32];
		/// Same as filename but without extension. Don't assume it's null terminated.
		char glow_map_name[32];
		Vector3 diffuse_color;
		Vector3 specular_color;
		float specular_power;
		float specular_value;
		/// @todo Create a class for the flags?
		uint32_t flags;
	};

	struct Packet_header {
		/// Packet type.
		/// Valid values: "RIGD", "SKIN", "COL2", "COL3", "HOOK",
		/// "WALK", "COLS", "TRRN", "HELM", "HAIR"
		char type[4];
		uint32_t packet_size;
	};

	struct Rigid_mesh_header : Packet_header {
		/// Packet name. Don't assume it's null terminated.
		char name[32];
		Material material;
		uint32_t vertex_count;
		uint32_t face_count;
	};

	struct Collision_mesh_header : Packet_header {
		/// Packet name. Don't assume it's null terminated.
		char name[32];
		Material material;
		uint32_t vertex_count;
		uint32_t face_count;
	};

	struct Skin_header : Packet_header {
		/// Packet name. Don't assume it's null terminated.
		char name[32];
		/// Skeleton name (same as filename but without extension). Don't assume it's null
		/// terminated.
		char skeleton_name[32];
		Material material;
		uint32_t vertex_count;
		uint32_t face_count;
	};

	struct Hook_header : Packet_header {
		/// Packet name. Don't assume it's null terminated.
		char name[32];
		/// Unknown.
		uint16_t point_type;
		/// Unknown.
		uint16_t point_size;
		Vector3 position;
		/// 3x3 matrix.
		/// @todo By row or by column?
		float orientation[3][3];
	};

	struct Walk_mesh_header : Packet_header {
		/// Packet name. Don't assume it's null terminated.
		char name[32];
		/// Unknown. Always 0.
		uint32_t ui_flags;
		uint32_t vertex_count;
		uint32_t face_count;
	};

	enum Packet_type {
		COL2, ///< Collision Mesh 2
		COL3, ///< Collision Mesh 3
		COLS, ///< Collision Sphere
		HAIR,
		HELM,
		HOOK,
		RIGD, ///< Rigid Mesh
		SKIN,
		TRRN, ///< Terrain
		WALK, ///< Walk Mesh
	};

	/// Abstract base class for packets.
	class Packet {
	public:
		Packet_type type;

		/// Retuns packet type as string.
		std::string type_str() const;
		virtual void read(std::ifstream& in) = 0;
	};

	/// Represents a rigid mesh (packet type RIGD).
	class Rigid_mesh : public Packet {
	public:
		Rigid_mesh_header header;
		std::vector<Rigid_mesh_vertex> verts;
		std::vector<Face> faces;

		void read(std::ifstream& in) override;
	};

	/// Represents a collision mesh (packet type COL2 or COL3).
	class Collision_mesh : public Packet {
	public:
		Collision_mesh_header header;
		std::vector<Collision_mesh_vertex> verts;
		std::vector<Face> faces;

		void read(std::ifstream& in) override;
	};

	/// Represents a skin (packet type SKIN).
	class Skin : public Packet {
	public:
		Skin_header header;
		std::vector<Skin_vertex> verts;
		std::vector<Face> faces;

		void read(std::ifstream& in) override;
	};

	/// Represents a hook (packet type HOOK).
	class Hook : public Packet {
	public:
		Hook_header header;

		void read(std::ifstream& in) override;
	};

	/// Represents a walk mesh (packet type WALK).
	class Walk_mesh : public Packet {
	public:
		Walk_mesh_header header;
		std::vector<Vector3> verts;
		std::vector<Walk_mesh_face> faces;

		void read(std::ifstream& in) override;
	};

	/// Opens a MDB file at the specified path.
	///
	/// @param filename The name of the file to be opened.
	MDB_file(const char* filename);

	/// Returns the error string.
	const char* error_str() const;

	/// Returns the major version of the MDB file.
	uint16_t major_version() const;

	/// Returns the minor version of the MDB file.
	uint16_t minor_version() const;

	/// Returns a pointer to the specified packet.
	///
	/// @return If specified packet exists, returns a pointer to the
	/// packet. Otherwise, returns null pointer.
	Packet* packet(uint32_t packet_index) const;

	/// Returns the number of packets contained in the MDB file.
	uint32_t packet_count() const;

	/// Checks if no error has occurred.
	operator bool() const;

private:
	struct Header {
		char signature[4]; // Should be "NWN2"
		uint16_t major_version;
		uint16_t minor_version;
		uint32_t packet_count;
	};

	struct Packet_key {
		char type[4]; // "RIGD", "SKIN", "COL2", "COL3", "HOOK",
		              // "WALK", "COLS", "TRRN", "HELM", "HAIR"
		uint32_t offset; // Offset to the packet from beginning of file
	};

	static_assert(sizeof(Collision_mesh_header) == 212);
	static_assert(sizeof(Collision_mesh_vertex) == 36);
	static_assert(sizeof(Face) == 6);
	static_assert(sizeof(Header) == 12);
	static_assert(sizeof(Hook_header) == 92);
	static_assert(sizeof(Material) == 164);
	static_assert(sizeof(Packet_key) == 8);
	static_assert(sizeof(Vector3) == 12);
	static_assert(sizeof(Rigid_mesh_header) == 212);
	static_assert(sizeof(Rigid_mesh_vertex) == 60);
	static_assert(sizeof(Skin_header) == 244);
	static_assert(sizeof(Skin_vertex) == 84);
	static_assert(sizeof(Walk_mesh_face) == 10);
	static_assert(sizeof(Walk_mesh_header) == 52);

	std::ifstream in;
	bool is_good_;
	std::string error_str_;
	Header header;
	std::vector<Packet_key> packet_keys;
	std::vector<std::unique_ptr<Packet>> packets;
};
