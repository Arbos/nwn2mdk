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

#include <algorithm>
#include <experimental/filesystem>
#include <iostream>

#include "archive_container.h"
#include "config.h"
#include "export_gr2.h"
#include "export_mdb.h"
#include "fbxsdk.h"
#include "gr2_file.h"
#include "mdb_file.h"

// Uncomment for print extra info
//#define VERBOSE

using namespace std;
using namespace std::experimental::filesystem::v1;

static void print_header(const MDB_file& mdb)
{
	cout << "Major Version: " << mdb.major_version() << endl;
	cout << "Minor Version: " << mdb.minor_version() << endl;
	cout << "Packet Count:  " << mdb.packet_count() << endl;
	cout << endl;
}

static void print_vector3(const Vector3<float>& v)
{
	cout << v.x << ", " << v.y << ", " << v.z << endl;
}

static void print_material_flags(uint32_t flags)
{
	cout << "Flags:          0x" << hex << flags << dec << endl;
	if(flags & MDB_file::ALPHA_TEST)
		cout << "                ALPHA_TEST\n";
	if(flags & MDB_file::ALPHA_BLEND)
		cout << "                ALPHA_BLEND\n";
	if(flags & MDB_file::ADDITIVE_BLEND)
		cout << "                ADDITIVE_BLEND\n";
	if(flags & MDB_file::ENVIRONMENT_MAPPING)
		cout << "                ENVIRONMENT_MAPPING\n";
	if(flags & MDB_file::CUTSCENE_MESH)
		cout << "                CUTSCENE_MESH\n";
	if(flags & MDB_file::GLOW)
		cout << "                GLOW\n";
	if(flags & MDB_file::CAST_NO_SHADOWS)
		cout << "                CAST_NO_SHADOWS\n";
	if(flags & MDB_file::PROJECTED_TEXTURES)
		cout << "                PROJECTED_TEXTURES\n";
}

static void print_material(const MDB_file::Material& material)
{
	cout << "Diffuse Map:    " << string(material.diffuse_map_name, 32)
	     << endl;
	cout << "Normal Map:     " << string(material.normal_map_name, 32)
	     << endl;
	cout << "Tint Map:       " << string(material.tint_map_name, 32)
	     << endl;
	cout << "Glow Map:       " << string(material.glow_map_name, 32)
	     << endl;
	cout << "Diffuse Color:  ";
	print_vector3(material.diffuse_color);
	cout << "Specular Color: ";
	print_vector3(material.specular_color);
	cout << "Specular Power: " << material.specular_power << endl;
	cout << "Specular Value: " << material.specular_value << endl;
	print_material_flags(material.flags);
}

#ifdef VERBOSE

template <typename T>
static void print_verts(const std::vector<T>& verts)
{
	for (auto& vert : verts) {
		cout << "v ";
		print_vector3(vert.position);
	}
}

template <>
void print_verts(const std::vector<MDB_file::Rigid_mesh_vertex>& verts)
{
	for (auto& vert : verts) {
		cout << "v   ";
		print_vector3(vert.position);
		cout << "vn  ";
		print_vector3(vert.normal);
		cout << "vta ";
		print_vector3(vert.tangent);
		cout << "vbi ";
		print_vector3(vert.binormal);
		cout << "uvw ";
		print_vector3(vert.uvw);
	}
}

template <typename T>
static void print_faces(const std::vector<T>& faces)
{
	for (auto& face : faces) {
		cout << "p " << face.vertex_indices[0] << ' '
		     << face.vertex_indices[1] << ' ' << face.vertex_indices[2]
		     << endl;
	}
}

#endif

static void print_collision_mesh(const MDB_file::Collision_mesh& cm)
{
	cout << "Signature:      " << string(cm.header.type, 4) << endl;
	cout << "Size:           " << cm.header.packet_size << endl;
	cout << "Name:           " << string(cm.header.name, 32).c_str()
	     << endl;
	print_material(cm.header.material);
	cout << "Vertices:       " << cm.header.vertex_count << endl;
	cout << "Faces:          " << cm.header.face_count << endl;

#ifdef VERBOSE
	print_verts(cm.verts);
	print_faces(cm.faces);
#endif
}

static void print_hook(const MDB_file::Hook& hook)
{
	cout << "Signature:   " << string(hook.header.type, 4) << endl;
	cout << "Size:        " << hook.header.packet_size << endl;
	cout << "Name:        " << string(hook.header.name, 32).c_str() << endl;
	cout << "Type:        " << hook.header.point_type << endl;
	cout << "Size:        " << hook.header.point_size << endl;
	cout << "Position:    ";
	print_vector3(hook.header.position);

	cout << "Orientation:\n";
	for (int i = 0; i < 3; ++i) {
		cout << "  ";
		for (int j = 0; j < 3; ++j)
			cout << hook.header.orientation[i][j] << ' ';
		cout << endl;
	}
}

static void print_rigid_mesh(const MDB_file::Rigid_mesh& rm)
{
	cout << "Signature:      " << string(rm.header.type, 4) << endl;
	cout << "Size:           " << rm.header.packet_size << endl;
	cout << "Name:           " << string(rm.header.name, 32).c_str()
	     << endl;
	print_material(rm.header.material);
	cout << "Vertices:       " << rm.header.vertex_count << endl;
	cout << "Faces:          " << rm.header.face_count << endl;

#ifdef VERBOSE
	print_verts(rm.verts);
	print_faces(rm.faces);
#endif
}

static void print_skin(const MDB_file::Skin& skin)
{
	cout << "Signature:      " << string(skin.header.type, 4) << endl;
	cout << "Size:           " << skin.header.packet_size << endl;
	cout << "Name:           " << string(skin.header.name, 32).c_str()
	     << "\n";
	cout << "Skeleton:       "
	     << string(skin.header.skeleton_name, 32).c_str() << endl;
	print_material(skin.header.material);
	cout << "Vertices:       " << skin.header.vertex_count << endl;
	cout << "Faces:          " << skin.header.face_count << endl;

#ifdef VERBOSE
	print_verts(skin.verts);
	print_faces(skin.faces);
#endif
}

static void print_walk_mesh(const MDB_file::Walk_mesh& wm)
{
	cout << "Signature:      " << string(wm.header.type, 4) << endl;
	cout << "Size:           " << wm.header.packet_size << endl;
	cout << "Name:           " << string(wm.header.name, 32).c_str()
	     << endl;
	cout << "Vertices:       " << wm.header.vertex_count << endl;
	cout << "Faces:          " << wm.header.face_count << endl;

#ifdef VERBOSE
	print_verts(wm.verts);
	print_faces(wm.faces);
#endif
}

static void print_packet(const MDB_file::Packet* packet)
{
	if (!packet)
		return;

	cout << packet->type_str() << endl;
	cout << "----\n";

	switch (packet->type) {
	case MDB_file::COL2:
	case MDB_file::COL3:
		print_collision_mesh(
		    *static_cast<const MDB_file::Collision_mesh*>(packet));
		break;
	case MDB_file::HOOK:
		print_hook(*static_cast<const MDB_file::Hook*>(packet));
		break;
	case MDB_file::RIGD:
		print_rigid_mesh(
		    *static_cast<const MDB_file::Rigid_mesh*>(packet));
		break;
	case MDB_file::SKIN:
		print_skin(*static_cast<const MDB_file::Skin*>(packet));
		break;
	case MDB_file::WALK:
		print_walk_mesh(
		    *static_cast<const MDB_file::Walk_mesh*>(packet));
		break;
	default:
		break;
	}

	cout << endl;
}

static void print_mdb(const MDB_file& mdb)
{
	cout << endl;
	cout << "===\n";
	cout << "MDB\n";
	cout << "===\n";

	print_header(mdb);

	for (uint32_t i = 0; i < mdb.packet_count(); ++i)
		print_packet(mdb.packet(i));
}

static Archive_container get_model_archives(const Config& config)
{
	const char* files[] = {
		"Data/NWN2_Models_X2_v121.zip", "Data/NWN2_Models_X2.zip",
		"Data/NWN2_Models_X1_v121.zip", "Data/NWN2_Models_X1.zip",
		"Data/NWN2_Models_v121.zip",    "Data/NWN2_Models_v112.zip",
		"Data/NWN2_Models_v107.zip",    "Data/NWN2_Models_v106.zip",
		"Data/NWN2_Models_v105.zip",    "Data/NWN2_Models_v104.zip",
		"Data/NWN2_Models_v103x1.zip",  "Data/NWN2_Models.zip",
		"Data/lod-merged_X2_v121.zip", "Data/lod-merged_X2.zip",
		"Data/lod-merged_X1_v121.zip", "Data/lod-merged_X1.zip",
		"Data/lod-merged_v121.zip",    "Data/lod-merged_v107.zip",
		"Data/lod-merged_v101.zip",    "Data/lod-merged.zip" };

	Archive_container archives;
	for (unsigned i = 0; i < sizeof(files) / sizeof(char*); ++i) {
		cout << "Indexing: " << files[i];
		path p = path(config.nwn2_home) / path(files[i]);
		if (!archives.add_archive(p.string().c_str())) {
			cout << " : Cannot open zip";
		}
		cout << endl;
	}

	return archives;
}

static Archive_container get_lod_archives(const Config& config)
{
	const char* files[] = {
		"Data/lod-merged_X2_v121.zip", "Data/lod-merged_X2.zip",
		"Data/lod-merged_X1_v121.zip", "Data/lod-merged_X1.zip",
		"Data/lod-merged_v121.zip",    "Data/lod-merged_v107.zip",
		"Data/lod-merged_v101.zip",    "Data/lod-merged.zip" };

	Archive_container archives;
	for (unsigned i = 0; i < sizeof(files) / sizeof(char*); ++i) {
		cout << "Indexing: " << files[i];
		path p = path(config.nwn2_home) / path(files[i]);
		if (!archives.add_archive(p.string().c_str())) {
			cout << " : Cannot open zip";
		}
		cout << endl;
	}

	return archives;
}

static Archive_container get_material_archives(const Config& config)
{
	const char* files[] = { "Data/NWN2_Materials_X2.zip",
		"Data/NWN2_Materials_X1_v121.zip",
		"Data/NWN2_Materials_X1_v113.zip",
		"Data/NWN2_Materials_X1.zip",
		"Data/NWN2_Materials_v121.zip",
		"Data/NWN2_Materials_v112.zip",
		"Data/NWN2_Materials_v110.zip",
		"Data/NWN2_Materials_v107.zip",
		"Data/NWN2_Materials_v106.zip",
		"Data/NWN2_Materials_v104.zip",
		"Data/NWN2_Materials_v103x1.zip",
		"Data/NWN2_Materials.zip" };

	Archive_container archives;
	for (unsigned i = 0; i < sizeof(files) / sizeof(char*); ++i) {
		cout << "Indexing: " << files[i];
		path p = path(config.nwn2_home) / path(files[i]);
		if (!archives.add_archive(p.string().c_str())) {
			cout << " : Cannot open zip";
		}
		cout << endl;
	}

	return archives;
}

bool find_and_extract_mdb(const Config& config, const char* pattern,
                          std::string& filename)
{
	auto model_archives = get_model_archives(config);
	auto r = model_archives.find_file(pattern);
	if (r.matches != 1)
		return false;

	path p(model_archives.filename(r.archive_index, r.file_index));
	p = path("output") / p.filename();
	filename = p.string();

	cout << "Extracting: " << filename << endl; 

	if (!model_archives.extract_file(r.archive_index, r.file_index,
	                                 filename.c_str())) {
		cout << "Cannot extract\n";
		return false;
	}

	return true;
}

static bool process_arg(const Config& config, char *arg,
	std::vector<std::string> &filenames)
{
	if (exists(arg)) {
		filenames.push_back(arg);
		return true;
	}

	static auto model_archives = get_model_archives(config);
	auto r = model_archives.find_file(arg);
	if (r.matches != 1)
		return false;

	path p(model_archives.filename(r.archive_index, r.file_index));
	p = path("output") / p.filename();
	string filename = p.string();

	cout << "Extracting: " << filename << endl;

	if (!model_archives.extract_file(r.archive_index, r.file_index,
		filename.c_str())) {
		cout << "  Cannot extract\n";
		return false;
	}

	filenames.push_back(filename);
	
	return true;
}

bool process_args(Export_info& export_info, int argc, char* argv[],
	std::vector<std::string> &filenames)
{
	for (int i = 1; i < argc; ++i) {
		if (!process_arg(export_info.config, argv[i], filenames))
			return false;
	}

	struct Input {
		std::unique_ptr<MDB_file> mdb;
		std::unique_ptr<GR2_file> gr2;
	};

	vector<Input> inputs;

	for (auto &filename : filenames) {
		auto ext = path(filename).extension().string();
		transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
		if (ext == ".MDB") {
			Input input;
			input.mdb.reset(new MDB_file(filename.c_str()));
			if (!(*input.mdb)) {
				cout << input.mdb->error_str() << endl;
				return false;
			}
			inputs.push_back(move(input));
		}
		else if (ext == ".GR2") {
			Input input;
			input.gr2.reset(new GR2_file(filename.c_str()));
			if (!(*input.gr2)) {
				cout << input.gr2->error_string() << endl;
				return false;
			}
			inputs.push_back(move(input));
		}
	}

	for (auto &input : inputs) {
		if (input.gr2 && input.gr2->file_info->skeletons_count > 0) {
			auto &dep = export_info.dependencies[input.gr2->file_info->from_file_name];			
			export_gr2(*input.gr2, export_info.scene, dep.fbx_bones);
			dep.exported = true;
			dep.extracted = true;
		}
	}

	for (auto &input : inputs) {
		if (input.gr2 && input.gr2->file_info->skeletons_count == 0) {
			vector<FbxNode*> fbx_bones;
			export_gr2(*input.gr2, export_info.scene, fbx_bones);
		}
	}

	for (auto &input : inputs) {
		if (input.mdb) {
			print_mdb(*input.mdb);

			if (!export_mdb(export_info, *input.mdb)) {
				cout << "Cannot export MDB\n";
				return false;
			}
		}
	}

	return true;
}

int main(int argc, char* argv[])
{
	Config config;

	if (config.nwn2_home.empty() || !exists(config.nwn2_home)) {
		cout << "Cannot find a NWN2 installation directory. Edit the "
		        "config.yml file and put the directory where NWN2 is "
		        "installed.\n";
		return 1;
	}

	GR2_file::granny2dll_filename = config.nwn2_home + "\\granny2.dll";

	if (argc < 2) {
		cout << "Usage: nw2fbx <file|substring ...>\n";
		return 1;
	}

	create_directory("output");

	auto manager = FbxManager::Create();
	if (!manager) {
		cout << "Unable to create FBX manager\n";
		return 1;
	}

	// Create an IOSettings object. This object holds all import/export
	// settings.
	auto ios = FbxIOSettings::Create(manager, IOSROOT);
	manager->SetIOSettings(ios);

	// Create an FBX scene. This object holds most objects imported/exported
	// from/to files.
	auto scene = FbxScene::Create(manager, "Scene");
	if (!scene) {
		cout << "Unable to create FBX scene\n";
		return 1;
	}

	scene->GetGlobalSettings().SetTimeMode(FbxTime::eFrames30);

	Export_info export_info = { config,
		get_material_archives(config),
		get_lod_archives(config), nullptr, scene };

	vector<std::string> filenames;
	if (!process_args(export_info, argc, argv, filenames))
		return 1;

	// Create an exporter.
	auto exporter = FbxExporter::Create(manager, "");
	auto fbx_filename =
		(path("output") / path(filenames[0]).stem()).concat(".fbx").string();
	if (!exporter->Initialize(fbx_filename.c_str(), -1, manager->GetIOSettings())) {
		cout << exporter->GetStatus().GetErrorString() << endl;
		return false;
	}
	exporter->SetFileExportVersion(
		FBX_2014_00_COMPATIBLE); // Blender needs this version
	exporter->Export(scene);
	exporter->Destroy();

	manager->Destroy();

	cout << "\nOutput is " << fbx_filename.c_str() << endl;
}