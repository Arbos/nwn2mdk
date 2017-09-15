#include <iostream>
#include <experimental/filesystem>
#include <set>
#include <list>
#include <assert.h>

#include "fbxsdk.h"
#include "gr2_file.h"
#include "mdb_file.h"
#include "string_collection.h"

const double time_step = 1 / 30.0;

static GR2_property_key CurveDataHeader_def[] = {
	{ GR2_type_uint8, (char*)"Format", nullptr, 0, 0, 0, 0, 0},
	{ GR2_type_uint8, (char*)"Degree", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key D3Constant32f_def[] = {
	{ GR2_type_inline, (char*)"CurveDataHeader_D3Constant32f", CurveDataHeader_def, 0, 0, 0, 0, 0 },
	{ GR2_type_int16, (char*)"Padding", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_real32, (char*)"Controls", nullptr, 3, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key DaIdentity_def[] = {
	{ GR2_type_inline, (char*)"CurveDataHeader_DaIdentity", CurveDataHeader_def, 0, 0, 0, 0, 0 },
	{ GR2_type_int16, (char*)"Dimension", nullptr, 0, 0, 0, 0, 0 },	
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Real32_def[] = {
	{ GR2_type_real32, (char*)"Real32", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key DaK32fC32f_def[] = {
	{ GR2_type_inline, (char*)"CurveDataHeader_DaK32fC32f", CurveDataHeader_def, 0, 0, 0, 0, 0 },
	{ GR2_type_int16, (char*)"Padding", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_pointer, (char*)"Knots", Real32_def, 0, 0, 0, 0, 0 },
	{ GR2_type_pointer, (char*)"Controls", Real32_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

using namespace std;
using namespace std::experimental::filesystem::v1;

bool operator==(const MDB_file::Collision_mesh_vertex& v1,
                const MDB_file::Collision_mesh_vertex& v2)
{
	return v1.position == v2.position && v1.normal == v2.normal &&
	       v1.uvw == v2.uvw;
}

bool operator==(const MDB_file::Rigid_mesh_vertex &v1, const MDB_file::Rigid_mesh_vertex &v2)
{
	return v1.position == v2.position && v1.normal == v2.normal &&
	       v1.tangent == v2.tangent && v1.binormal == v2.binormal &&
	       v1.uvw == v2.uvw;
}

bool operator==(const MDB_file::Skin_vertex &v1, const MDB_file::Skin_vertex &v2)
{
	return v1.position == v2.position && v1.normal == v2.normal &&
		v1.bone_weights[0] == v2.bone_weights[0] &&
		v1.bone_weights[1] == v2.bone_weights[1] &&
		v1.bone_weights[2] == v2.bone_weights[2] &&
		v1.bone_weights[3] == v2.bone_weights[3] &&
		v1.bone_indices[0] == v2.bone_indices[0] &&
		v1.bone_indices[1] == v2.bone_indices[1] &&
		v1.bone_indices[2] == v2.bone_indices[2] &&
		v1.bone_indices[3] == v2.bone_indices[3] &&
		v1.tangent == v2.tangent && v1.binormal == v2.binormal &&
		v1.uvw == v2.uvw &&
		v1.bone_count == v2.bone_count;
}

FbxQuaternion euler_to_quaternion(const FbxVector4 &v)
{
	double y = v[0];
	double p = v[1];
	double r = v[2];

	y = y * M_PI / 180;
	p = p * M_PI / 180;
	r = r * M_PI / 180;

	double sp = sin(p * 0.5);
	double cp = cos(p * 0.5);
	double sr = sin(y * 0.5);
	double cr = cos(y * 0.5);
	double sy = sin(r * 0.5);
	double cy = cos(r * 0.5);

	FbxQuaternion q;
	q[0] = cy*sr*cp - sy*cr*sp;
	q[1] = cy*cr*sp + sy*sr*cp;
	q[2] = sy*cr*cp - cy*sr*sp;
	q[3] = cy*cr*cp + sy*sr*sp;

	return q;
}

bool ends_with(const char *s1, const char *s2)
{
	auto l1 = strlen(s1);
	auto l2 = strlen(s2);

	return l1 >= l2 && strcmp(s1 + l1 - l2, s2) == 0;
}

const char* mapping_mode_str(FbxLayerElement::EMappingMode m)
{
	switch(m) {
	case FbxLayerElement::eNone:
		return "eNone";
	case FbxLayerElement::eByControlPoint:
		return "eByControlPoint";
	case FbxLayerElement::eByPolygonVertex:
		return "eByPolygonVertex";
	case FbxLayerElement::eByPolygon:
		return "eByPolygon";
	case FbxLayerElement::eByEdge:
		return "eByEdge";
	case FbxLayerElement::eAllSame:
		return "eAllSame";
	}

	return "UNKNOWN";
}

const char* reference_mode_str(FbxLayerElement::EReferenceMode m)
{
	switch(m) {
	case FbxLayerElement::eDirect:
		return "eDirect";
	case FbxLayerElement::eIndex:
		return "eIndex";
	case FbxLayerElement::eIndexToDirect:
		return "eIndexToDirect";
	}

	return "UNKNOWN";
}

FbxSkin *skin(FbxMesh *mesh)
{
	if (mesh->GetDeformerCount() == 0)
		return nullptr;

	auto deformer = mesh->GetDeformer(0);
	if (deformer->GetDeformerType() != FbxDeformer::eSkin)
		return nullptr;

	return static_cast<FbxSkin*>(deformer);
}

template <typename T>
void import_positions(FbxMesh* mesh, int polygon_index, T* poly_vertices)
{
	for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
		int index = mesh->GetPolygonVertex(polygon_index, i);
		FbxVector4 p = mesh->GetControlPointAt(index);
		poly_vertices[i].position.x = float(p[0]);
		poly_vertices[i].position.y = float(p[1]);
		poly_vertices[i].position.z = float(p[2]);
	}
}

template <typename T>
void import_normals(FbxMesh* mesh, int polygon_index, T* poly_vertices)
{
	for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
		FbxVector4 normal;
		mesh->GetPolygonVertexNormal(polygon_index, i, normal);
		poly_vertices[i].normal.x = float(normal[0]);
		poly_vertices[i].normal.y = float(normal[1]);
		poly_vertices[i].normal.z = float(normal[2]);
	}
}

template <typename T>
void import_tangents(FbxMesh* mesh, int polygon_index, T* poly_vertices)
{
	if(mesh->GetElementTangentCount() <= 0)
		return;

	auto *e = mesh->GetElementTangent(0);
	switch(e->GetMappingMode()) {
	case FbxGeometryElement::eByPolygonVertex:
		for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
			int index = mesh->GetPolygonVertexIndex(polygon_index) + i;
			FbxVector4 v = e->GetDirectArray().GetAt(index);
			poly_vertices[i].tangent.x = float(v[0]);
			poly_vertices[i].tangent.y = float(v[1]);
			poly_vertices[i].tangent.z = float(v[2]);
		}
		break;
	case FbxGeometryElement::eByControlPoint:
		for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
			int index = mesh->GetPolygonVertex(polygon_index, i);
			FbxVector4 v = e->GetDirectArray().GetAt(index);
			poly_vertices[i].tangent.x = float(v[0]);
			poly_vertices[i].tangent.y = float(v[1]);
			poly_vertices[i].tangent.z = float(v[2]);
		}
		break;
	default:
		break;
	}
}

template <typename T>
void import_binormals(FbxMesh* mesh, int polygon_index, T* poly_vertices)
{
	if(mesh->GetElementBinormalCount() <= 0)
		return;

	auto *e = mesh->GetElementBinormal(0);
	switch(e->GetMappingMode()) {
	case FbxGeometryElement::eByPolygonVertex:
		for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
			int index = mesh->GetPolygonVertexIndex(polygon_index) + i;
			FbxVector4 v = e->GetDirectArray().GetAt(index);
			poly_vertices[i].binormal.x = float(v[0]);
			poly_vertices[i].binormal.y = float(v[1]);
			poly_vertices[i].binormal.z = float(v[2]);
		}
		break;
	case FbxGeometryElement::eByControlPoint:
		for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
			int index = mesh->GetPolygonVertex(polygon_index, i);
			FbxVector4 v = e->GetDirectArray().GetAt(index);
			poly_vertices[i].binormal.x = float(v[0]);
			poly_vertices[i].binormal.y = float(v[1]);
			poly_vertices[i].binormal.z = float(v[2]);
		}
		break;
	default:
		break;
	}
}

template <typename T>
void import_uv(FbxMesh* mesh, int polygon_index, T* poly_vertices)
{
	if(mesh->GetElementUVCount() <= 0)
		return;

	FbxGeometryElementUV *uv = mesh->GetElementUV(0);
	switch(uv->GetMappingMode()) {
	case FbxGeometryElement::eByPolygonVertex:
		for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
			int index = mesh->GetTextureUVIndex(polygon_index, i);
			FbxVector2 v = uv->GetDirectArray().GetAt(index);
			poly_vertices[i].uvw.x = float(v[0]);
			poly_vertices[i].uvw.y = float(-v[1]);
			poly_vertices[i].uvw.z = 1;
		}
		break;
	case FbxGeometryElement::eByControlPoint:
		for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
			int index = mesh->GetPolygonVertex(polygon_index, i);
			FbxVector2 v = uv->GetDirectArray().GetAt(index);
			poly_vertices[i].uvw.x = float(v[0]);
			poly_vertices[i].uvw.y = float(-v[1]);
			poly_vertices[i].uvw.z = 1;
		}
		break;
	default:
		break;
	}
}

int bone_index(const char* bone_name, GR2_skeleton *skel)
{
	// It seems Ribcage is always 53.
	if (strcmp(bone_name, "Ribcage") == 0)
		return 53;

	int index = 0;
	for (int i = 0; i < skel->bones_count; ++i) {
		if (strncmp(skel->bones[i].name, "ap_", 3) == 0) {
			// Ignore this bone
		}
		else if (strcmp(skel->bones[i].name, "Ribcage") == 0) {
			// Ignore this bone
		}
		else if (strcmp(bone_name, skel->bones[i].name) == 0)
			return index;
		else
			++index;
	}

	return index;
}

void import_skinning(FbxMesh *mesh, int vertex_index, GR2_skeleton *skel,
	MDB_file::Skin_vertex &poly_vertex)
{
	auto s = skin(mesh);
	assert(s);

	for (int i = 0; i < 4; ++i) {
		poly_vertex.bone_indices[i] = 0;
		poly_vertex.bone_weights[i] = 0;
	}

	poly_vertex.bone_count = 4;

	int bone_count = 0;

	for (int i = 0; i < s->GetClusterCount(); ++i) {
		auto cluster = s->GetCluster(i);
		for (int j = 0; j < cluster->GetControlPointIndicesCount(); ++j) {
			if (vertex_index == cluster->GetControlPointIndices()[j]) {
				if (bone_count == 4) {
					cout << "  A vertex cannot be influence by more than 4 bones\n";
					return;
				}
				poly_vertex.bone_indices[bone_count] = bone_index(cluster->GetLink()->GetName(), skel);
				poly_vertex.bone_weights[bone_count] = float(cluster->GetControlPointWeights()[j]);
				++bone_count;
			}
		}
	}
}

void import_skinning(FbxMesh *mesh, int polygon_index,
	GR2_skeleton *skel, MDB_file::Skin_vertex *poly_vertices)
{
	for (int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
		int index = mesh->GetPolygonVertex(polygon_index, i);
		import_skinning(mesh, index, skel, poly_vertices[i]);

	}
}

void import_map(char* map_name, FbxSurfaceMaterial* fbx_material,
                const char* name)
{
	FbxProperty property =
	    fbx_material->FindProperty(name);
	int textures = property.GetSrcObjectCount<FbxTexture>();
	if(textures <= 0)
		return;

	FbxTexture *texture = property.GetSrcObject<FbxTexture>(0);
	FbxFileTexture *file_texture = FbxCast<FbxFileTexture>(texture);
	if(!file_texture)
		return;

	strncpy(map_name,
	        path(file_texture->GetFileName()).stem().string().c_str(), 32);
}

void import_material(MDB_file::Material& material, FbxMesh* mesh)
{
	if(mesh->GetNode()->GetMaterialCount() <= 0)
		return;
	
	FbxSurfaceMaterial *fbx_material = mesh->GetNode()->GetMaterial(0);
	import_map(material.diffuse_map_name, fbx_material,
	           FbxSurfaceMaterial::sDiffuse);
	import_map(material.normal_map_name, fbx_material,
	           FbxSurfaceMaterial::sNormalMap);
	import_map(material.tint_map_name, fbx_material,
	           FbxSurfaceMaterial::sDisplacementColor);
	import_map(material.glow_map_name, fbx_material,
	           FbxSurfaceMaterial::sEmissive);

	if(fbx_material->ShadingModel.Get() != "Phong")
		return;

	auto m = (FbxSurfacePhong *)fbx_material;

	FbxDouble3 d = m->Diffuse.Get();
	material.diffuse_color.x = float(d[0]);
	material.diffuse_color.y = float(d[1]);
	material.diffuse_color.z = float(d[2]);

	FbxDouble3 s = m->Specular.Get();
	material.specular_color.x = float(s[0]);
	material.specular_color.y = float(s[1]);
	material.specular_color.z = float(s[2]);

	material.specular_value = float(m->SpecularFactor.Get()*200.0);
	material.specular_power = float(m->Shininess.Get()*2.5/100.0);
}

template <typename T, typename U>
unsigned push_vertex(T& mesh, U& v)
{
	for(unsigned i = 0; i < mesh.verts.size(); ++i) {
		if(mesh.verts[i] == v)
			return i;
	}

	mesh.verts.push_back(v);

	return mesh.verts.size() - 1;
}

void import_polygon(MDB_file::Collision_mesh& col_mesh, FbxMesh* mesh,
                    int polygon_index)
{
	if(mesh->GetPolygonSize(polygon_index) != 3) {
		cout << "  Polygon is not a triangle\n";
		return;
	}
	
	cout << "   ";

	for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i)
		cout << ' ' << mesh->GetPolygonVertex(polygon_index, i);

	MDB_file::Collision_mesh_vertex poly_vertices[3];
	import_positions(mesh, polygon_index, poly_vertices);
	import_normals(mesh, polygon_index, poly_vertices);
	import_uv(mesh, polygon_index, poly_vertices);

	MDB_file::Face face;

	for(int i = 0; i < 3; ++i)
		face.vertex_indices[i] =
		    push_vertex(col_mesh, poly_vertices[i]);

	col_mesh.faces.push_back(face);

	cout << endl;
}

void import_collision_mesh(MDB_file& mdb, FbxMesh* mesh)
{
	cout << "  Polygons: " << mesh->GetPolygonCount() << endl;

	auto col_mesh = make_unique<MDB_file::Collision_mesh>(
	    ends_with(mesh->GetName(), "_C2") ? MDB_file::COL2
	                                      : MDB_file::COL3);
	strncpy(col_mesh->header.name, mesh->GetName(), 32);

	for(int i = 0; i < mesh->GetPolygonCount(); ++i)
		import_polygon(*col_mesh.get(), mesh, i);

	mdb.add_packet(move(col_mesh));
}

void import_polygon(MDB_file::Rigid_mesh& rigid_mesh, FbxMesh* mesh,
	int polygon_index)
{
	if(mesh->GetPolygonSize(polygon_index) != 3) {
		cout << "  Polygon is not a triangle\n";
		return;
	}
	
	cout << "   ";

	for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i)
		cout << ' ' << mesh->GetPolygonVertex(polygon_index, i);

	MDB_file::Rigid_mesh_vertex poly_vertices[3];
	import_positions(mesh, polygon_index, poly_vertices);
	import_normals(mesh, polygon_index, poly_vertices);
	import_tangents(mesh, polygon_index, poly_vertices);
	import_binormals(mesh, polygon_index, poly_vertices);
	import_uv(mesh, polygon_index, poly_vertices);

	MDB_file::Face face;

	for(int i = 0; i < 3; ++i)
		face.vertex_indices[i] =
		    push_vertex(rigid_mesh, poly_vertices[i]);

	rigid_mesh.faces.push_back(face);

	cout << endl;
}

void print_mesh(FbxMesh *mesh)
{
	cout << "  Layers: " << mesh->GetLayerCount() << endl;

	cout << "  UV elements: " << mesh->GetElementUVCount();
	if (mesh->GetElementUVCount() > 0) {
		FbxGeometryElementUV *e = mesh->GetElementUV(0);
		cout << ' ' << mapping_mode_str(e->GetMappingMode()) << ' '
			<< reference_mode_str(e->GetReferenceMode());
	}
	cout << endl;

	cout << "  Normal elements: " << mesh->GetElementNormalCount();
	if (mesh->GetElementNormalCount() > 0) {
		auto e = mesh->GetElementNormal(0);
		cout << ' ' << mapping_mode_str(e->GetMappingMode()) << ' '
			<< reference_mode_str(e->GetReferenceMode());
	}
	cout << endl;

	cout << "  Tangent elements: " << mesh->GetElementTangentCount();
	if (mesh->GetElementTangentCount() > 0) {
		auto e = mesh->GetElementTangent(0);
		cout << ' ' << mapping_mode_str(e->GetMappingMode()) << ' '
			<< reference_mode_str(e->GetReferenceMode());
	}
	cout << endl;

	cout << "  Binormal elements: " << mesh->GetElementBinormalCount();
	if (mesh->GetElementBinormalCount() > 0) {
		auto e = mesh->GetElementBinormal(0);
		cout << ' ' << mapping_mode_str(e->GetMappingMode()) << ' '
			<< reference_mode_str(e->GetReferenceMode());
	}
	cout << endl;

	cout << "  Polygons: " << mesh->GetPolygonCount() << endl;
}

void import_rigid_mesh(MDB_file& mdb, FbxMesh* mesh)
{
	print_mesh(mesh);

	auto rigid_mesh = make_unique<MDB_file::Rigid_mesh>();
	strncpy(rigid_mesh->header.name, mesh->GetName(), 32);

	import_material(rigid_mesh->header.material, mesh);

	for(int i = 0; i < mesh->GetPolygonCount(); ++i)
		import_polygon(*rigid_mesh.get(), mesh, i);

	mdb.add_packet(move(rigid_mesh));
}

const char *skeleton_name(FbxNode *node)
{
	if (!node)
		return "";

	while (node->GetParent() != node->GetScene()->GetRootNode())
		node = node->GetParent();

	return node->GetName();
}

const char *skeleton_name(FbxMesh *mesh)
{	
	auto s = skin(mesh);

	if (!s)
		return "";

	auto cluster = s->GetCluster(0);

	return skeleton_name(cluster->GetLink());
}

void import_polygon(MDB_file::Skin& skin, GR2_skeleton *skel, FbxMesh* mesh,
	int polygon_index)
{
	if (mesh->GetPolygonSize(polygon_index) != 3) {
		cout << "  Polygon is not a triangle\n";
		return;
	}

	for (int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i)
		cout << ' ' << mesh->GetPolygonVertex(polygon_index, i);

	MDB_file::Skin_vertex poly_vertices[3];
	import_positions(mesh, polygon_index, poly_vertices);
	import_normals(mesh, polygon_index, poly_vertices);
	import_tangents(mesh, polygon_index, poly_vertices);
	import_binormals(mesh, polygon_index, poly_vertices);
	import_uv(mesh, polygon_index, poly_vertices);
	import_skinning(mesh, polygon_index, skel, poly_vertices);

	MDB_file::Face face;

	for (int i = 0; i < 3; ++i)
		face.vertex_indices[i] =
		push_vertex(skin, poly_vertices[i]);

	skin.faces.push_back(face);

	cout << endl;
}

void import_skin(MDB_file& mdb, FbxMesh* mesh)
{
	print_mesh(mesh);

#ifdef _WIN32
	auto skin = make_unique<MDB_file::Skin>();
	strncpy(skin->header.name, mesh->GetName(), 32);
	auto skel_name = skeleton_name(mesh);
	cout << "  Skeleton: " << skel_name << endl;

	strncpy(skin->header.skeleton_name, skel_name, 32);

	path skel_filename = (path("output") / skel_name).concat(".gr2");
	cout << "  Skeleton filename: " << skel_filename.string() << endl;

	GR2_file gr2(skel_filename.string().c_str());
	if (!gr2) {
		cout << "    " << gr2.error_string() << endl;
		return;
	}

	if (gr2.file_info->skeletons_count == 0) {
		cout << "  GR2 doesn't contains any skeleton\n";
		return;
	}

	import_material(skin->header.material, mesh);

	for (int i = 0; i < mesh->GetPolygonCount(); ++i)
		import_polygon(*skin.get(), gr2.file_info->skeletons[0], mesh, i);

	mdb.add_packet(move(skin));
#endif
}

void import_mesh(MDB_file& mdb, FbxMesh* mesh)
{
	cout << mesh->GetName() << endl;

	if (ends_with(mesh->GetName(), "_C2"))
		import_collision_mesh(mdb, mesh);
	else if (ends_with(mesh->GetName(), "_C3"))
		import_collision_mesh(mdb, mesh);
	else if (skin(mesh))
		import_skin(mdb, mesh);
	else
		import_rigid_mesh(mdb, mesh);

	cout << endl;
}

void import_meshes(MDB_file& mdb, FbxScene* scene)
{
	int mesh_count = scene->GetSrcObjectCount<FbxMesh>();
	for(int i = 0; i < mesh_count; ++i) {
		FbxMesh *m = scene->GetSrcObject<FbxMesh>(i);
		import_mesh(mdb, m);
	}
}

struct GR2_transform_track_data {
	std::vector<float> position_knots;
	std::vector<float> position_controls;
};

struct GR2_track_group_info {
	GR2_track_group track_group;
	std::vector<GR2_transform_track> transform_tracks;
};

struct GR2_import_info {
	FbxAnimStack *anim_stack;
	GR2_file_info file_info;
	GR2_art_tool_info art_tool_info;
	GR2_exporter_info exporter_info;
	GR2_animation animation;
	GR2_animation *animations[1];
	std::vector<GR2_track_group_info> track_groups;
	std::list<GR2_curve_data_D3Constant32f> d3c_curves;
	std::list<GR2_curve_data_DaK32fC32f> da_curves;
	std::list<GR2_curve_data_DaIdentity> id_curves;
	std::list<std::vector<float>> float_arrays;
	String_collection strings;
	std::vector<GR2_track_group*> track_group_pointers;
};

GR2_track_group_info &track_group(GR2_import_info& import_info, const char *name)
{
	// Find track group
	for(auto &tg : import_info.track_groups)
		if(strcmp(tg.track_group.name, name) == 0)
			return tg;

	import_info.track_groups.emplace_back();
	auto& tg = import_info.track_groups.back();
	tg.track_group.name = import_info.strings.get(name);
	tg.track_group.vector_tracks_count = 0;
	tg.track_group.transform_LOD_errors_count = 0;
	tg.track_group.text_tracks_count = 0;
	tg.track_group.initial_placement.flags = 0;
	tg.track_group.initial_placement.translation = Vector3<float>(0, 0, 0);
	tg.track_group.initial_placement.rotation = Vector4<float>(0, 0, 0, 1);
	tg.track_group.initial_placement.scale_shear[0] = 1;
	tg.track_group.initial_placement.scale_shear[1] = 0;
	tg.track_group.initial_placement.scale_shear[2] = 0;
	tg.track_group.initial_placement.scale_shear[3] = 0;
	tg.track_group.initial_placement.scale_shear[4] = 1;
	tg.track_group.initial_placement.scale_shear[5] = 0;
	tg.track_group.initial_placement.scale_shear[6] = 0;
	tg.track_group.initial_placement.scale_shear[7] = 0;
	tg.track_group.initial_placement.scale_shear[8] = 1;
	tg.track_group.accumulation_flags = 2;
	tg.track_group.loop_translation[0] = 0;
	tg.track_group.loop_translation[1] = 0;
	tg.track_group.loop_translation[2] = 0;
	tg.track_group.periodic_loop = nullptr;
	tg.track_group.root_motion = nullptr;
	tg.track_group.extended_data.keys = nullptr;
	tg.track_group.extended_data.values = nullptr;

	return tg;
}

void import_position(GR2_import_info& import_info, FbxNode* node,
                     FbxAnimLayer* layer, GR2_transform_track& tt)
{
	auto curve_x = node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_X);
	auto curve_y = node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Y);
	auto curve_z = node->LclTranslation.GetCurve(layer, FBXSDK_CURVENODE_COMPONENT_Z);

	if (curve_x || curve_y || curve_z) {
		cout << "  Positions:\n";

		tt.position_curve.keys = DaK32fC32f_def;

		import_info.da_curves.emplace_back();
		auto& curve = import_info.da_curves.back();
		curve.curve_data_header_DaK32fC32f.format = DaK32fC32f;
		curve.curve_data_header_DaK32fC32f.degree = 1;

		import_info.float_arrays.emplace_back();
		auto& knots = import_info.float_arrays.back();

		import_info.float_arrays.emplace_back();
		auto& controls = import_info.float_arrays.back();

		FbxTime time = import_info.anim_stack->LocalStart;
		FbxTime dt;
		dt.SetSecondDouble(time_step);
		float t = 0;

		while (time <= import_info.anim_stack->LocalStop) {
			knots.push_back(t);

			auto p = node->LclTranslation.EvaluateValue(time);
			controls.push_back(float(p[0]));
			controls.push_back(float(p[1]));
			controls.push_back(float(p[2]));

			cout << "    " << knots.back() << ": " << p[0] << ' '
			     << p[1] << ' ' << p[2] << endl;

			time += dt;
			t += time_step;
		}

		curve.knots_count = knots.size();
		curve.knots = knots.data();
		curve.controls_count = controls.size();
		curve.controls = controls.data();

		tt.position_curve.curve_data = reinterpret_cast<GR2_curve_data*>(&curve);
	}
	else {
		tt.position_curve.keys = D3Constant32f_def;

		import_info.d3c_curves.emplace_back();
		auto& curve = import_info.d3c_curves.back();
		curve.curve_data_header_D3Constant32f.format = D3Constant32f;
		curve.curve_data_header_D3Constant32f.degree = 0;
		curve.controls[0] = float(node->LclTranslation.Get()[0]);
		curve.controls[1] = float(node->LclTranslation.Get()[1]);
		curve.controls[2] = float(node->LclTranslation.Get()[2]);
		tt.position_curve.curve_data =
		    reinterpret_cast<GR2_curve_data*>(&curve);
	}
}

void import_rotation(GR2_import_info& import_info, FbxNode* node,
                     FbxAnimLayer* layer, GR2_transform_track& tt)
{
	cout << "  Rotations:\n";

	tt.orientation_curve.keys = DaK32fC32f_def;

	import_info.da_curves.emplace_back();
	auto& curve = import_info.da_curves.back();
	curve.curve_data_header_DaK32fC32f.format = DaK32fC32f;
	curve.curve_data_header_DaK32fC32f.degree = 1;

	import_info.float_arrays.emplace_back();
	auto& knots = import_info.float_arrays.back();

	import_info.float_arrays.emplace_back();
	auto& controls = import_info.float_arrays.back();

	FbxTime time = import_info.anim_stack->LocalStart;
	FbxTime dt;
	dt.SetSecondDouble(time_step);
	float t = 0;

	FbxQuaternion prev_quat(0, 0, 0, 1);

	while (time <= import_info.anim_stack->LocalStop) {
		knots.push_back(t);

		auto p = node->LclRotation.EvaluateValue(time);
		
		auto quat = euler_to_quaternion(p);		

		if (quat.DotProduct(prev_quat) < 0)
			quat = -quat;

		if (strcmp(tt.name, "RLeg1") == 0 && t > 1) {			
			controls.push_back(float(prev_quat[0]));
			controls.push_back(float(prev_quat[1]));
			controls.push_back(float(prev_quat[2]));
			controls.push_back(float(prev_quat[3]));
		}
		else {
			controls.push_back(float(quat[0]));
			controls.push_back(float(quat[1]));
			controls.push_back(float(quat[2]));
			controls.push_back(float(quat[3]));
			prev_quat = quat;
		}

		cout << "    " << knots.back() << ": ";
		cout << '[' << p[0] << ' ' << p[1] << ' ' << p[2] << "] "<< quat[0] << ' '
		     << quat[1] << ' ' << quat[2] << ' ' << quat[3] << endl;

		time += dt;
		t += time_step;
	}

	curve.knots_count = knots.size();
	curve.knots = knots.data();
	curve.controls_count = controls.size();
	curve.controls = controls.data();

	tt.orientation_curve.curve_data = reinterpret_cast<GR2_curve_data*>(&curve);
}

void import_scaleshear(GR2_import_info& import_info, FbxNode* node,
                       GR2_transform_track& tt)
{
	tt.scale_shear_curve.keys = DaIdentity_def;

	import_info.id_curves.emplace_back();
	auto& curve = import_info.id_curves.back();
	curve.curve_data_header_DaIdentity.format = DaIdentity;
	curve.curve_data_header_DaIdentity.degree = 0;
	curve.dimension = 9;
	tt.scale_shear_curve.curve_data = reinterpret_cast<GR2_curve_data*>(&curve);
}

void import_anim_layer(GR2_import_info& import_info, FbxAnimLayer* layer,
                       FbxNode* node)
{
	cout << node->GetName() << endl;

	auto attr = node->GetNodeAttribute();	
	if(attr && attr->GetAttributeType() == FbxNodeAttribute::eSkeleton && node->GetParent() != node->GetScene()->GetRootNode())	{
		auto skel_name = skeleton_name(node);
		auto &tg = track_group(import_info, skel_name);				

		tg.transform_tracks.emplace_back();
		auto& tt = tg.transform_tracks.back();
		tt.name = import_info.strings.get(node->GetName());

		import_position(import_info, node, layer, tt);
		import_rotation(import_info, node, layer, tt);
		import_scaleshear(import_info, node, tt);
	}

	for(int i = 0; i < node->GetChildCount(); ++i)
		import_anim_layer(import_info, layer, node->GetChild(i));
}

void import_anim_layer(GR2_import_info& import_info, FbxAnimLayer* layer)
{
	printf("    - Name: %s\n", layer->GetName());
	printf("      Animation curve nodes: %d\n", layer->GetMemberCount<FbxAnimCurveNode>());

	import_anim_layer(import_info, layer, layer->GetScene()->GetRootNode());
}

void import_animation(FbxAnimStack *stack, const char* filename)
{
	GR2_import_info import_info;

	import_info.anim_stack = stack;

	import_info.art_tool_info.from_art_tool_name =
	    import_info.strings.get("FBX");
	import_info.art_tool_info.art_tool_major_revision = 0;
	import_info.art_tool_info.art_tool_minor_revision = 0;
	import_info.art_tool_info.units_per_meter = 100;
	import_info.art_tool_info.origin = Vector3<float>(0, 0, 0);
	import_info.art_tool_info.right_vector = Vector3<float>(1, 0, 0);
	import_info.art_tool_info.up_vector = Vector3<float>(0, 0, 1);
	import_info.art_tool_info.back_vector = Vector3<float>(0, -1, 0);
	import_info.file_info.art_tool_info = &import_info.art_tool_info;

	import_info.exporter_info.exporter_name =
	    import_info.strings.get("NWN2 MDK 0.1");
	import_info.exporter_info.exporter_major_revision = 2;
	import_info.exporter_info.exporter_minor_revision = 6;
	import_info.exporter_info.exporter_customization = 0;
	import_info.exporter_info.exporter_build_number = 10;
	import_info.file_info.exporter_info = &import_info.exporter_info;

	import_info.file_info.from_file_name =
	    import_info.strings.get(filename);

	import_info.file_info.textures_count = 0;
	import_info.file_info.materials_count = 0;
	import_info.file_info.skeletons_count = 0;
	import_info.file_info.vertex_datas_count = 0;
	import_info.file_info.tri_topologies_count = 0;
	import_info.file_info.meshes_count = 0;
	import_info.file_info.models_count = 0;
	import_info.file_info.track_groups_count = 0;

	printf("  Animation layers: #%d\n", stack->GetMemberCount<FbxAnimLayer>());
	for(int i = 0; i < stack->GetMemberCount<FbxAnimLayer>(); ++i) {
		import_anim_layer(import_info,
		                  stack->GetMember<FbxAnimLayer>(i));
	}

	for(auto &tg : import_info.track_groups) {
		sort(tg.transform_tracks.begin(), tg.transform_tracks.end(), 
			[](GR2_transform_track &t1, GR2_transform_track &t2) {
			return strcmp(t1.name, t2.name) < 0;
		});		
		tg.track_group.transform_tracks_count = tg.transform_tracks.size();	
		//tg.track_group.transform_tracks_count = 16;
		tg.track_group.transform_tracks = tg.transform_tracks.data();
		import_info.track_group_pointers.push_back(&tg.track_group);
	}

	import_info.file_info.track_groups_count = import_info.track_group_pointers.size();
	import_info.file_info.track_groups = import_info.track_group_pointers.data();

	import_info.animation.name = import_info.strings.get(stack->GetName());
	import_info.animation.duration =
	    float(import_info.anim_stack->LocalStop.Get().GetSecondDouble() -
	    import_info.anim_stack->LocalStart.Get().GetSecondDouble());
	import_info.animation.time_step = float(time_step);
	import_info.animation.oversampling = 1;
	import_info.animation.track_groups_count = import_info.track_group_pointers.size();
	import_info.animation.track_groups = import_info.track_group_pointers.data();

	import_info.animations[0] = &import_info.animation;

	import_info.file_info.animations_count = 1;
	import_info.file_info.animations = import_info.animations;

	import_info.file_info.extended_data.keys = nullptr;
	import_info.file_info.extended_data.values = nullptr;

	GR2_file gr2;
	gr2.read(&import_info.file_info);
	string output_filename = (path("output") / path(filename).stem()).string() + ".anim.gr2";
	gr2.write(output_filename.c_str());

	cout << "\nOutput is " << output_filename << endl;
}

void import_animations(FbxScene* scene, const char* filename)
{
	printf("Animation stacks: #%d\n", scene->GetSrcObjectCount<FbxAnimStack>());
	for(int i = 0; i < scene->GetSrcObjectCount<FbxAnimStack>(); ++i)
		import_animation(scene->GetSrcObject<FbxAnimStack>(i), filename);
}

int main(int argc, char* argv[])
{
	if(argc < 2) {
		cout << "Usage: fbx2nw <file>\n";
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

	// Create an importer.
	auto importer = FbxImporter::Create(manager, "");
	if(!importer->Initialize(argv[1], -1, manager->GetIOSettings())) {
		cout << importer->GetStatus().GetErrorString() << endl;
		return 1;
	}

	// Create a new scene so it can be populated by the imported file.
	FbxScene* scene = FbxScene::Create(manager, "");

	// Import the contents of the file into the scene.
	importer->Import(scene);

	// The file has been imported; we can get rid of the importer.
	importer->Destroy();

	MDB_file mdb;

	import_meshes(mdb, scene);
	import_animations(scene, argv[1]);

	string output_filename = (path("output") / path(argv[1]).stem()).string() + ".MDB";
	mdb.save(output_filename.c_str());

	cout << "\nOutput is " << output_filename << endl;

	// Destroy the sdk manager and all other objects it was handling.
	manager->Destroy();

	return 0;
}