#include <iostream>
#include <algorithm>
#include <experimental/filesystem>

#include "config.h"
#include "export_gr2.h"
#include "export_mdb.h"
#include "mdb_file.h"

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
	material->DiffuseFactor.Set(1.0f);
	material->Specular.Set(FbxDouble3(mdb_material.specular_color.x,
		mdb_material.specular_color.y,
		mdb_material.specular_color.z));
	// Transform from [0, 100] to [0, 0.5]
	material->SpecularFactor.Set(mdb_material.specular_value/200.0f);
	// Transform from [0, 2.5] to [0, 100]
	material->Shininess.Set(mdb_material.specular_power/2.5f*100.0f);

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
	export_map(export_info, material->EmissiveFactor,
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

FbxMesh* create_sphere(Export_info& export_info, const char* name, double radius)
{
	auto mesh = FbxMesh::Create(export_info.scene, name);

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

static void export_collision_sphere(Export_info& export_info,
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

	auto node = FbxNode::Create(export_info.scene, node_name.c_str());
	node->LclRotation.Set(FbxDouble3(-90, 0, 0));
	node->LclScaling.Set(FbxDouble3(100, 100, 100));

	if (fbx_bone) {
		auto m = fbx_bone->EvaluateGlobalTransform();
		node->LclTranslation.Set(m.GetT());
	}

	export_info.scene->GetRootNode()->AddChild(node);

	auto mesh = create_sphere(export_info, node_name.c_str(), cs.spheres[sphere_index].radius);
	node->SetNodeAttribute(mesh);
	
	node->AddMaterial(collision_sphere_material(export_info.scene));
}

static void export_collision_spheres(Export_info& export_info,
	const MDB_file::Collision_spheres& cs)
{
	Dependency* dep = nullptr;
	for (auto &d : export_info.dependencies) {
		if (d.second.fbx_bones.size() > 0) {
			dep = &d.second;
			break;
		}
	}

	if (!dep)
		return;

	for (uint32_t i = 0; i < cs.header.sphere_count; ++i)
		export_collision_sphere(export_info, cs, i, *dep);
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

static void export_skinning(Export_info& export_info,
	const MDB_file::Skin& skin, FbxMesh* mesh)
{
	for (auto &dep : export_info.dependencies) {
		if (dep.second.fbx_bones.size() > 0 && strcmpi(skin.header.skeleton_name, dep.second.fbx_bones[0]->GetName()) == 0) {
			export_skinning(export_info, skin, mesh, dep.second);
			return;
		}
	}
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

bool export_mdb(Export_info& export_info, const MDB_file& mdb)
{
	export_info.mdb = &mdb;

	extract_dependencies(export_info);
	create_walk_mesh_materials(export_info.scene);

	for (uint32_t i = 0; i < mdb.packet_count(); ++i)
		export_packet(export_info, mdb.packet(i));
	
	return true;
}
