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
static void export_tangents(FbxMesh* mesh, const std::vector<T>& verts)
{
	FbxGeometryElementTangent* element_tangent = mesh->CreateElementTangent();
	// One tangent for each vertex.
	element_tangent->SetMappingMode(FbxGeometryElement::eByControlPoint);
	// Map information in DirectArray.
	element_tangent->SetReferenceMode(FbxGeometryElement::eDirect);

	for (uint32_t i = 0; i < verts.size(); ++i) {
		FbxVector4 n(verts[i].tangent.x, verts[i].tangent.y,
		             verts[i].tangent.z);
		element_tangent->GetDirectArray().Add(n);
	}
}

template <class T>
static void export_binormals(FbxMesh* mesh, const std::vector<T>& verts)
{
	FbxGeometryElementBinormal* element_binormal = mesh->CreateElementBinormal();
	// One binormal for each vertex.
	element_binormal->SetMappingMode(FbxGeometryElement::eByControlPoint);
	// Map information in DirectArray.
	element_binormal->SetReferenceMode(FbxGeometryElement::eDirect);

	for (uint32_t i = 0; i < verts.size(); ++i) {
		FbxVector4 n(verts[i].binormal.x, verts[i].binormal.y,
		             verts[i].binormal.z);
		element_binormal->GetDirectArray().Add(n);
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

static void export_collision_mesh_2(const MDB_file::Collision_mesh& cm,
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
	material->Diffuse.Set(FbxDouble3(0.9, 0.4, 0.09));
	material->DiffuseFactor.Set(0.8);
	material->TransparentColor.Set(FbxDouble3(0.9, 0.4, 0.09));
	material->TransparencyFactor.Set(0.5);
	material->Specular.Set(FbxDouble3(0, 0, 0));
	material->SpecularFactor.Set(0);
	material->Shininess.Set(2);
	node->AddMaterial(material);
}

static void export_collision_mesh_3(const MDB_file::Collision_mesh& cm,
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
	material->Diffuse.Set(FbxDouble3(0.76, 0.11, 0.09));
	material->DiffuseFactor.Set(0.8);
	material->TransparentColor.Set(FbxDouble3(0.76, 0.11, 0.09));
	material->TransparencyFactor.Set(0.5);
	material->Specular.Set(FbxDouble3(0, 0, 0));
	material->SpecularFactor.Set(0);
	material->Shininess.Set(2);
	node->AddMaterial(material);
}

static void export_rigid_mesh(const MDB_file::Rigid_mesh& rm,
                              FbxManager* manager, FbxScene* scene)
{
	string name(rm.header.name, 32);

	auto mesh = FbxMesh::Create(scene, name.c_str());

	export_vertices(mesh, rm.verts);
	export_normals(mesh, rm.verts);
	export_tangents(mesh, rm.verts);
	export_binormals(mesh, rm.verts);
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
	export_tangents(mesh, skin.verts);
	export_binormals(mesh, skin.verts);
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
		export_collision_mesh_2(
		    *static_cast<const MDB_file::Collision_mesh*>(packet),
		    manager, scene);
		break;
	case MDB_file::COL3:
		export_collision_mesh_3(
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
		cout << "Unable to create FBX manager\n";
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
		cout << "Unable to create FBX scene\n";
		return false;
	}

	for (uint32_t i = 0; i < mdb.packet_count(); ++i)
		export_packet(mdb.packet(i), manager, scene);

	// Create an exporter.
	auto exporter = FbxExporter::Create(manager, "");
	if (!exporter->Initialize(filename, -1, manager->GetIOSettings())) {
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

static Archive_container get_lod_archives(const Config& config)
{
	const char* files[] = {
	    "Data/lod-merged_X2_v121.zip", "Data/lod-merged_X2.zip",
	    "Data/lod-merged_X1_v121.zip", "Data/lod-merged_X1.zip",
	    "Data/lod-merged_v121.zip",    "Data/lod-merged_v107.zip",
	    "Data/lod-merged_v101.zip",    "Data/lod-merged.zip"};

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

static Archive_container get_model_archives(const Config& config)
{
	const char* files[] = {
	    "Data/NWN2_Models_X2_v121.zip", "Data/NWN2_Models_X2.zip",
	    "Data/NWN2_Models_X1_v121.zip", "Data/NWN2_Models_X1.zip",
	    "Data/NWN2_Models_v121.zip",    "Data/NWN2_Models_v112.zip",
	    "Data/NWN2_Models_v107.zip",    "Data/NWN2_Models_v106.zip",
	    "Data/NWN2_Models_v105.zip",    "Data/NWN2_Models_v104.zip",
	    "Data/NWN2_Models_v103x1.zip",  "Data/NWN2_Models.zip"};

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

static void extract_dependency(const char* str,
                               const Archive_container& archives,
                               set<std::string>& extracted)
{
	if(extracted.find(str) != extracted.end())
		return;

	extracted.insert(str);

	auto r = archives.find_file(str);
	if (r.matches > 0) {
		path p(archives.filename(r.archive_index, r.file_index));
		p = path("output") / p.filename();
	
		cout << "Extracting: " << p.string() << endl; 

		if(exists(p)) {
			cout << "  Already exists in destination. Don't overwrite.\n";
			return;
		}

		if (!archives.extract_file(r.archive_index, r.file_index,
		                           p.string().c_str())) {
			cout << "Cannot extract " << str << endl;
		}
	}
	else
		cout << str << " not found\n";
}

static void extract_textures(const MDB_file::Material& material,
                             const Archive_container& archives,
                             set<std::string>& extracted)
{
	string diffuse_map = string(material.diffuse_map_name, 32).c_str();
	if (!diffuse_map.empty())
		extract_dependency((diffuse_map + '.').c_str(), archives,
		                   extracted);

	string normal_map = string(material.normal_map_name, 32).c_str();
	if (!normal_map.empty())
		extract_dependency((normal_map + '.').c_str(), archives,
		                   extracted);

	string tint_map = string(material.tint_map_name, 32).c_str();
	if (!tint_map.empty())
		extract_dependency((tint_map + '.').c_str(), archives,
		                   extracted);

	string glow_map = string(material.glow_map_name, 32).c_str();
	if (!glow_map.empty())
		extract_dependency((glow_map + '.').c_str(), archives,
		                   extracted);
}

static void extract_textures(const MDB_file::Rigid_mesh& rm,
                             const Archive_container& archives,
                             set<std::string>& extracted)
{
	extract_textures(rm.header.material, archives, extracted);
}

static void extract_textures(const MDB_file::Skin& skin,
                             const Archive_container& archives,
                             set<std::string>& extracted)
{
	extract_textures(skin.header.material, archives, extracted);
}

static void extract_textures(const MDB_file::Packet* packet,
                             const Archive_container& archives,
                             set<std::string>& extracted)
{
	if (!packet)
		return;

	switch (packet->type) {
	case MDB_file::RIGD:
		extract_textures(
		    *static_cast<const MDB_file::Rigid_mesh*>(packet),
		    archives, extracted);
		break;
	case MDB_file::SKIN:
		extract_textures(*static_cast<const MDB_file::Skin*>(packet),
		                 archives, extracted);
		break;
	default:
		break;
	}
}

static void extract_textures(const MDB_file& mdb, const Config& config,
                             set<std::string>& extracted)
{
	auto archives = get_material_archives(config);

	for (uint32_t i = 0; i < mdb.packet_count(); ++i)
		extract_textures(mdb.packet(i), archives, extracted);
}

static void extract_skeleton(const MDB_file::Skin& skin,
                             const Archive_container& archives,
                             set<std::string>& extracted)
{
	string skeleton = string(skin.header.skeleton_name, 32).c_str();
	if (!skeleton.empty())
		extract_dependency((skeleton + '.').c_str(), archives,
		                   extracted);
}

static void extract_skeleton(const MDB_file::Packet* packet,
                             const Archive_container& archives,
                             set<std::string>& extracted)
{
	if (!packet)
		return;

	switch (packet->type) {
	case MDB_file::SKIN:
		extract_skeleton(*static_cast<const MDB_file::Skin*>(packet),
		                 archives, extracted);
		break;
	default:
		break;
	}
}

static void extract_skeletons(const MDB_file& mdb, const Config& config,
                              set<std::string>& extracted)
{
	auto archives = get_lod_archives(config);

	for (uint32_t i = 0; i < mdb.packet_count(); ++i)
		extract_skeleton(mdb.packet(i), archives, extracted);
}

static void extract_dependencies(const MDB_file& mdb,
                                 const Config &config)
{
	// Keeps track of dependencies already extracted.
	set<std::string> extracted;

	extract_textures(mdb, config, extracted);
	extract_skeletons(mdb, config, extracted);
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
		if(!find_and_extract_mdb(config, filename.c_str(), filename))
			return 1;
	}

	MDB_file mdb(filename.c_str());
	if (!mdb) {
		cout << mdb.error_str() << endl;
		return 1;
	}

	print_mdb(mdb);
	extract_dependencies(mdb, config);

	auto fbx_filename =
	    (path("output") / path(filename).stem()).concat(".fbx").string();
	cout << "Converting: " << filename << " -> " << fbx_filename << endl;
	if (!export_mdb(mdb, fbx_filename.c_str())) {
		cout << "Cannot export to FBX\n";
		return 1;
	}
}
