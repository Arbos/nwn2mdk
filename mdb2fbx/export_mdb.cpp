#include <iostream>
#include <algorithm>
#include <map>
#include <experimental/filesystem>

#include "archive_container.h"
#include "config.h"
#include "export_gr2.h"
#include "export_mdb.h"
#include "fbxsdk.h"
#include "mdb_file.h"

struct Dependency {
	bool extracted;
	std::string extracted_path;
	bool exported;
	std::vector<FbxNode*> fbx_bones;
};

struct Export_info {
	const Config &config;
	Archive_container materials;
	Archive_container lod_merge;
	const MDB_file &mdb;
	FbxScene *scene;
	std::map<std::string, Dependency> dependencies;
};

using namespace std;
using namespace std::experimental::filesystem::v1;

static FbxSurfacePhong *create_material(FbxScene* scene, FbxNode* node,
	const MDB_file::Material& mdb_material,
	const char* name)
{
	FbxSurfacePhong* material = FbxSurfacePhong::Create(scene, name);
	material->Diffuse.Set(FbxDouble3(mdb_material.diffuse_color.x,
		mdb_material.diffuse_color.y,
		mdb_material.diffuse_color.z));
	material->Specular.Set(FbxDouble3(mdb_material.specular_color.x,
		mdb_material.specular_color.y,
		mdb_material.specular_color.z));

	return material;
}

static FbxFileTexture* create_texture(FbxScene* scene, const char* name)
{
	FbxFileTexture* texture = FbxFileTexture::Create(scene, name);

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
static void export_map(Export_info& export_info, FbxPropertyT<T>& property,
	const char* map_name)
{
	if (strlen(map_name) > 0)
		property.ConnectSrcObject(create_texture(export_info.scene, map_name));
}

static void export_maps(Export_info& export_info, FbxSurfacePhong* material,
	const MDB_file::Material& mdb_material)
{
	export_map(export_info, material->Diffuse,
		string(mdb_material.diffuse_map_name, 32).c_str());
	export_map(export_info, material->NormalMap,
		string(mdb_material.normal_map_name, 32).c_str());
	export_map(export_info, material->DisplacementColor,
		string(mdb_material.tint_map_name, 32).c_str());
	export_map(export_info, material->EmissiveFactor,
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

static void export_collision_mesh_2(Export_info& export_info,
	const MDB_file::Collision_mesh& cm)
{	
	string name(cm.header.name, 32);
	cout << "  Exporting: " << name.c_str() << endl;

	auto mesh = FbxMesh::Create(export_info.scene, name.c_str());

	export_vertices(mesh, cm.verts);
	export_normals(mesh, cm.verts);
	export_uv(mesh, cm.verts);
	export_faces(mesh, cm.faces);

	auto node = create_node(export_info.scene, mesh, name.c_str());
	export_info.scene->GetRootNode()->AddChild(node);

	auto material =
		create_material(export_info.scene, node, cm.header.material, name.c_str());
	material->Diffuse.Set(FbxDouble3(0.9, 0.4, 0.09));
	material->DiffuseFactor.Set(0.8);
	material->TransparentColor.Set(FbxDouble3(0.9, 0.4, 0.09));
	material->TransparencyFactor.Set(0.5);
	material->Specular.Set(FbxDouble3(0, 0, 0));
	material->SpecularFactor.Set(0);
	material->Shininess.Set(2);
	node->AddMaterial(material);
}

static void export_collision_mesh_3(Export_info& export_info,
	const MDB_file::Collision_mesh& cm)
{
	string name(cm.header.name, 32);
	cout << "  Exporting: " << name.c_str() << endl;

	auto mesh = FbxMesh::Create(export_info.scene, name.c_str());

	export_vertices(mesh, cm.verts);
	export_normals(mesh, cm.verts);
	export_uv(mesh, cm.verts);
	export_faces(mesh, cm.faces);

	auto node = create_node(export_info.scene, mesh, name.c_str());
	export_info.scene->GetRootNode()->AddChild(node);

	auto material =
		create_material(export_info.scene, node, cm.header.material, name.c_str());
	material->Diffuse.Set(FbxDouble3(0.76, 0.11, 0.09));
	material->DiffuseFactor.Set(0.8);
	material->TransparentColor.Set(FbxDouble3(0.76, 0.11, 0.09));
	material->TransparencyFactor.Set(0.5);
	material->Specular.Set(FbxDouble3(0, 0, 0));
	material->SpecularFactor.Set(0);
	material->Shininess.Set(2);
	node->AddMaterial(material);
}

static void export_rigid_mesh(Export_info& export_info,
	const MDB_file::Rigid_mesh& rm)
{
	string name(rm.header.name, 32);
	cout << "  Exporting: " << name.c_str() << endl;

	auto mesh = FbxMesh::Create(export_info.scene, name.c_str());

	export_vertices(mesh, rm.verts);
	export_normals(mesh, rm.verts);
	export_tangents(mesh, rm.verts);
	export_binormals(mesh, rm.verts);
	export_uv(mesh, rm.verts);
	export_faces(mesh, rm.faces);

	auto node = create_node(export_info.scene, mesh, name.c_str());
	export_info.scene->GetRootNode()->AddChild(node);

	auto material =
		create_material(export_info.scene, node, rm.header.material, name.c_str());
	export_maps(export_info, material, rm.header.material);
	node->AddMaterial(material);
}

static void export_skinning(Export_info& export_info,
	const MDB_file::Skin& skin, FbxMesh* mesh)
{
	auto dep = export_info.dependencies.find(skin.header.skeleton_name);
	if (dep == export_info.dependencies.end() || !dep->second.extracted)
		return;

	if (!dep->second.exported) {
		dep->second.exported = true;
		export_gr2(dep->second.extracted_path.c_str(), export_info.scene,
			dep->second.fbx_bones);
	}

	FbxSkin *fbx_skin = FbxSkin::Create(export_info.scene, "");
	fbx_skin->SetSkinningType(FbxSkin::eRigid);

	std::vector<FbxCluster*> clusters(dep->second.fbx_bones.size());
	for (unsigned i = 0; i < clusters.size(); ++i) {
		clusters[i] = FbxCluster::Create(export_info.scene, "");
		clusters[i]->SetLink(dep->second.fbx_bones[i]);
		clusters[i]->SetLinkMode(FbxCluster::eNormalize);
		FbxAMatrix m;
		m.SetT(FbxVector4(0, 0, 0));
		m.SetR(FbxVector4(-90, 0, 0));
		m.SetS(FbxVector4(100, 100, 100));
		clusters[i]->SetTransformMatrix(m);
		clusters[i]->SetTransformLinkMatrix(dep->second.fbx_bones[i]->EvaluateGlobalTransform());
	}

	for (unsigned i = 0; i < skin.verts.size(); ++i) {
		auto &v = skin.verts[i];
		assert(v.bone_count == 4);
		for (unsigned j = 0; j < 4; ++j) {
			if (v.bone_weights[j] > 0) {
				auto bone_index = v.bone_indices[j];
				assert(bone_index < clusters.size());
				clusters[bone_index]->AddControlPointIndex(i, v.bone_weights[j]);
			}
		}
	}

	for (unsigned i = 0; i < clusters.size(); ++i)
		fbx_skin->AddCluster(clusters[i]);

	mesh->AddDeformer(fbx_skin);
}

static void export_skin(Export_info& export_info, const MDB_file::Skin& skin)
{
	string name(skin.header.name, 32);
	cout << "  Exporting: " << name.c_str() << endl;

	auto mesh = FbxMesh::Create(export_info.scene, name.c_str());

	export_vertices(mesh, skin.verts);
	export_normals(mesh, skin.verts);
	export_tangents(mesh, skin.verts);
	export_binormals(mesh, skin.verts);
	export_uv(mesh, skin.verts);
	export_faces(mesh, skin.faces);

	auto node = create_node(export_info.scene, mesh, name.c_str());
	export_info.scene->GetRootNode()->AddChild(node);

	auto material =
		create_material(export_info.scene, node, skin.header.material, name.c_str());
	export_maps(export_info, material, skin.header.material);
	node->AddMaterial(material);

	export_skinning(export_info, skin, mesh);	
}

static void export_walk_mesh(Export_info& export_info, const MDB_file::Walk_mesh& wm)
{
	string name(wm.header.name, 32);
	cout << "  Exporting: " << name.c_str() << endl;

	auto mesh = FbxMesh::Create(export_info.scene, name.c_str());

	// Create control points.
	mesh->InitControlPoints(wm.verts.size());
	for (uint32_t i = 0; i < wm.verts.size(); ++i) {
		// Some walk mesh vertices have z=-1000000. We clamp z to -20.
		FbxVector4 v(wm.verts[i].position.x, wm.verts[i].position.y,
			max(wm.verts[i].position.z, -20.0f));
		mesh->GetControlPoints()[i] = v;
	}

	export_faces(mesh, wm.faces);

	auto node = create_node(export_info.scene, mesh, name.c_str());
	export_info.scene->GetRootNode()->AddChild(node);
}

static void export_packet(Export_info& export_info,
	const MDB_file::Packet* packet)
{
	if (!packet)
		return;

	switch (packet->type) {
	case MDB_file::COL2:
		export_collision_mesh_2(export_info,
			*static_cast<const MDB_file::Collision_mesh*>(packet));
		break;
	case MDB_file::COL3:
		export_collision_mesh_3(export_info,
			*static_cast<const MDB_file::Collision_mesh*>(packet));
		break;
	case MDB_file::HOOK:
		break;
	case MDB_file::RIGD:
		export_rigid_mesh(export_info,
			*static_cast<const MDB_file::Rigid_mesh*>(packet));
		break;
	case MDB_file::SKIN:
		export_skin(export_info,
			*static_cast<const MDB_file::Skin*>(packet));
		break;
	case MDB_file::WALK:
		export_walk_mesh(export_info,
			*static_cast<const MDB_file::Walk_mesh*>(packet));
		break;
	default:
		break;
	}
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

static void extract_dependency(Export_info& export_info, const char* str,
	const Archive_container& archives)
{
	if (export_info.dependencies.find(str) != export_info.dependencies.end())
		return;

	auto &dep = export_info.dependencies[str];
	dep.extracted = false;
	dep.exported = false;

	auto r = archives.find_file((string(str) + '.').c_str());
	if (r.matches > 0) {
		path p(archives.filename(r.archive_index, r.file_index));
		p = path("output") / p.filename();
		dep.extracted_path = p.string();

		cout << "Extracting: " << p.string() << endl;

		if (exists(p)) {
			dep.extracted = true;
			cout << "  Already exists in destination. Don't overwrite.\n";
			return;
		}

		if (!archives.extract_file(r.archive_index, r.file_index,
			p.string().c_str())) {
			cout << "Cannot extract " << str << endl;
		}

		dep.extracted = true;
	}
	else
		cout << str << " not found\n";
}

static void extract_textures(Export_info& export_info,
	const MDB_file::Material& material)
{
	string diffuse_map = string(material.diffuse_map_name, 32).c_str();
	if (!diffuse_map.empty())
		extract_dependency(export_info, diffuse_map.c_str(), export_info.materials);

	string normal_map = string(material.normal_map_name, 32).c_str();
	if (!normal_map.empty())
		extract_dependency(export_info, normal_map.c_str(), export_info.materials);

	string tint_map = string(material.tint_map_name, 32).c_str();
	if (!tint_map.empty())
		extract_dependency(export_info, tint_map.c_str(), export_info.materials);

	string glow_map = string(material.glow_map_name, 32).c_str();
	if (!glow_map.empty())
		extract_dependency(export_info, glow_map.c_str(), export_info.materials);
}

static void extract_textures(Export_info& export_info,
	const MDB_file::Rigid_mesh& rm)
{
	extract_textures(export_info, rm.header.material);
}

static void extract_textures(Export_info& export_info,
	const MDB_file::Skin& skin)
{
	extract_textures(export_info, skin.header.material);
}

static void extract_textures(Export_info& export_info,
	const MDB_file::Packet* packet)
{
	if (!packet)
		return;

	switch (packet->type) {
	case MDB_file::RIGD:
		extract_textures(export_info,
			*static_cast<const MDB_file::Rigid_mesh*>(packet));
		break;
	case MDB_file::SKIN:
		extract_textures(export_info,
			*static_cast<const MDB_file::Skin*>(packet));
		break;
	default:
		break;
	}
}

static void extract_textures(Export_info& export_info)
{	
	for (uint32_t i = 0; i < export_info.mdb.packet_count(); ++i)
		extract_textures(export_info, export_info.mdb.packet(i));
}

static void extract_skeleton(Export_info& export_info, const MDB_file::Skin& skin)
{
	string skeleton = string(skin.header.skeleton_name, 32).c_str();
	if (!skeleton.empty())
		extract_dependency(export_info, skeleton.c_str(), export_info.lod_merge);
}

static void extract_skeleton(Export_info& export_info,
	const MDB_file::Packet* packet)
{
	if (!packet)
		return;

	switch (packet->type) {
	case MDB_file::SKIN:
		extract_skeleton(export_info,
			*static_cast<const MDB_file::Skin*>(packet));
		break;
	default:
		break;
	}
}

static void extract_skeletons(Export_info& export_info)
{
	for (uint32_t i = 0; i < export_info.mdb.packet_count(); ++i)
		extract_skeleton(export_info, export_info.mdb.packet(i));
}

static void extract_dependencies(Export_info& export_info)
{
	extract_textures(export_info);
	extract_skeletons(export_info);
}

bool export_mdb(const MDB_file& mdb, const char* filename, const Config& config)
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

	Export_info export_info = { config,
		get_material_archives(config),
		get_lod_archives(config), mdb, scene };

	extract_dependencies(export_info);

	cout << "Converting: " << filename << " -> " << filename << endl;

	for (uint32_t i = 0; i < mdb.packet_count(); ++i)
		export_packet(export_info, mdb.packet(i));	

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