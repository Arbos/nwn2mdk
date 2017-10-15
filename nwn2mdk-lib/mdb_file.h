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

#include "cgmath.h"

/// Represents a MDB file.
class MDB_file {
public:
	struct Rigid_mesh_vertex {
		Vector3<float> position;
		Vector3<float> normal;
		Vector3<float> tangent;
		Vector3<float> binormal;
		Vector3<float> uvw; ///< Texture coordinates.
	};

	struct Collision_mesh_vertex {
		Vector3<float> position;
		Vector3<float> normal;
		Vector3<float> uvw; ///< Texture coordinates.
	};

	struct Skin_vertex {
		Vector3<float> position;
		Vector3<float> normal;
		float bone_weights[4];
		unsigned char bone_indices[4];
		Vector3<float> tangent;
		Vector3<float> binormal;
		Vector3<float> uvw; ///< Texture coordinates.
		float bone_count;
	};

	struct Walk_mesh_vertex {
		Vector3<float> position;
	};

	/// Face of a mesh. It's always a triangle.
	struct Face {
		uint16_t vertex_indices[3];
	};

	struct Walk_mesh_face : Face {
		/// @todo Create a class for the flags?
		uint16_t flags[2];
	};

	struct Collision_sphere {
		uint32_t bone_index;
		float radius;
	};

	enum Material_flags {
		/// Alpha map values from the diffuse map below 50% grey are not
		/// drawn.
		ALPHA_TEST = 0x01,
		/// Should not be used, performance or not implemented ?
		ALPHA_BLEND = 0x02,
		/// Should not be used, performance or not implemented ?
		ADDITIVE_BLEND = 0x04,
		/// Creates a mirroring effect on the object.
		ENVIRONMENT_MAPPING = 0x08,
		/// Likely for highest resolution meshes only used in cutscenes.
		CUTSCENE_MESH = 0x10,
		/// Enables the illumination map to create a glowing effect.
		GLOW = 0x20,
		/// Does not cast shadows.
		CAST_NO_SHADOWS = 0x40,
		/// The model will accept UI projected textures such as the
		/// spell targeting cursor.
		PROJECTED_TEXTURES = 0x80
	};

	struct Material {
		/// Same as filename but without extension. Don't assume it's
		/// null terminated.
		char diffuse_map_name[32];
		/// Same as filename but without extension. Don't assume it's
		/// null terminated.
		char normal_map_name[32];
		/// Same as filename but without extension. Don't assume it's
		/// null terminated.
		char tint_map_name[32];
		/// Same as filename but without extension. Don't assume it's
		/// null terminated.
		char glow_map_name[32];
		Vector3<float> diffuse_color;
		Vector3<float> specular_color;
		float specular_power;
		float specular_value;
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
		/// Skeleton name (same as filename but without extension).
		/// Don't assume it's null terminated.
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
		Vector3<float> position;
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

	struct Collision_spheres_header : Packet_header {
		uint32_t sphere_count; ///< Number of collision spheres
	};

	enum Packet_type {
		COL2, ///< Collision Mesh 2
		COL3, ///< Collision Mesh 3
		COLS, ///< Collision Sphere
		HAIR, ///< Hair Shortening Behavior
		HELM, ///< Helm Hair Hiding Behavior
		HOOK, ///< Hook Point
		RIGD, ///< Rigid Mesh
		SKIN, ///< Skin Mesh
		TRRN, ///< Terrain
		WALK, ///< Walk Mesh
	};

	/// Abstract base class for packets.
	class Packet {
	public:
		Packet_type type;

		/// Retuns packet type as string.
		const char* type_str() const;

		virtual uint32_t packet_size() = 0;
		virtual void read(std::istream& in) = 0;
		virtual void write(std::ostream& out) = 0;
	};

	/// Represents a rigid mesh (packet type RIGD).
	class Rigid_mesh : public Packet {
	public:
		Rigid_mesh_header header;
		std::vector<Rigid_mesh_vertex> verts;
		std::vector<Face> faces;

		Rigid_mesh();
		Rigid_mesh(std::istream& in);

		virtual uint32_t packet_size() override;
		void read(std::istream& in) override;
		void write(std::ostream& out) override;
	};

	/// Represents a collision mesh (packet type COL2 or COL3).
	class Collision_mesh : public Packet {
	public:
		Collision_mesh_header header;
		std::vector<Collision_mesh_vertex> verts;
		std::vector<Face> faces;

		Collision_mesh(Packet_type t);
		Collision_mesh(std::istream& in);

		virtual uint32_t packet_size() override;
		void read(std::istream& in) override;
		void write(std::ostream& out) override;
	};

	/// Represents a skin (packet type SKIN).
	class Skin : public Packet {
	public:
		Skin_header header;
		std::vector<Skin_vertex> verts;
		std::vector<Face> faces;

		Skin();
		Skin(std::istream& in);

		virtual uint32_t packet_size() override;
		void read(std::istream& in) override;
		void write(std::ostream& out) override;
	};

	/// Represents a hook (packet type HOOK).
	class Hook : public Packet {
	public:
		Hook_header header;

		Hook();
		Hook(std::istream& in);

		virtual uint32_t packet_size() override;
		void read(std::istream& in) override;
		void write(std::ostream& out) override;
	};

	/// Represents a walk mesh (packet type WALK).
	class Walk_mesh : public Packet {
	public:
		Walk_mesh_header header;
		std::vector<Walk_mesh_vertex> verts;
		std::vector<Walk_mesh_face> faces;

		Walk_mesh();
		Walk_mesh(std::istream& in);

		virtual uint32_t packet_size() override;
		void read(std::istream& in) override;
		void write(std::ostream& out) override;
	};

	/// Represents collision spheres (packet type COLS).
	class Collision_spheres : public Packet {
	public:
		Collision_spheres_header header;
		std::vector<Collision_sphere> spheres;

		Collision_spheres();
		Collision_spheres(std::istream& in);

		uint32_t packet_size() override;
		void read(std::istream& in) override;
		void write(std::ostream& out) override;
	};

	struct Walk_mesh_material {
		char *name;
		uint16_t flags;
		Vector3<float> color;
	};

	static Walk_mesh_material walk_mesh_materials[12];

	/// Constructs an empty MDB.
	MDB_file();

	/// Opens a MDB file at the specified path.
	///
	/// @param filename The name of the file to be opened.
	MDB_file(const char* filename);

	/// Reads a MDB from a stream.
	MDB_file(std::istream& in);

	/// Adds a packet.
	///
	/// @param The packet to add.
	void add_packet(std::unique_ptr<Packet> packet);

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

	/// Saves to a file.
	///
	/// @param filename The name of the file.
	void save(const char* filename);

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
	static_assert(sizeof(Collision_sphere) == 8);
	static_assert(sizeof(Collision_spheres_header) == 12);
	static_assert(sizeof(Face) == 6);
	static_assert(sizeof(Header) == 12);
	static_assert(sizeof(Hook_header) == 92);
	static_assert(sizeof(Material) == 164);
	static_assert(sizeof(Packet_key) == 8);
	static_assert(sizeof(Rigid_mesh_header) == 212);
	static_assert(sizeof(Rigid_mesh_vertex) == 60);
	static_assert(sizeof(Skin_header) == 244);
	static_assert(sizeof(Skin_vertex) == 84);
	static_assert(sizeof(Walk_mesh_face) == 10);
	static_assert(sizeof(Walk_mesh_header) == 52);
	static_assert(sizeof(Walk_mesh_vertex) == 12);

	bool is_good_;
	std::string error_str_;
	Header header;
	std::vector<Packet_key> packet_keys;
	std::vector<std::unique_ptr<Packet>> packets;

	void read(std::istream& in);
	void read_packets(std::istream& in);
	void read_packet(Packet_key& packet_key, std::istream& in);
};
