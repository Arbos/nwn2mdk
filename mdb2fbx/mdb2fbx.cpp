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
#include "redirect_output_handle.h"

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

static void print_collision_spheres(const MDB_file::Collision_spheres& cs)
{
	cout << "Signature:      " << string(cs.header.type, 4) << endl;
	cout << "Size:           " << cs.header.packet_size << endl;		
	cout << "Spheres:        " << cs.header.sphere_count << endl;	
}

static void print_orientation(const float orientation[3][3])
{
	for (int i = 0; i < 3; ++i) {
		cout << "  ";
		for (int j = 0; j < 3; ++j)
			cout << orientation[i][j] << ' ';
		cout << endl;
	}
}

static void print_hair(const MDB_file::Hair& hair)
{
	cout << "Signature:   " << string(hair.header.type, 4) << endl;
	cout << "Size:        " << hair.header.packet_size << endl;
	cout << "Name:        " << string(hair.header.name, 32).c_str() << endl;

	cout << "Shortening:  " << hair.header.shortening_behavior;	
	switch (hair.header.shortening_behavior) {
	case MDB_file::HSB_LOW:
		cout << " (LOW)\n";
		break;
	case MDB_file::HSB_SHORT:
		cout << " (SHORT)\n";
		break;
	case MDB_file::HSB_PONYTAIL:
		cout << " (PONYTAIL)\n";
		break;
	}

	cout << "Position:    ";
	print_vector3(hair.header.position);

	cout << "Orientation:\n";
	print_orientation(hair.header.orientation);	
}

static void print_helm(const MDB_file::Helm& helm)
{
	cout << "Signature:   " << string(helm.header.type, 4) << endl;
	cout << "Size:        " << helm.header.packet_size << endl;
	cout << "Name:        " << string(helm.header.name, 32).c_str() << endl;

	cout << "Hiding:      " << helm.header.hiding_behavior;
	switch (helm.header.hiding_behavior) {
	case MDB_file::HHHB_NONE_HIDDEN:
		cout << " (NONE_HIDDEN)\n";
		break;
	case MDB_file::HHHB_HAIR_HIDDEN:
		cout << " (HAIR_HIDDEN)\n";
		break;
	case MDB_file::HHHB_PARTIAL_HAIR:
		cout << " (PARTIAL_HAIR)\n";
		break;
	case MDB_file::HHHB_HEAD_HIDDEN:
		cout << " (HEAD_HIDDEN)\n";
		break;
	}

	cout << "Position:    ";
	print_vector3(helm.header.position);

	cout << "Orientation:\n";
	print_orientation(helm.header.orientation);
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
	print_orientation(hook.header.orientation);
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
	case MDB_file::COLS:
		print_collision_spheres(*static_cast<const MDB_file::Collision_spheres*>(packet));
		break;
	case MDB_file::HAIR:
		print_hair(*static_cast<const MDB_file::Hair*>(packet));
		break;
	case MDB_file::HELM:
		print_helm(*static_cast<const MDB_file::Helm*>(packet));
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

static bool extract_arg(const Config& config, char *arg,
	std::vector<std::string> &filenames)
{
	if (exists(arg)) { // File is already extracted
		filenames.push_back(arg);
		return true;
	}

	static auto model_archives = get_model_archives(config);
	auto r = model_archives.find_file(arg);
	if (r.matches != 1)
		return false;

	path p(model_archives.filename(r.archive_index, r.file_index));
	p = p.filename();
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

static bool extract_args(Export_info& export_info, int argc, char* argv[],
	std::vector<std::string> &filenames)
{
	for (int i = 1; i < argc; ++i) {
		if (!extract_arg(export_info.config, argv[i], filenames))
			return false;
	}

	return true;
}

struct Input {
	std::string filename;
	std::unique_ptr<MDB_file> mdb;
	std::unique_ptr<GR2_file> gr2;
};

static bool open_mdb(vector<Input>& inputs, const char* filename)
{
	Input input;
	input.filename = filename;
	input.mdb.reset(new MDB_file(filename));
	if (!(*input.mdb)) {
		cout << input.mdb->error_str() << endl;
		return false;
	}
	inputs.push_back(move(input));

	return true;
}

static bool open_gr2(vector<Input>& inputs, const char* filename)
{
#ifdef _WIN32
	Input input;
	input.filename = filename;
	input.gr2.reset(new GR2_file(filename));
	if (!(*input.gr2)) {
		cout << input.gr2->error_string() << endl;
		return false;
	}
	inputs.push_back(move(input));
#endif

	return true;
}

static bool open_file(vector<Input>& inputs, const char* filename)
{
	auto ext = path(filename).extension().string();
	transform(ext.begin(), ext.end(), ext.begin(), ::toupper);
	if (ext == ".MDB") {
		if (!open_mdb(inputs, filename))
			return false;
	}
	else if (ext == ".GR2") {
		if (!open_gr2(inputs, filename))
			return false;
	}

	return true;
}

static bool open_files(vector<Input>& inputs, const std::vector<std::string>& filenames)
{
	for (auto &filename : filenames) {
		if (!open_file(inputs, filename.c_str()))
			return false;
	}

	return true;
}

#ifdef _WIN32
static void process_fbx_bones(Dependency& dep)
{
	FbxNode* ribcage = nullptr;

	for (auto it = dep.fbx_bones.begin(); it != dep.fbx_bones.end();) {
		if (strncmp((*it)->GetName(), "ap_", 3) == 0) {
			// "ap_..." (attachment point) bones are not used for skinning.			
		}
		else if (strncmp((*it)->GetName(), "f_", 2) == 0) {
			// "f_..." (face) bones are only used for head skinning.
			dep.fbx_face_bones.push_back(*it);			
		}
		else if (strcmp((*it)->GetName(), "Ribcage") == 0) {
			// Keep "Ribcage" bone to reinsert it later.
			ribcage = *it;			
		}
		else {
			// The remaining bones are used for body skinning.
			dep.fbx_body_bones.push_back(*it);
		}
			++it;
	}

	if (ribcage) {
		// Reinsert "Ribcage" bone. For some unknown reason, it seems		
		// this bone must be always the last one.
		dep.fbx_body_bones.push_back(ribcage);
	}
}
#endif

static bool export_skeletons(Export_info& export_info, vector<Input>& inputs)
{
#ifdef _WIN32
	for (auto &input : inputs) {
		if (input.gr2 && input.gr2->file_info->skeletons_count > 0) {
			auto &dep = export_info.dependencies[input.filename];
			export_gr2(*input.gr2, export_info.scene, dep.fbx_bones);
			dep.exported = true;
			dep.extracted = true;
			process_fbx_bones(dep);
		}
	}
#endif

	return true;
}

static bool export_animations(Export_info& export_info, vector<Input>& inputs)
{
#ifdef _WIN32
	for (auto &input : inputs) {
		if (input.gr2 && input.gr2->file_info->skeletons_count == 0) {
			vector<FbxNode*> fbx_bones;
			export_gr2(*input.gr2, export_info.scene, fbx_bones);
		}
	}
#endif

	return true;
}

static bool export_mdb_files(Export_info& export_info, vector<Input>& inputs)
{
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

static bool process_args(Export_info& export_info, int argc, char* argv[],
	std::vector<std::string> &filenames)
{
	if (!extract_args(export_info, argc, argv, filenames))
		return false;

	vector<Input> inputs;

	if (!open_files(inputs, filenames))
		return false;

	if (!export_skeletons(export_info, inputs))
		return false;

	if (!export_mdb_files(export_info, inputs))
		return false;

	if (!export_animations(export_info, inputs))
		return false;

	return true;
}

int main(int argc, char* argv[])
{
	Redirect_output_handle redirect_output_handle;

	Config config((path(argv[0]).parent_path() / "config.yml").string().c_str());

	if (config.nwn2_home.empty())		
		return 1;

#ifdef _WIN32
	GR2_file::granny2dll_filename = config.nwn2_home + "\\granny2.dll";
#endif

	if (argc < 2) {
		cout << "Usage: nw2fbx <file|substring ...>\n";
		return 1;
	}	

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
		get_material_archives(config), nullptr, scene };

	vector<std::string> filenames;
	if (!process_args(export_info, argc, argv, filenames))
		return 1;

	// Create an exporter.
	auto exporter = FbxExporter::Create(manager, "");
	auto fbx_filename =
		path(filenames[0]).stem().concat(".fbx").string();
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