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

#include <experimental/filesystem>
#include <iostream>
#include <set>

#include "archive_container.h"
#include "config.h"
#include "fbxsdk.h"
#include "mdb_file.h"
#include "miniz.h"

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

static void print_vector3(const MDB_file::Vector3& v)
{
	cout << v.x << ", " << v.y << ", " << v.z << endl;
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
}

template <typename T>
static void print_verts(const std::vector<T>& verts)
{
	for (auto& vert : verts) {
		cout << "v ";
		print_vector3(vert.position);
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
	print_header(mdb);

	for (uint32_t i = 0; i < mdb.packet_count(); ++i)
		print_packet(mdb.packet(i));
}

static FbxSurfacePhong *create_material(FbxManager* manager, FbxNode* node,
                            const MDB_file::Material& mdb_material,
                            const char* name)
{
	FbxSurfacePhong* material = FbxSurfacePhong::Create(manager, name);
	material->Diffuse.Set(FbxDouble3(mdb_material.diffuse_color.x,
	                                 mdb_material.diffuse_color.y,
	                                 mdb_material.diffuse_color.z));
	material->Specular.Set(FbxDouble3(mdb_material.specular_color.x,
	                                  mdb_material.specular_color.y,
	                                  mdb_material.specular_color.z));

	return material;
}

static FbxFileTexture* export_texture(FbxManager* manager, const char* name)
{
	FbxFileTexture* texture = FbxFileTexture::Create(manager, "");

	// Assuming all texture extensions are dds.
	string path = string(name) + ".dds";

	// Set texture properties.
	texture->SetFileName(path.c_str());
	texture->SetName(name);
	texture->SetTextureUse(FbxTexture::eStandard);
	texture->SetMappingType(FbxTexture::eUV);
	texture->SetMaterialUse(FbxFileTexture::eModelMaterial);
	texture->SetSwapUV(false);
	texture->SetAlphaSource(FbxTexture::eBlack);
	texture->SetTranslation(0.0, 0.0);
	texture->SetScale(1.0, 1.0);
	texture->SetRotation(0.0, 0.0);

	return texture;
}

template <typename T>
static void export_map(FbxManager* manager, FbxPropertyT<T>& property,
                       const char* map_name)
{
	if (strlen(map_name) > 0)
		property.ConnectSrcObject(export_texture(manager, map_name));
}

static void export_maps(FbxManager* manager, FbxSurfacePhong* material,
                        const MDB_file::Material& mdb_material)
{
	export_map(manager, material->Diffuse,
	           string(mdb_material.diffuse_map_name, 32).c_str());
	export_map(manager, material->NormalMap,
	           string(mdb_material.normal_map_name, 32).c_str());
	export_map(manager, material->DisplacementColor,
	           string(mdb_material.tint_map_name, 32).c_str());
	export_map(manager, material->EmissiveFactor,
	           string(mdb_material.glow_map_name, 32).c_str());
}

template <class T>
static void export_vertices(FbxMesh* mesh, const std::vector<T>& verts)
{
	// Create control points.
	mesh->InitControlPoints(verts.size());
	for (uint32_t i = 0; i < verts.size(); ++i) {
		FbxVector4 v(verts[i].position.x, verts[i].position.y,
		             verts[i].position.z);
		mesh->GetControlPoints()[i] = v;
	}
}

template <class T>
static void export_faces(FbxMesh* mesh, const std::vector<T>& faces)
{
	for (auto& face : faces) {
		mesh->BeginPolygon(-1, -1, -1, false);
		mesh->AddPolygon(face.vertex_indices[0]);
		mesh->AddPolygon(face.vertex_indices[1]);
		mesh->AddPolygon(face.vertex_indices[2]);
		mesh->EndPolygon();
	}
}

template <class T>
static void export_normals(FbxMesh* mesh, const std::vector<T>& verts)
{
	FbxGeometryElementNormal* element_normal = mesh->CreateElementNormal();
	// One normal for each vertex.
	element_normal->SetMappingMode(FbxGeometryElement::eByControlPoint);
	// Map information in DirectArray.
	element_normal->SetReferenceMode(FbxGeometryElement::eDirect);

	for (uint32_t i = 0; i < verts.size(); ++i) {
		FbxVector4 n(verts[i].normal.x, verts[i].normal.y,
		             verts[i].normal.z);
		element_normal->GetDirectArray().Add(n);
	}
}

template <class T>
static void export_uv(FbxMesh* mesh, const std::vector<T>& verts)
{
	FbxGeometryElementUV* element_uv = mesh->CreateElementUV("UVMap");
	// One UV for each vertex.
	element_uv->SetMappingMode(FbxGeometryElement::eByControlPoint);
	// Map information in DirectArray.
	element_uv->SetReferenceMode(FbxGeometryElement::eDirect);

	for (uint32_t i = 0; i < verts.size(); ++i) {
		FbxVector2 uv(verts[i].uvw.x, -verts[i].uvw.y);
		element_uv->GetDirectArray().Add(uv);
	}
}

static FbxNode* create_node(FbxScene* scene, FbxMesh *mesh, const char* name)
{	
	auto node = FbxNode::Create(scene, name);
	node->SetNodeAttribute(mesh);
	// Blender default import settings rotate 90 degrees around x-axis and
	// scale by 0.01. We appply the inverse transform to the FBX node.
	node->LclRotation.Set(FbxDouble3(-90, 0, 0));
	node->LclScaling.Set(FbxDouble3(100, 100, 100));

	return node;
}

static void export_collision_mesh(const MDB_file::Collision_mesh& cm,
                                  FbxManager* manager, FbxScene* scene)
{
	string name(cm.header.name, 32);

	auto mesh = FbxMesh::Create(scene, name.c_str());

	export_vertices(mesh, cm.verts);
	export_normals(mesh, cm.verts);
	export_uv(mesh, cm.verts);
	export_faces(mesh, cm.faces);

	auto node = create_node(scene, mesh, name.c_str());
	scene->GetRootNode()->AddChild(node);

	auto material =
	    create_material(manager, node, cm.header.material, name.c_str());
	material->TransparentColor.Set(FbxDouble3(0.5, 0.5, 0.5));
	material->TransparencyFactor.Set(0.5);
	node->AddMaterial(material);
}

static void export_rigid_mesh(const MDB_file::Rigid_mesh& rm,
                              FbxManager* manager, FbxScene* scene)
{
	string name(rm.header.name, 32);

	auto mesh = FbxMesh::Create(scene, name.c_str());

	export_vertices(mesh, rm.verts);
	export_normals(mesh, rm.verts);
	export_uv(mesh, rm.verts);
	export_faces(mesh, rm.faces);

	auto node = create_node(scene, mesh, name.c_str());
	scene->GetRootNode()->AddChild(node);

	auto material =
	    create_material(manager, node, rm.header.material, name.c_str());
	export_maps(manager, material, rm.header.material);
	node->AddMaterial(material);
}

static void export_skin(const MDB_file::Skin& skin, FbxManager* manager,
                        FbxScene* scene)
{
	string name(skin.header.name, 32);

	auto mesh = FbxMesh::Create(scene, name.c_str());

	export_vertices(mesh, skin.verts);
	export_normals(mesh, skin.verts);
	export_uv(mesh, skin.verts);
	export_faces(mesh, skin.faces);

	auto node = create_node(scene, mesh, name.c_str());
	scene->GetRootNode()->AddChild(node);

	auto material =
	    create_material(manager, node, skin.header.material, name.c_str());
	export_maps(manager, material, skin.header.material);
	node->AddMaterial(material);
}

static void export_walk_mesh(const MDB_file::Walk_mesh& wm, FbxManager* manager,
                             FbxScene* scene)
{
	string name(wm.header.name, 32);

	auto mesh = FbxMesh::Create(scene, name.c_str());

	// Create control points.
	mesh->InitControlPoints(wm.verts.size());
	for (uint32_t i = 0; i < wm.verts.size(); ++i) {
		// Some walk mesh vertices have z=-1000000. We clamp z to -20.
		FbxVector4 v(wm.verts[i].position.x, wm.verts[i].position.y,
		             max(wm.verts[i].position.z, -20.0f));
		mesh->GetControlPoints()[i] = v;
	}

	export_faces(mesh, wm.faces);

	auto node = create_node(scene, mesh, name.c_str());
	scene->GetRootNode()->AddChild(node);
}

static void export_packet(const MDB_file::Packet* packet, FbxManager* manager,
                          FbxScene* scene)
{
	if (!packet)
		return;

	switch (packet->type) {
	case MDB_file::COL2:
	case MDB_file::COL3:
		export_collision_mesh(
		    *static_cast<const MDB_file::Collision_mesh*>(packet),
		    manager, scene);
		break;
	case MDB_file::HOOK:
		break;
	case MDB_file::RIGD:
		export_rigid_mesh(
		    *static_cast<const MDB_file::Rigid_mesh*>(packet), manager,
		    scene);
		break;
	case MDB_file::SKIN:
		export_skin(*static_cast<const MDB_file::Skin*>(packet),
		            manager, scene);
		break;
	case MDB_file::WALK:
		export_walk_mesh(
		    *static_cast<const MDB_file::Walk_mesh*>(packet), manager,
		    scene);
		break;
	default:
		break;
	}
}

static bool export_mdb(const MDB_file& mdb, const char* filename)
{
	auto manager = FbxManager::Create();
	if (!manager) {
		cout << "unable to create FBX manager\n";
		return false;
	}

	// Create an IOSettings object. This object holds all import/export
	// settings.
	auto ios = FbxIOSettings::Create(manager, IOSROOT);
	manager->SetIOSettings(ios);

	// Create an FBX scene. This object holds most objects imported/exported
	// from/to files.
	auto scene = FbxScene::Create(manager, "Scene");
	if (!scene) {
		cout << "unable to create FBX scene\n";
		return false;
	}

	for (uint32_t i = 0; i < mdb.packet_count(); ++i)
		export_packet(mdb.packet(i), manager, scene);

	// Create an exporter.
	path p = path("output") / filename;
	p += ".fbx";
	auto exporter = FbxExporter::Create(manager, "");
	if (!exporter->Initialize(p.c_str(), -1, manager->GetIOSettings())) {
		cout << exporter->GetStatus().GetErrorString() << endl;
		return false;
	}
	exporter->SetFileExportVersion(
	    FBX_2014_00_COMPATIBLE); // Blender needs this version
	exporter->Export(scene);
	exporter->Destroy();

	manager->Destroy();

	return true;
}

static void gather_dependencies(const MDB_file::Material& material,
                                std::set<std::string>& dependencies)
{
	string diffuse_map = string(material.diffuse_map_name, 32).c_str();
	if (!diffuse_map.empty())
		dependencies.insert(diffuse_map + ".dds");

	string normal_map = string(material.normal_map_name, 32).c_str();
	if (!normal_map.empty())
		dependencies.insert(normal_map + ".dds");

	string tint_map = string(material.tint_map_name, 32).c_str();
	if (!tint_map.empty())
		dependencies.insert(tint_map + ".dds");

	string glow_map = string(material.glow_map_name, 32).c_str();
	if (!glow_map.empty())
		dependencies.insert(glow_map + ".dds");
}

static void gather_dependencies(const MDB_file::Rigid_mesh& rm,
                                std::set<std::string>& dependencies)
{
	gather_dependencies(rm.header.material, dependencies);
}

static void gather_dependencies(const MDB_file::Skin& skin,
                                std::set<std::string>& dependencies)
{
	gather_dependencies(skin.header.material, dependencies);
}

static void gather_dependencies(const MDB_file::Packet* packet,
                                std::set<std::string>& dependencies)
{
	if (!packet)
		return;

	switch (packet->type) {
	case MDB_file::RIGD:
		gather_dependencies(
		    *static_cast<const MDB_file::Rigid_mesh*>(packet),
		    dependencies);
		break;
	case MDB_file::SKIN:
		gather_dependencies(*static_cast<const MDB_file::Skin*>(packet),
		                    dependencies);
		break;
	default:
		break;
	}
}

static std::set<std::string> gather_dependencies(const MDB_file& mdb)
{
	set<string> dependencies;

	for (uint32_t i = 0; i < mdb.packet_count(); ++i)
		gather_dependencies(mdb.packet(i), dependencies);

	return dependencies;
}

static void extract_dependency(const char* str,
                               const Archive_container& archives)
{
	auto r = archives.find_file(str);
	if (r.matches > 0) {
		path p = path("output") / str;

		if (!archives.extract_file(r.archive_index, r.file_index,
		                           p.c_str())) {
			cout << "cannot extract " << str << endl;
		}
	}
	else
		cout << str << " not found\n";
}

static void extract_dependencies(const MDB_file& mdb,
                                 const Archive_container& archives)
{
	auto dependencies = gather_dependencies(mdb);

	for (auto& str : dependencies)
		extract_dependency(str.c_str(), archives);
}

static Archive_container get_model_archives(const Config& config)
{
	const char* files[] = {
	    "Data/NWN2_Models_X2_v121.zip", "Data/NWN2_Models_X2.zip",
	    "Data/NWN2_Models_X1_v121.zip", "Data/NWN2_Models_X1.zip",
	    "Data/NWN2_Models_v121.zip",    "Data/NWN2_Models_v112.zip",
	    "Data/NWN2_Models_v107.zip",    "Data/NWN2_Models_v106.zip",
	    "Data/NWN2_Models_v105.zip",    "Data/NWN2_Models_v104.zip",
	    "Data/NWN2_Models_v103x1.zip",  "Data/NWN2_Models.zip"};

	Archive_container model_archives;
	for (unsigned i = 0; i < sizeof(files) / sizeof(char*); ++i) {
		cout << "Indexing " << files[i] << " ...";
		path p = path(config.nwn2_home) / path(files[i]);
		if (!model_archives.add_archive(p.c_str())) {
			cout << " Cannot open zip";
		}
		cout << endl;
	}

	cout << endl;

	return model_archives;
}

static Archive_container get_material_archives(const Config& config)
{
	const char* files[] = {"Data/NWN2_Materials_X2.zip",
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
	                       "Data/NWN2_Materials.zip"};

	Archive_container material_archives;
	for (unsigned i = 0; i < sizeof(files) / sizeof(char*); ++i) {
		cout << "Indexing " << files[i] << " ...";
		path p = path(config.nwn2_home) / path(files[i]);
		if (!material_archives.add_archive(p.c_str())) {
			cout << " Cannot open zip";
		}
		cout << endl;
	}

	cout << endl;

	return material_archives;
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

	if (argc < 2) {
		cout << "Usage: mdb2fbx <file|substring>\n";
		return 1;
	}

	create_directory("output");

	string filename = argv[1];

	if(!exists(filename)) {
		auto model_archives = get_model_archives(config);
		auto r = model_archives.find_file(filename.c_str());
		if (r.matches != 1) {
			return 1;
		}

		path p(model_archives.filename(r.archive_index, r.file_index));
		p = path("output") / p.filename();

		if (!model_archives.extract_file(r.archive_index, r.file_index,
		                                 p.c_str())) {
			cout << "Cannot extract\n";
			return 1;
		}

		filename = p.c_str();
	}

	MDB_file mdb(filename.c_str());
	if (!mdb) {
		cout << mdb.error_str() << endl;
		return 1;
	}

	print_mdb(mdb);
	if (!export_mdb(mdb, path(filename).stem().c_str())) {
		cout << "Cannot export to FBX\n";
		return 1;
	}
	extract_dependencies(mdb, get_material_archives(config));
}
