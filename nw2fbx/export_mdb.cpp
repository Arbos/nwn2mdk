#include <iostream>
#include <algorithm>
#include <filesystem>

#include "config.h"
#include "export_gr2.h"
#include "export_info.h"
#include "export_mdb.h"
#include "mdb_file.h"

using namespace std;
using namespace std::filesystem;

// Round to 3 decimals.
static double round3(double x)
{
	return round(x * 1000.0) / 1000.0;
}

// Round to 3 decimals.
static FbxDouble3 round3(const FbxDouble3& v)
{
	return FbxDouble3(round3(v[0]), round3(v[1]), round3(v[2]));
}

static void export_diffuse_color(FbxSurfacePhong* material, FbxNode* node,
	const MDB_file::Material& mdb_material)
{
	FbxDouble3 diffuse_color(mdb_material.diffuse_color.x,
		mdb_material.diffuse_color.y,
		mdb_material.diffuse_color.z);
	material->Diffuse.Set(diffuse_color);
	material->DiffuseFactor.Set(1.0f);

	auto prop = FbxProperty::Create(node, FbxDouble3DT, "DIFFUSE_COLOR");
	prop.Set(round3(diffuse_color));
	prop.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
}

static void export_specular_color(FbxSurfacePhong* material, FbxNode* node,
	const MDB_file::Material& mdb_material)
{
	FbxDouble3 specular_color(mdb_material.specular_color.x,
		mdb_material.specular_color.y,
		mdb_material.specular_color.z);
	material->Specular.Set(specular_color);

	auto prop = FbxProperty::Create(node, FbxDouble3DT, "SPECULAR_COLOR");
	prop.Set(round3(specular_color));
	prop.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
}

static void export_specular_level(FbxSurfacePhong* material, FbxNode* node,
	const MDB_file::Material& mdb_material)
{
	material->SpecularFactor.Set(mdb_material.specular_level);

	auto prop = FbxProperty::Create(node, FbxFloatDT, "SPECULAR_LEVEL");
	prop.Set(mdb_material.specular_level);
	prop.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
}

static void export_specular_power(FbxSurfacePhong* material, FbxNode* node,
	const MDB_file::Material& mdb_material)
{
	material->Shininess.Set(mdb_material.specular_power);

	auto prop = FbxProperty::Create(node, FbxFloatDT, "GLOSSINESS");
	prop.Set(mdb_material.specular_power);
	prop.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
}

static FbxSurfacePhong *create_material(FbxScene* scene, FbxNode* node,
	const MDB_file::Material& mdb_material,
	const char* name)
{
	FbxSurfacePhong* material = FbxSurfacePhong::Create(scene, name);
	export_diffuse_color(material, node, mdb_material);
	export_specular_color(material, node, mdb_material);
	export_specular_level(material, node, mdb_material);
	export_specular_power(material, node, mdb_material);

	return material;
}

static void create_user_property(FbxNode* node,
	const MDB_file::Material& mdb_material, MDB_file::Material_flags flag,
	const char* flag_name)
{
	auto prop = FbxProperty::Create(node, FbxFloatDT, flag_name);
	prop.Set(mdb_material.flags & flag ? 1.0f : 0.0f);
	prop.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
}

static void create_user_properties(FbxNode* node,
	const MDB_file::Material& mdb_material)
{
	create_user_property(node, mdb_material, MDB_file::ALPHA_TEST, "TRANSPARENCY_MASK");
	create_user_property(node, mdb_material, MDB_file::ENVIRONMENT_MAPPING, "ENVIRONMENT_MAP");
	create_user_property(node, mdb_material, MDB_file::CUTSCENE_MESH, "HEAD");
	create_user_property(node, mdb_material, MDB_file::GLOW, "GLOW");
	create_user_property(node, mdb_material, MDB_file::CAST_NO_SHADOWS, "DONT_CAST_SHADOWS");
	create_user_property(node, mdb_material, MDB_file::PROJECTED_TEXTURES, "PROJECTED_TEXTURES");
}

static FbxFileTexture* create_texture(FbxScene* scene, const char* name)
{
	FbxFileTexture* texture = FbxFileTexture::Create(scene, name);
	
	path p = name;
	p.concat(".tga");
	if (!exists(p))
		p.replace_extension(".dds");

	// Set texture properties.
	texture->SetFileName(p.string().c_str());
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

static void export_maps(Export_info& export_info, FbxNode* node, FbxSurfacePhong* material,
	const MDB_file::Material& mdb_material)
{
	export_map(export_info, material->Diffuse,
		string(mdb_material.diffuse_map_name, 32).c_str());
	export_map(export_info, material->NormalMap,
		string(mdb_material.normal_map_name, 32).c_str());
	export_map(export_info, material->DisplacementColor,
		string(mdb_material.tint_map_name, 32).c_str());
	export_map(export_info, material->Emissive,
		string(mdb_material.glow_map_name, 32).c_str());
	
	// Export tint map as a node property
	auto prop_tint_map = FbxProperty::Create(node, FbxStringDT, "TINT_MAP");
	prop_tint_map.Set<FbxString>(string(mdb_material.tint_map_name, 32).c_str());	
	prop_tint_map.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
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
		FbxVector4 n(-verts[i].binormal.x, -verts[i].binormal.y,
			-verts[i].binormal.z);
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
	cout << "Exporting COL2: " << name.c_str() << endl;

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
	cout << "Exporting COL3: " << name.c_str() << endl;

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

FbxMesh* create_sphere(FbxScene* scene, const char* name, double radius)
{
	auto mesh = FbxMesh::Create(scene, name);

	int rings = 16;
	int segments = 32;
	int vertex_count = 2 + (rings - 1)*segments;

	mesh->InitControlPoints(vertex_count);

	// North pole vertex
	mesh->GetControlPoints()[0] = FbxVector4(0, 0, radius);

	// Middle rings vertices
	for (int vertex_index = 1, i = 1; i < rings; ++i) {
		double inclination = M_PI / rings*i;
		for (int j = 0; j < segments; ++j) {
			double azimuth = 2 * M_PI / segments*j;
			double x = radius * sin(inclination) * cos(azimuth);
			double y = radius * sin(inclination) * sin(azimuth);
			double z = radius * cos(inclination);
			mesh->GetControlPoints()[vertex_index] = FbxVector4(x, y, z);
			++vertex_index;
		}
	}

	// South pole vertex
	mesh->GetControlPoints()[vertex_count - 1] = FbxVector4(0, 0, -radius);

	// Top ring faces
	for (int i = 1; i <= segments; ++i) {
		mesh->BeginPolygon(-1, -1, -1, false);
		mesh->AddPolygon(0);
		mesh->AddPolygon(i);
		mesh->AddPolygon(i % segments + 1);
		mesh->EndPolygon();
	}

	// Middle rings faces
	for (int r = 1; r <= rings - 2; ++r) {
		int vertex_base = 1 + (r - 1)*segments;

		for (int i = 0; i < segments; ++i) {
			mesh->BeginPolygon(-1, -1, -1, false);
			mesh->AddPolygon(vertex_base + i);
			mesh->AddPolygon(vertex_base + segments + i);
			mesh->AddPolygon(vertex_base + segments + (i + 1) % segments);
			mesh->AddPolygon(vertex_base + (i + 1) % segments);
			mesh->EndPolygon();
		}		
	}

	// Bottom ring faces
	for (int i = 0; i < segments; ++i) {
		mesh->BeginPolygon(-1, -1, -1, false);
		mesh->AddPolygon(vertex_count - 1 - segments + i);
		mesh->AddPolygon(vertex_count - 1);
		mesh->AddPolygon(vertex_count - 1 - segments + (i + 1) % segments);
		mesh->EndPolygon();
	}

	return mesh;
}

static FbxSurfacePhong *collision_sphere_material(FbxScene* scene)
{
	static FbxSurfacePhong* material = nullptr;
	if (!material) {
		material = FbxSurfacePhong::Create(scene, "COLS");
		material->Diffuse.Set(FbxDouble3(0.5, 0.5, 0.5));
		material->DiffuseFactor.Set(1.0);
		material->TransparentColor.Set(FbxDouble3(1.0, 1.0, 1.0));
		material->TransparencyFactor.Set(1.0);
		material->Specular.Set(FbxDouble3(0, 0, 0));
		material->SpecularFactor.Set(0);
		material->Shininess.Set(2);
	}

	return material;
}

static void export_collision_sphere(FbxNode *parent_node,
	const MDB_file::Collision_spheres& cs, uint32_t sphere_index,
	Dependency& dep)
{
	FbxNode* fbx_bone = nullptr;
	if (cs.spheres[sphere_index].bone_index < dep.fbx_bones.size())
		fbx_bone = dep.fbx_bones[cs.spheres[sphere_index].bone_index];
	
	string node_name = "COLS_";
	if (fbx_bone)
		node_name += fbx_bone->GetName();
	else
		node_name += to_string(cs.spheres[sphere_index].bone_index);

	auto node = FbxNode::Create(parent_node->GetScene(), node_name.c_str());
	node->LclRotation.Set(FbxDouble3(0, 0, 0));
	node->LclScaling.Set(FbxDouble3(1, 1, 1));

	if (fbx_bone) {
		auto m = fbx_bone->EvaluateGlobalTransform();
		auto t = m.GetT()/100.0;
		swap(t[1], t[2]);
		t[1] = -t[1];
		node->LclTranslation.Set(t);
	}

	parent_node->AddChild(node);

	auto mesh = create_sphere(node->GetScene(), node_name.c_str(),
	                          cs.spheres[sphere_index].radius);
	node->SetNodeAttribute(mesh);
	
	node->AddMaterial(collision_sphere_material(node->GetScene()));
}

static void export_collision_spheres(Export_info& export_info,
	const MDB_file::Collision_spheres& cs)
{
	Dependency* dep = nullptr;
	for (auto &d : export_info.dependencies) {
		if (d.second.fbx_bones.size() > 0 && strcmpi(export_info.mdb_skeleton_name.c_str(), d.second.fbx_bones[0]->GetName()) == 0) {
			dep = &d.second;
			break;
		}
	}

	if (!dep)
		return;

	auto cols_node = FbxNode::Create(export_info.scene, "COLS");
	cols_node->LclRotation.Set(FbxDouble3(-90, 0, 0));
	cols_node->LclScaling.Set(FbxDouble3(100, 100, 100));
	export_info.scene->GetRootNode()->AddChild(cols_node);

	for (uint32_t i = 0; i < cs.header.sphere_count; ++i)
		export_collision_sphere(cols_node, cs, i, *dep);
}

static FbxDouble3 orientation_to_euler(const float orientation[3][3])
{
	FbxMatrix m(orientation[1][0],
		orientation[1][1],
		orientation[1][2],
		0,
		orientation[0][0],
		orientation[0][1],
		orientation[0][2],
		0,
		orientation[2][0],
		orientation[2][1],
		orientation[2][2],
		0,
		0, 0, 0, 1);
	FbxVector4 translation;
	FbxVector4 rotation;
	FbxVector4 shearing;
	FbxVector4 scaling;
	double sign;
	m.GetElements(translation, rotation, shearing, scaling, sign);
	return FbxDouble3(rotation[0] - 90, rotation[2], -rotation[1]);	
}

static void create_user_property(FbxNode* node,
	const MDB_file::Hair& hair, MDB_file::Hair_shortening_behavior hsb,
	const char* property_name)
{
	auto prop = FbxProperty::Create(node, FbxFloatDT, property_name);
	prop.Set(hair.header.shortening_behavior == hsb ? 1.0f : 0.0f);
	prop.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
}

static void create_user_properties(FbxNode* node, const MDB_file::Hair& hair)
{
	create_user_property(node, hair, MDB_file::HSB_LOW, "HSB_LOW");
	create_user_property(node, hair, MDB_file::HSB_SHORT, "HSB_SHORT");
	create_user_property(node, hair, MDB_file::HSB_PONYTAIL, "HSB_PONYTAIL");
}

static void export_hair(Export_info& export_info, const MDB_file::Hair& hair)
{
	string name(hair.header.name, 32);
	cout << "Exporting HAIR: " << name.c_str() << endl;

	auto node = FbxNode::Create(export_info.scene, name.c_str());
	node->LclTranslation.Set(FbxDouble3(hair.header.position.x * 100, hair.header.position.z * 100, -hair.header.position.y * 100));
	node->LclRotation.Set(orientation_to_euler(hair.header.orientation));
	node->LclScaling.Set(FbxDouble3(100, 100, 100));
	export_info.scene->GetRootNode()->AddChild(node);

	create_user_properties(node, hair);
}

static void create_user_property(FbxNode* node,
	const MDB_file::Helm& helm, MDB_file::Helm_hair_hiding_behavior hhhb,
	const char* property_name)
{
	auto prop = FbxProperty::Create(node, FbxFloatDT, property_name);
	prop.Set(helm.header.hiding_behavior == hhhb ? 1.0f : 0.0f);
	prop.ModifyFlag(FbxPropertyFlags::eUserDefined, true);
}

static void create_user_properties(FbxNode* node, const MDB_file::Helm& helm)
{
	create_user_property(node, helm, MDB_file::HHHB_NONE_HIDDEN, "HHHB_NONE_HIDDEN");
	create_user_property(node, helm, MDB_file::HHHB_HAIR_HIDDEN, "HHHB_HAIR_HIDDEN");
	create_user_property(node, helm, MDB_file::HHHB_PARTIAL_HAIR, "HHHB_PARTIAL_HAIR");
	create_user_property(node, helm, MDB_file::HHHB_HEAD_HIDDEN, "HHHB_HEAD_HIDDEN");
}

static void export_helm(Export_info& export_info, const MDB_file::Helm& helm)
{
	string name(helm.header.name, 32);
	cout << "Exporting HELM: " << name.c_str() << endl;

	auto node = FbxNode::Create(export_info.scene, name.c_str());
	node->LclTranslation.Set(FbxDouble3(helm.header.position.x * 100, helm.header.position.z * 100, -helm.header.position.y * 100));
	node->LclRotation.Set(orientation_to_euler(helm.header.orientation));
	node->LclScaling.Set(FbxDouble3(100, 100, 100));
	export_info.scene->GetRootNode()->AddChild(node);

	create_user_properties(node, helm);
}

static void export_hook(Export_info& export_info, const MDB_file::Hook& hook)
{	
	string name(hook.header.name, 32);
	cout << "Exporting HOOK: " << name.c_str() << endl;

	auto node = FbxNode::Create(export_info.scene, name.c_str());	
	node->LclTranslation.Set(FbxDouble3(hook.header.position.x * 100, hook.header.position.z * 100, -hook.header.position.y * 100));	
	node->LclRotation.Set(orientation_to_euler(hook.header.orientation));
	node->LclScaling.Set(FbxDouble3(100, 100, 100));
	export_info.scene->GetRootNode()->AddChild(node);	
}

static void export_rigid_mesh(Export_info& export_info,
	const MDB_file::Rigid_mesh& rm)
{
	string name(rm.header.name, 32);
	cout << "Exporting RIGD: " << name.c_str() << endl;

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
	export_maps(export_info, node, material, rm.header.material);
	node->AddMaterial(material);

	create_user_properties(node, rm.header.material);
}

static std::vector<FbxCluster*> create_clusters(Export_info& export_info,
	const MDB_file::Skin& skin, Dependency& dep)
{
	std::vector<FbxCluster*> clusters(dep.fbx_body_bones.size());
	for (unsigned i = 0; i < clusters.size(); ++i) {
		auto fbx_bone = (skin.header.material.flags & MDB_file::CUTSCENE_MESH) && i < dep.fbx_face_bones.size() ?
			dep.fbx_face_bones[i] : dep.fbx_body_bones[i];
		clusters[i] = FbxCluster::Create(export_info.scene, "");
		clusters[i]->SetLink(fbx_bone);
		clusters[i]->SetLinkMode(FbxCluster::eNormalize);
		FbxAMatrix m;
		m.SetT(FbxVector4(0, 0, 0));
		m.SetR(FbxVector4(-90, 0, 0));
		m.SetS(FbxVector4(100, 100, 100));
		clusters[i]->SetTransformMatrix(m);
		clusters[i]->SetTransformLinkMatrix(fbx_bone->EvaluateGlobalTransform());
	}

	return clusters;
}

static void fill_vertex_clusters(const MDB_file::Skin& skin, unsigned vertex_index, std::vector<FbxCluster*>& clusters)
{
	auto& v = skin.verts[vertex_index];
	assert(v.bone_count >= 0 && v.bone_count <= 4);

	for (unsigned j = 0; j < 4; ++j) {
		if (v.bone_weights[j] > 0) {
			auto bone_index = v.bone_indices[j];
			if (bone_index < clusters.size())
				clusters[bone_index]->AddControlPointIndex(vertex_index, v.bone_weights[j]);
			else
				cout << "  WARNING: bone index out of bounds (" << unsigned(bone_index) << " >= " << clusters.size() << ")\n";				
		}
	}
}

static void fill_clusters(const MDB_file::Skin& skin, std::vector<FbxCluster*>& clusters)
{
	for (unsigned i = 0; i < skin.verts.size(); ++i)		
		fill_vertex_clusters(skin, i, clusters);
}

static void add_clusters(FbxSkin* fbx_skin, const std::vector<FbxCluster*>& clusters)
{
	for (unsigned i = 0; i < clusters.size(); ++i)
		fbx_skin->AddCluster(clusters[i]);
}

static void export_skinning(Export_info& export_info,
	const MDB_file::Skin& skin, FbxMesh* mesh,
	Dependency& dep)
{
	FbxSkin *fbx_skin = FbxSkin::Create(export_info.scene, "");
	fbx_skin->SetSkinningType(FbxSkin::eRigid);

	auto clusters = create_clusters(export_info, skin, dep);
	fill_clusters(skin, clusters);
	add_clusters(fbx_skin, clusters);

	mesh->AddDeformer(fbx_skin);
}

static Dependency* find_skeleton_dependency(Export_info& export_info,
                                            const char* skeleton_name)
{
	for (auto &dep : export_info.dependencies) {
		if (dep.second.exported && dep.second.fbx_bones.size() > 0 &&
		    strcmpi(skeleton_name,
		            dep.second.fbx_bones[0]->GetName()) == 0) {
			return &dep.second;
		}
	}

	return nullptr;
}

static const char* guess_cloak_skeleton_name(const char* packet_name)
{
	struct Skeletons {
		const char* prefix;
		const char* name;
	} skels[] = {"P_DDM_", "P_DDMcapewing_skel", //
	             "P_DDF_", "P_DDFcapewing_skel", //
	             "P_EEM_", "P_HHMcapewing_skel", //
	             "P_EEF_", "P_HHFcapewing_skel", //
	             "P_GGM_", "P_GGMcapewing_skel", //
	             "P_GGF_", "P_GGFcapewing_skel", //
	             "P_HHM_", "P_HHMcapewing_skel", //
	             "P_HHF_", "P_HHFcapewing_skel", //
	             "P_OOM_", "P_OOMcapewing_skel", //
	             "P_OOF_", "P_OOFcapewing_skel"};

	for (size_t i = 0; i < size(skels); ++i) {
		if (strncmpi(packet_name, skels[i].prefix, 6) == 0)
			return skels[i].name;
	}

	return nullptr;
}

static const char* guess_tail_skeleton_name(const char* packet_name)
{
	struct Skeletons {
		const char* prefix;
		const char* name;
	} skels[] = {"P_DDM_", "P_DDMtail_skel", //
	             "P_DDF_", "P_DDFtail_skel", //
	             "P_EEM_", "P_HHMtail_skel", //
	             "P_EEF_", "P_HHFtail_skel", //
	             "P_GGM_", "P_GGMtail_skel", //
	             "P_GGF_", "P_GGFtail_skel", //
	             "P_HHM_", "P_HHMtail_skel", //
	             "P_HHF_", "P_HHFtail_skel", //
	             "P_HTM_", "P_HHMtail_skel", //
	             "P_HTF_", "P_HHFtail_skel", //
	             "P_OOM_", "P_OOMtail_skel", //
	             "P_OOF_", "P_OOFtail_skel"};

	for (size_t i = 0; i < size(skels); ++i) {
		if (strncmpi(packet_name, skels[i].prefix, 6) == 0)
			return skels[i].name;
	}

	return nullptr;
}

static const char* guess_body_skeleton_name(const char* packet_name)
{
	struct Skeletons {
		const char* prefix;
		const char* name;
	} skels[] = {"P_AAM_", "P_HHM_skel", // Halfling
	             "P_AAF_", "P_HHF_skel", //
	             "P_ASM_", "P_HHM_skel", // Halfling, Strongheart
	             "P_ASF_", "P_HHF_skel", //
	             "P_DDM_", "P_DDM_skel", // Dwarf
	             "P_DDF_", "P_DDF_skel", //
	             "P_DGM_", "P_DDM_skel", // Dwarf, Gold
	             "P_DGF_", "P_DDF_skel", //
	             "P_DUM_", "P_DDM_skel", // Dwarf, Duergar
	             "P_DUF_", "P_DDF_skel", //
	             "P_EDM_", "P_HHM_skel", // Elf, Drow
	             "P_EDF_", "P_HHF_skel", //
	             "P_EEM_", "P_HHM_skel", // Elf
	             "P_EEF_", "P_HHF_skel", //
	             "P_EHM_", "P_HHM_skel", // Half-elf
	             "P_EHF_", "P_HHF_skel", //
	             "P_ELM_", "P_HHM_skel", // Elf, Wild
	             "P_ELF_", "P_HHF_skel", //
	             "P_ERM_", "P_HHM_skel", // Half-drow
	             "P_ERF_", "P_HHF_skel", //
	             "P_ESM_", "P_HHM_skel", // Elf, Sun
	             "P_ESF_", "P_HHF_skel", //
	             "P_EWM_", "P_HHM_skel", // Elf, Woord
	             "P_EWF_", "P_HHF_skel", //
	             "P_GGM_", "P_GGM_skel", // Gnome
	             "P_GGF_", "P_GGF_skel", //
	             "P_GSM_", "P_GGM_skel", // Gnome, Svirfneblin
	             "P_GSF_", "P_GGF_skel", //
	             "P_HAM_", "P_HHM_skel", // Assimar
	             "P_HAF_", "P_HHF_skel", //
	             "P_HEM_", "P_HHM_skel", // Earth Genasi
	             "P_HEF_", "P_HHF_skel", //
	             "P_HFM_", "P_HHM_skel", // Fire Genasi
	             "P_HFF_", "P_HHF_skel", //
	             "P_HHM_", "P_HHM_skel", // Human
	             "P_HHF_", "P_HHF_skel", //
	             "P_HIM_", "P_HHM_skel", // Air Genasi
	             "P_HIF_", "P_HHF_skel", //
	             "P_HPM_", "P_HHM_skel", // Yuanti Pureblood
	             "P_HPF_", "P_HHF_skel", //
	             "P_HTM_", "P_HHM_skel", // Tiefling
	             "P_HTF_", "P_HHF_skel", //
	             "P_HWM_", "P_HHM_skel", // Water Genasi
	             "P_HWF_", "P_HHF_skel", //
	             "P_OGM_", "P_OOM_skel", // Gray Orc
	             "P_OGF_", "P_OOF_skel", //
	             "P_OOM_", "P_OOM_skel", // Half-orc
	             "P_OOF_", "P_OOF_skel"};

	for (size_t i = 0; i < size(skels); ++i) {
		if (strncmpi(packet_name, skels[i].prefix, 6) == 0)
			return skels[i].name;
	}

	return nullptr;
}

static const char* guess_skeleton_name(const char* packet_name)
{
	string n = packet_name;
	transform(n.begin(), n.end(), n.begin(), ::toupper);

	if (strstr(n.c_str(), "_CLOAK") || strstr(n.c_str(), "_WINGS"))
		return guess_cloak_skeleton_name(n.c_str());
	else if (strstr(n.c_str(), "_TAIL"))
		return guess_tail_skeleton_name(n.c_str());
	else // Body, head, helm, gloves, belt, boots
		return guess_body_skeleton_name(n.c_str());
}

static void export_skinning(Export_info& export_info,
	const MDB_file::Skin& skin, FbxMesh* mesh)
{
	// First try to find the skeleton within the exported ones.
	Dependency* dep =
	    find_skeleton_dependency(export_info, skin.header.skeleton_name);

	if (!dep) {
		// Try to export the skeleton.

		cout << "  Skeleton \"" << skin.header.skeleton_name
		     << "\" for this skin not found in the input files. Trying "
		        "to find it on drive.\n";

		string filename = string(skin.header.skeleton_name) + ".gr2";
		export_gr2(export_info, filename.c_str());
		dep = find_skeleton_dependency(export_info,
		                               skin.header.skeleton_name);
	}

	if (!dep) {
		// Maybe skeleton name is wrong. Try with a guessed name.

		const char* sn = guess_skeleton_name(skin.header.name);

		if (sn) {
			cout << "  WARNING: Skeleton \""
			     << skin.header.skeleton_name
			     << "\" for this skin not found. Maybe skeleton "
			        "name is wrong. Trying with heuristic name \""
			     << sn << "\".\n";

			dep = find_skeleton_dependency(export_info, sn);

			if (!dep) {
				// Try to export the skeleton.
				string filename = string(sn) + ".gr2";
				export_gr2(export_info, filename.c_str());
				dep = find_skeleton_dependency(export_info, sn);
			}
		}
	}

	if (dep)
		export_skinning(export_info, skin, mesh, *dep);
	else
		cout << "  WARNING: Could not find skeleton for this skin.\n";
}

static void export_skin(Export_info& export_info, const MDB_file::Skin& skin)
{
	string name(skin.header.name, 32);
	cout << "Exporting SKIN: " << name.c_str() << endl;

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
	export_maps(export_info, node, material, skin.header.material);
	node->AddMaterial(material);

	create_user_properties(node, skin.header.material);

	export_skinning(export_info, skin, mesh);	
}

static void add_walk_mesh_materials(FbxScene* scene, FbxNode* node)
{
	for (unsigned i = 0; i < size(MDB_file::walk_mesh_materials); ++i)
		node->AddMaterial(scene->GetMaterial((char*)MDB_file::walk_mesh_materials[i].name));
}

static void export_vertices(FbxMesh* mesh, const MDB_file::Walk_mesh& wm)
{
	// Create control points.
	mesh->InitControlPoints(wm.verts.size());
	for (uint32_t i = 0; i < wm.verts.size(); ++i) {
		// Some walk mesh vertices have z=-1000000. We clamp z to -20.
		FbxVector4 v(wm.verts[i].position.x, wm.verts[i].position.y,
			max(wm.verts[i].position.z, -20.0f));
		mesh->GetControlPoints()[i] = v;
	}
}

// Returns the FBX material index for a walk mesh face.
static int material_index(const MDB_file::Walk_mesh_face& f)
{
	for (unsigned i = 0; i < size(MDB_file::walk_mesh_materials); ++i)
		if (f.flags[0] == MDB_file::walk_mesh_materials[i].flags)
			return i;

	return 0;
}

static void export_faces(FbxMesh* mesh, const MDB_file::Walk_mesh& wm)
{
	// Set material mapping.
	auto material_element = mesh->CreateElementMaterial();
	material_element->SetMappingMode(FbxGeometryElement::eByPolygon);
	material_element->SetReferenceMode(FbxGeometryElement::eIndexToDirect);

	for (auto& face : wm.faces) {			
		mesh->BeginPolygon(material_index(face), -1, -1, false);
		mesh->AddPolygon(face.vertex_indices[0]);
		mesh->AddPolygon(face.vertex_indices[1]);
		mesh->AddPolygon(face.vertex_indices[2]);
		mesh->EndPolygon();
	}
}

static void export_walk_mesh(Export_info& export_info, const MDB_file::Walk_mesh& wm)
{
	string name(wm.header.name, 32);
	cout << "Exporting WALK: " << name.c_str() << endl;	

	auto mesh = FbxMesh::Create(export_info.scene, name.c_str());
	auto node = create_node(export_info.scene, mesh, name.c_str());
	export_info.scene->GetRootNode()->AddChild(node);

	add_walk_mesh_materials(export_info.scene, node);	
	export_vertices(mesh, wm);
	export_faces(mesh, wm);	
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
	case MDB_file::COLS:
		export_collision_spheres(export_info,
			*static_cast<const MDB_file::Collision_spheres*>(packet));
		break;
	case MDB_file::HAIR:
		export_hair(export_info,
			*static_cast<const MDB_file::Hair*>(packet));
		break;
	case MDB_file::HELM:
		export_helm(export_info,
			*static_cast<const MDB_file::Helm*>(packet));
		break;
	case MDB_file::HOOK:
		export_hook(export_info,
			*static_cast<const MDB_file::Hook*>(packet));
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

static bool exists_texture(const char* name)
{
	if (exists(path(name).concat(".dds")))
		return true;
	else if (exists(path(name).concat(".tga")))
		return true;

	return false;
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
		p = p.filename();
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
	if (!diffuse_map.empty() && !exists_texture(diffuse_map.c_str()))
		extract_dependency(export_info, diffuse_map.c_str(), export_info.materials);

	string normal_map = string(material.normal_map_name, 32).c_str();
	if (!normal_map.empty() && !exists_texture(normal_map.c_str()))
		extract_dependency(export_info, normal_map.c_str(), export_info.materials);

	string tint_map = string(material.tint_map_name, 32).c_str();
	if (!tint_map.empty() && !exists_texture(tint_map.c_str()))
		extract_dependency(export_info, tint_map.c_str(), export_info.materials);

	string glow_map = string(material.glow_map_name, 32).c_str();
	if (!glow_map.empty() && !exists_texture(glow_map.c_str()))
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
	for (uint32_t i = 0; i < export_info.mdb->packet_count(); ++i)
		extract_textures(export_info, export_info.mdb->packet(i));
}

static void extract_dependencies(Export_info& export_info)
{
	extract_textures(export_info);
}

void create_walk_mesh_materials(FbxScene* scene)
{
	for (unsigned i = 0; i < size(MDB_file::walk_mesh_materials); ++i) {
		auto material = FbxSurfacePhong::Create(scene, MDB_file::walk_mesh_materials[i].name);
		auto color = MDB_file::walk_mesh_materials[i].color;
		material->Diffuse.Set(FbxDouble3(color.x, color.y, color.z));
		material->DiffuseFactor.Set(1.0f);
		material->TransparentColor.Set(FbxDouble3(1, 1, 1));
		material->TransparencyFactor.Set(0.5);
	}
}

static std::string find_skeleton_name(const MDB_file& mdb)
{
	for (uint32_t i = 0; i < mdb.packet_count(); ++i) {
		auto packet = mdb.packet(i);

		if (packet && packet->type == MDB_file::SKIN)
			return static_cast<const MDB_file::Skin*>(packet)->header.skeleton_name;
	}

	return "";
}

bool export_mdb(Export_info& export_info, const MDB_file& mdb)
{
	export_info.mdb = &mdb;

	extract_dependencies(export_info);
	create_walk_mesh_materials(export_info.scene);
	export_info.mdb_skeleton_name = find_skeleton_name(mdb);

	for (uint32_t i = 0; i < mdb.packet_count(); ++i)
		export_packet(export_info, mdb.packet(i));
	
	return true;
}
