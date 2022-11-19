#include <iostream>
#include <filesystem>
#include <set>
#include <list>
#include <assert.h>
#include <algorithm>

#include "config.h"
#include "fbxsdk.h"
#include "gr2_file.h"
#include "log.h"
#include "mdb_file.h"
#include "redirect_output_handle.h"
#include "string_collection.h"

enum class Output_type {
	any,
	mdb,
	gr2
};

struct Import_info {
	std::string input_path;
	// Without extension.
	std::string output_path;
	Output_type output_type;
};

const double time_step = 1 / 30.0;

static GR2_property_key CurveDataHeader_def[] = {
	{ GR2_type_uint8, (char*)"Format", nullptr, 0, 0, 0, 0, 0},
	{ GR2_type_uint8, (char*)"Degree", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Real32_def[] = {
	{ GR2_type_real32, (char*)"Real32", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key DaConstant32f_def[] = {
	{ GR2_type_inline, (char*)"CurveDataHeader_DaConstant32f", CurveDataHeader_def, 0, 0, 0, 0, 0 },
	{ GR2_type_int16, (char*)"Padding", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_pointer, (char*)"Controls", Real32_def, 0, 0, 0, 0, 0 },
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

static GR2_property_key DaK32fC32f_def[] = {
	{ GR2_type_inline, (char*)"CurveDataHeader_DaK32fC32f", CurveDataHeader_def, 0, 0, 0, 0, 0 },
	{ GR2_type_int16, (char*)"Padding", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_pointer, (char*)"Knots", Real32_def, 0, 0, 0, 0, 0 },
	{ GR2_type_pointer, (char*)"Controls", Real32_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

using namespace std;
using namespace std::filesystem;

static bool parse_args(int argc, char* argv[], Import_info& import_info)
{
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] == '-') {
			if (strcmp(argv[i], "-o") == 0 && i < argc - 1)
				import_info.output_path = argv[++i];
		}
		else if (import_info.input_path.empty()) {
			import_info.input_path = argv[i];
		}
		else {
			Log::error() << "More than one input file provided\n";
			return false;
		}
	}

	if (import_info.input_path.empty()) {
		Log::error() << "No input file provided\n";
		return false;
	}

	if (import_info.output_path.empty()) {
		import_info.output_path =
		    path(import_info.input_path).stem().string();
		import_info.output_type = Output_type::any;
	}
	else {
		string ext = path(import_info.output_path).extension().string();

		if (stricmp(ext.c_str(), ".mdb") == 0)
			import_info.output_type = Output_type::mdb;
		else if (stricmp(ext.c_str(), ".gr2") == 0)
			import_info.output_type = Output_type::gr2;
		else {
			Log::error() << "Unrecognized output extension\n";
			return false;
		}

		import_info.output_path =
		    path(import_info.output_path).replace_extension().string();
	}

	return true;
}

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

bool operator==(const MDB_file::Walk_mesh_vertex &v1, const MDB_file::Walk_mesh_vertex &v2)
{
	return v1.position == v2.position;
}

// Assumes the euler angles are from a right-handed system with y-axis up.
FbxQuaternion euler_to_quaternion(const FbxVector4 &v)
{
	double pitch = v[0];
	double roll = v[1];
	double yaw = v[2];

	pitch = pitch * M_PI / 180; // To radians
	roll = roll * M_PI / 180; // To radians
	yaw = yaw * M_PI / 180; // To radians
	
	double sinp = sin(pitch * 0.5);
	double cosp = cos(pitch * 0.5);
	double sinr = sin(roll * 0.5);
	double cosr = cos(roll * 0.5);
	double siny = sin(yaw * 0.5);
	double cosy = cos(yaw * 0.5);

	FbxQuaternion q;
	q[0] = sinp*cosr*cosy - cosp*sinr*siny;
	q[1] = cosp*sinr*cosy + sinp*cosr*siny;
	q[2] = cosp*cosr*siny - sinp*sinr*cosy;
	q[3] = cosp*cosr*cosy + sinp*sinr*siny;

	return q;
}

bool starts_with(const char *s1, const char *s2)
{
	return strncmp(s1, s2, strlen(s2)) == 0;
}

// Case-insensitive comparison.
static bool starts_with_i(const char* s1, const char* s2)
{
	return strnicmp(s1, s2, strlen(s2)) == 0;
}

bool ends_with(const char *s1, const char *s2)
{
	auto l1 = strlen(s1);
	auto l2 = strlen(s2);

	return l1 >= l2 && strcmp(s1 + l1 - l2, s2) == 0;
}

void trim_start_spaces(std::string& s)
{
	s.erase(s.begin(),
		find_if(s.begin(), s.end(),
			[](int ch) { return !isspace(ch); }));
}

void trim_end_spaces(std::string& s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(),
		[](int ch) { return !isspace(ch); }).base(),
		s.end());
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

FbxSkin *skin(FbxNode* node)
{
	auto mesh = node->GetMesh();

	if (!mesh)
		return nullptr;

	return skin(mesh);	
}

void set_packet_name(char* packet_name, const char* node_name)
{
	strncpy(packet_name, node_name, 31);
	packet_name[31] = '\0';

	if (strlen(node_name) > 31)
		Log::error() << "Packet name greater than 31 chars, truncating: " << packet_name << endl;
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
			poly_vertices[i].binormal.x = float(-v[0]);
			poly_vertices[i].binormal.y = float(-v[1]);
			poly_vertices[i].binormal.z = float(-v[2]);
		}
		break;
	case FbxGeometryElement::eByControlPoint:
		for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
			int index = mesh->GetPolygonVertex(polygon_index, i);
			FbxVector4 v = e->GetDirectArray().GetAt(index);
			poly_vertices[i].binormal.x = float(-v[0]);
			poly_vertices[i].binormal.y = float(-v[1]);
			poly_vertices[i].binormal.z = float(-v[2]);
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

struct Fbx_bones {
	std::vector<FbxNode*> bones;
	std::vector<FbxNode*> body_bones;
	std::vector<FbxNode*> face_bones;
	FbxNode* ribcage = nullptr;
};

int bone_index(const char* bone_name, Fbx_bones& fbx_bones)
{
	// Ribcage bone is always the last bone.
	if (strcmp(bone_name, "Ribcage") == 0)
		return fbx_bones.body_bones.size();

	// Search in the body bones.
	for (unsigned i = 0; i < fbx_bones.body_bones.size(); ++i) {
		if (strcmp(fbx_bones.body_bones[i]->GetName(), bone_name) == 0)
			return i;		
	}

	// Search in the face bones.
	for (unsigned i = 0; i < fbx_bones.face_bones.size(); ++i) {
		if (strcmp(fbx_bones.face_bones[i]->GetName(), bone_name) == 0)
			return i;
	}

	return 0;
}

bool validate_vertex_weights(FbxCluster* cluster, int bone_count)
{
	if (bone_count == 4) {
		Log::error() << "Vertex has more than 4 weights.\n";
		return false;
	}
	else if (starts_with(cluster->GetLink()->GetName(), "ap_")) {
		Log::error() << "Vertex is weighted to non-rendering bone (" << cluster->GetLink()->GetName() << ").\n";
		return false;
	}

	return true;
}

void init_skin_vertex(MDB_file::Skin_vertex &v)
{
	for (int i = 0; i < 4; ++i) {
		v.bone_indices[i] = 0;
		v.bone_weights[i] = 0;
	}

	v.bone_count = 4;
}

// Makes the sum of bone weights to be 1.
void normalize_bone_weights(MDB_file::Skin_vertex &v)
{
	float sum = 0;

	for (int i = 0; i < 4; ++i)
		sum += v.bone_weights[i];

	if (sum == 0) {		
		Log::error() << "Vertex has no weights.\n";
		return;
	}

	for (int i = 0; i < 4; ++i)
		v.bone_weights[i] /= sum;
}

void import_skinning(FbxMesh *mesh, int vertex_index, Fbx_bones& fbx_bones,
	MDB_file::Skin_vertex &poly_vertex)
{
	auto s = skin(mesh);
	assert(s);

	init_skin_vertex(poly_vertex);

	int bone_count = 0;

	for (int i = 0; i < s->GetClusterCount(); ++i) {
		auto cluster = s->GetCluster(i);
		for (int j = 0; j < cluster->GetControlPointIndicesCount(); ++j) {
			if (vertex_index == cluster->GetControlPointIndices()[j]) {
				if (!validate_vertex_weights(cluster, bone_count))
					return;

				poly_vertex.bone_indices[bone_count] = bone_index(cluster->GetLink()->GetName(), fbx_bones);
				poly_vertex.bone_weights[bone_count] = float(cluster->GetControlPointWeights()[j]);
				++bone_count;
			}
		}
	}

	normalize_bone_weights(poly_vertex);		
}

void import_skinning(FbxMesh *mesh, int polygon_index,
	Fbx_bones& fbx_bones, MDB_file::Skin_vertex *poly_vertices)
{
	for (int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
		int index = mesh->GetPolygonVertex(polygon_index, i);
		import_skinning(mesh, index, fbx_bones, poly_vertices[i]);

	}
}

void set_map_name(char* map_name, const char* texture_filename)
{
	strncpy(map_name, texture_filename, 31);
	map_name[31] = '\0';

	if (strlen(texture_filename) > 31)
		Log::error() << "Map name greater than 31 chars, truncating: " << map_name << endl;
}

void import_map(char* map_name, FbxSurfaceMaterial* fbx_material,
	const char* name)
{
	FbxProperty property =
		fbx_material->FindProperty(name);
	int textures = property.GetSrcObjectCount<FbxTexture>();
	if (textures <= 0)
		return;

	FbxTexture *texture = property.GetSrcObject<FbxTexture>(0);
	FbxFileTexture *file_texture = FbxCast<FbxFileTexture>(texture);
	if (!file_texture)
		return;

	set_map_name(map_name,
		path(file_texture->GetFileName()).stem().string().c_str());
}

void import_map(char* map_name, FbxNode* node, const char* property_name)
{
	auto prop = node->FindProperty(property_name, false);
	if (!prop.IsValid())
		return;

	string s = prop.Get<FbxString>().Buffer();
	trim_start_spaces(s);
	trim_end_spaces(s);	
	set_map_name(map_name, s.c_str());
}

void import_diffuse_color(MDB_file::Material& material, FbxNode* node,
	FbxSurfacePhong* fbx_material)
{
	auto prop = node->FindProperty("NWN2MDK_DIFFUSE_COLOR", false);

	if (!prop.IsValid())
		prop = node->FindProperty("DIFFUSE_COLOR", false);

	FbxDouble3 c;

	if (prop.IsValid())
		c = prop.Get<FbxDouble3>();
	else
		c = fbx_material->Diffuse.Get();

	material.diffuse_color.x = float(c[0]);
	material.diffuse_color.y = float(c[1]);
	material.diffuse_color.z = float(c[2]);
}

void import_specular_color(MDB_file::Material& material, FbxNode* node,
	FbxSurfacePhong* fbx_material)
{
	auto prop = node->FindProperty("NWN2MDK_SPECULAR_COLOR", false);

	if (!prop.IsValid())
		prop = node->FindProperty("SPECULAR_COLOR", false);

	FbxDouble3 c;

	if (prop.IsValid())
		c = prop.Get<FbxDouble3>();
	else
		c = fbx_material->Specular.Get();

	material.specular_color.x = float(c[0]);
	material.specular_color.y = float(c[1]);
	material.specular_color.z = float(c[2]);
}

void import_specular_level(MDB_file::Material& material, FbxNode* node,
	FbxSurfacePhong* fbx_material)
{
	auto prop = node->FindProperty("NWN2MDK_SPECULAR_LEVEL", false);

	if (!prop.IsValid())
		prop = node->FindProperty("SPECULAR_LEVEL", false);

	if (prop.IsValid())
		material.specular_level = prop.Get<float>();
	else
		material.specular_level = float(fbx_material->SpecularFactor.Get());
}

void import_specular_power(MDB_file::Material& material, FbxNode* node,
	FbxSurfacePhong* fbx_material)
{
	auto prop = node->FindProperty("NWN2MDK_GLOSSINESS", false);

	if (!prop.IsValid())
		prop = node->FindProperty("GLOSSINESS", false);

	if (prop.IsValid())
		material.specular_power = prop.Get<float>();
	else
		material.specular_power = float(fbx_material->Shininess.Get());
}

void import_material(MDB_file::Material& material, FbxMesh* mesh)
{
	if(mesh->GetNode()->GetMaterialCount() <= 0)
		return;
	
	FbxSurfaceMaterial *fbx_material = mesh->GetNode()->GetMaterial(0);
	import_map(material.diffuse_map_name, fbx_material,
	           FbxSurfaceMaterial::sDiffuse);
	import_map(material.normal_map_name, fbx_material,
	           FbxSurfaceMaterial::sBump);
	import_map(material.normal_map_name, fbx_material,
	           FbxSurfaceMaterial::sNormalMap);
	import_map(material.tint_map_name, mesh->GetNode(),
	           "TINT_MAP");
	import_map(material.tint_map_name, mesh->GetNode(),
	           "NWN2MDK_TINT_MAP");
	import_map(material.glow_map_name, fbx_material,
	           FbxSurfaceMaterial::sEmissiveFactor);
	import_map(material.glow_map_name, fbx_material,
	           FbxSurfaceMaterial::sEmissive);

	if(fbx_material->ShadingModel.Get() != "Phong")
		return;

	auto m = (FbxSurfacePhong *)fbx_material;

	import_diffuse_color(material, mesh->GetNode(), m);
	import_specular_color(material, mesh->GetNode(), m);
	import_specular_level(material, mesh->GetNode(), m);
	import_specular_power(material, mesh->GetNode(), m);
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
		Log::error() << "Polygon is not a triangle.\n";
		return;
	}

	MDB_file::Collision_mesh_vertex poly_vertices[3];
	import_positions(mesh, polygon_index, poly_vertices);
	import_normals(mesh, polygon_index, poly_vertices);
	import_uv(mesh, polygon_index, poly_vertices);

	MDB_file::Face face;

	for(int i = 0; i < 3; ++i)
		face.vertex_indices[i] =
		    push_vertex(col_mesh, poly_vertices[i]);

	col_mesh.faces.push_back(face);
}

void import_collision_mesh(MDB_file& mdb, FbxNode* node)
{
	auto mesh = node->GetMesh();

	if (!mesh)
		return;

	cout << "Exporting COL2|COL3: " << node->GetName() << endl;

	cout << "  Vertices: " << mesh->GetControlPointsCount() << endl;
	cout << "  Polygons: " << mesh->GetPolygonCount() << endl;

	auto col_mesh = make_unique<MDB_file::Collision_mesh>(
	    ends_with(node->GetName(), "_C2") ? MDB_file::COL2
	                                      : MDB_file::COL3);
	set_packet_name(col_mesh->header.name, node->GetName());

	for(int i = 0; i < mesh->GetPolygonCount(); ++i)
		import_polygon(*col_mesh.get(), mesh, i);

	mdb.add_packet(move(col_mesh));
}

// Returns the material of a mesh polygon.
FbxSurfaceMaterial *polygon_material(FbxMesh* mesh, int polygon_index)
{
	auto element_material = mesh->GetElementMaterial(0);
	if (!element_material)
		return nullptr;

	int mat_index;
	switch (element_material->GetMappingMode()) {
	case FbxLayerElement::eByPolygon:
		mat_index = element_material->GetIndexArray()[polygon_index];
		return mesh->GetNode()->GetMaterial(mat_index);		
	case FbxLayerElement::eAllSame: // All polygons have same material.
		mat_index = element_material->GetIndexArray()[0];
		return mesh->GetNode()->GetMaterial(mat_index);		
	default:
		return nullptr;
	}	
}

bool is_walk_mesh(FbxNode* node)
{
	return ends_with(node->GetName(), "_W") || ends_with(node->GetName(), "_w");
}

uint16_t walk_mesh_face_flags(FbxMesh* mesh, int polygon_index)
{
	auto mat = polygon_material(mesh, polygon_index);
	if (!mat)
		return 0;

	for (unsigned i = 0; i < size(MDB_file::walk_mesh_materials); ++i)
		if (starts_with(mat->GetName(), MDB_file::walk_mesh_materials[i].name))
			return MDB_file::walk_mesh_materials[i].flags;

	return 0;
}

void import_polygon(MDB_file::Walk_mesh& walk_mesh, FbxMesh* mesh,
	int polygon_index)
{
	if (mesh->GetPolygonSize(polygon_index) != 3) {
		Log::error() << "Polygon is not a triangle.\n";
		return;
	}

	MDB_file::Walk_mesh_vertex poly_vertices[3];
	import_positions(mesh, polygon_index, poly_vertices);

	MDB_file::Walk_mesh_face face;

	for (int i = 0; i < 3; ++i) {
		// When z is below a threshold, interpret it as the special big number.
		if (poly_vertices[i].position.z < -19.9f)
			poly_vertices[i].position.z = -1000000.0f;

		face.vertex_indices[i] = push_vertex(walk_mesh, poly_vertices[i]);
	}

	face.flags[0] = walk_mesh_face_flags(mesh, polygon_index);
	face.flags[1] = 0;

	walk_mesh.faces.push_back(face);
}

void import_walk_mesh(MDB_file& mdb, FbxNode* node)
{
	auto mesh = node->GetMesh();

	if (!mesh)
		return;

	cout << "Exporting WALK: " << node->GetName() << endl;

	auto walk_mesh = make_unique<MDB_file::Walk_mesh>();
	set_packet_name(walk_mesh->header.name, node->GetName());

	for (int i = 0; i < mesh->GetPolygonCount(); ++i)
		import_polygon(*walk_mesh, mesh, i);

	mdb.add_packet(move(walk_mesh));
}

static void print_vector3(const Vector3<float>& v)
{
	cout << v.x << ", " << v.y << ", " << v.z << endl;
}

static void print_orientation(const float orientation[3][3])
{
	for (int i = 0; i < 3; ++i) {
		cout << "    ";
		for (int j = 0; j < 3; ++j)
			cout << orientation[i][j] << ' ';
		cout << endl;
	}
}

static bool has_skeleton_attribute(FbxNode* node)
{
	auto attr = node->GetNodeAttribute();
	return attr && attr->GetAttributeType() == FbxNodeAttribute::eSkeleton;
}

static bool is_hook_packet(FbxNode* node)
{
	return !has_skeleton_attribute(node) &&
	       (starts_with_i(node->GetName(), "HP_") ||
	        starts_with_i(node->GetName(), "AP_"));
}

void print_hook(MDB_file::Hook& hook)
{
	cout << "  Position: ";
	print_vector3(hook.header.position);

	cout << "  Orientation:\n";
	print_orientation(hook.header.orientation);	
}

void transform_to_orientation(const FbxAMatrix& m, float orientation[3][3])
{
	FbxAMatrix mp = m;
	mp.SetS(FbxVector4(1, 1, 1)); // Reset scale

	FbxAMatrix r90x;
	r90x.SetR(FbxVector4(90, 0, 0));
	mp = mp*r90x; // Undo rotation from the exporter

	// Swap y-axis with z-axis. The negations are for flipping handedness.
	orientation[0][0] = float(-mp.Get(2, 0));
	orientation[0][1] = float(mp.Get(2, 2));
	orientation[0][2] = float(-mp.Get(2, 1));
	orientation[1][0] = float(mp.Get(0, 0));
	orientation[1][1] = float(-mp.Get(0, 2));
	orientation[1][2] = float(mp.Get(0, 1));
	orientation[2][0] = float(mp.Get(1, 0));
	orientation[2][1] = float(-mp.Get(1, 2));
	orientation[2][2] = float(mp.Get(1, 1));
}

void import_hook_point(MDB_file& mdb, FbxNode* node)
{
	cout << "Exporting HOOK: " << node->GetName() << endl;

	auto hook = make_unique<MDB_file::Hook>();
	set_packet_name(hook->header.name, node->GetName());
	
	auto m = node->EvaluateGlobalTransform();

	auto translation = m.GetT();	
	hook->header.position.x = float(translation[0]/100);
	hook->header.position.y = float(-translation[2]/100);
	hook->header.position.z = float(translation[1]/100);

	transform_to_orientation(m, hook->header.orientation);

	print_hook(*hook);
	
	mdb.add_packet(move(hook));
}

static bool is_hair_packet(FbxNode* node)
{
	return node->FindProperty("HSB_LOW", false).IsValid() ||
		node->FindProperty("HSB_SHORT", false).IsValid() ||
		node->FindProperty("HSB_PONYTAIL", false).IsValid() ||
		node->FindProperty("NWN2MDK_HSB", false).IsValid();
}

static void print_hair(const MDB_file::Hair& hair)
{	
	cout << "  Shortening: " << hair.header.shortening_behavior;
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

	cout << "  Position: ";
	print_vector3(hair.header.position);

	cout << "  Orientation:\n";
	print_orientation(hair.header.orientation);
}

MDB_file::Hair_shortening_behavior hair_shortening_behavior(FbxNode* node)
{
	auto prop = node->FindProperty("NWN2MDK_HSB", false);
	if (prop.IsValid()) {
		FbxString hsb = prop.Get<FbxString>();

		if (hsb == "LOW")
			return MDB_file::HSB_LOW;
		else if (hsb == "SHORT")
			return MDB_file::HSB_SHORT;
		else if (hsb == "PONYTAIL")
			return MDB_file::HSB_PONYTAIL;
		else
			return MDB_file::HSB_LOW;
	}

	prop = node->FindProperty("HSB_LOW", false);
	if (prop.IsValid() && prop.Get<float>() != 0)
		return MDB_file::HSB_LOW;

	prop = node->FindProperty("HSB_SHORT", false);
	if (prop.IsValid() && prop.Get<float>() != 0)
		return MDB_file::HSB_SHORT;

	prop = node->FindProperty("HSB_PONYTAIL", false);
	if (prop.IsValid() && prop.Get<float>() != 0)
		return MDB_file::HSB_PONYTAIL;

	return MDB_file::HSB_LOW;
}

void import_hair(MDB_file& mdb, FbxNode* node)
{
	cout << "Exporting HAIR: " << node->GetName() << endl;

	auto hair = make_unique<MDB_file::Hair>();
	set_packet_name(hair->header.name, node->GetName());
	hair->header.shortening_behavior = hair_shortening_behavior(node);

	auto m = node->EvaluateGlobalTransform();

	auto translation = m.GetT();
	hair->header.position.x = float(translation[0] / 100);
	hair->header.position.y = float(-translation[2] / 100);
	hair->header.position.z = float(translation[1] / 100);

	transform_to_orientation(m, hair->header.orientation);	

	print_hair(*hair);

	mdb.add_packet(move(hair));
}

static bool is_helm_packet(FbxNode* node)
{
	return node->FindProperty("HHHB_NONE_HIDDEN", false).IsValid() ||
		node->FindProperty("HHHB_HAIR_HIDDEN", false).IsValid() ||
		node->FindProperty("HHHB_PARTIAL_HAIR", false).IsValid() ||
		node->FindProperty("HHHB_HEAD_HIDDEN", false).IsValid() ||
		node->FindProperty("NWN2MDK_HHHB", false).IsValid();
}

MDB_file::Helm_hair_hiding_behavior helm_hair_hiding_behavior(FbxNode* node)
{
	auto prop = node->FindProperty("NWN2MDK_HHHB", false);
	if (prop.IsValid()) {
		FbxString hsb = prop.Get<FbxString>();

		if (hsb == "NONE_HIDDEN")
			return MDB_file::HHHB_NONE_HIDDEN;
		else if (hsb == "HAIR_HIDDEN")
			return MDB_file::HHHB_HAIR_HIDDEN;
		else if (hsb == "PARTIAL_HAIR")
			return MDB_file::HHHB_PARTIAL_HAIR;
		else if (hsb == "HEAD_HIDDEN")
			return MDB_file::HHHB_HEAD_HIDDEN;
		else
			return MDB_file::HHHB_NONE_HIDDEN;
	}

	prop = node->FindProperty("HHHB_NONE_HIDDEN", false);
	if (prop.IsValid() && prop.Get<float>() != 0)
		return MDB_file::HHHB_NONE_HIDDEN;

	prop = node->FindProperty("HHHB_HAIR_HIDDEN", false);
	if (prop.IsValid() && prop.Get<float>() != 0)
		return MDB_file::HHHB_HAIR_HIDDEN;

	prop = node->FindProperty("HHHB_PARTIAL_HAIR", false);
	if (prop.IsValid() && prop.Get<float>() != 0)
		return MDB_file::HHHB_PARTIAL_HAIR;

	prop = node->FindProperty("HHHB_HEAD_HIDDEN", false);
	if (prop.IsValid() && prop.Get<float>() != 0)
		return MDB_file::HHHB_HEAD_HIDDEN;

	return MDB_file::HHHB_NONE_HIDDEN;
}

static void print_helm(const MDB_file::Helm& helm)
{	
	cout << "  Hiding: " << helm.header.hiding_behavior;
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

	cout << "  Position: ";
	print_vector3(helm.header.position);

	cout << "  Orientation:\n";
	print_orientation(helm.header.orientation);
}

void import_helm(MDB_file& mdb, FbxNode* node)
{
	cout << "Exporting HELM: " << node->GetName() << endl;

	auto helm = make_unique<MDB_file::Helm>();
	set_packet_name(helm->header.name, node->GetName());
	helm->header.hiding_behavior = helm_hair_hiding_behavior(node);

	auto m = node->EvaluateGlobalTransform();

	auto translation = m.GetT();
	helm->header.position.x = float(translation[0] / 100);
	helm->header.position.y = float(-translation[2] / 100);
	helm->header.position.z = float(translation[1] / 100);

	transform_to_orientation(m, helm->header.orientation);

	print_helm(*helm);

	mdb.add_packet(move(helm));
}

void import_polygon(MDB_file::Rigid_mesh& rigid_mesh, FbxMesh* mesh,
	int polygon_index)
{
	if(mesh->GetPolygonSize(polygon_index) != 3) {
		Log::error() << "Polygon is not a triangle.\n";
		return;
	}

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

	cout << "  Vertices: " << mesh->GetControlPointsCount() << endl;
	cout << "  Polygons: " << mesh->GetPolygonCount() << endl;
}

bool validate_rigid_mesh(FbxMesh* mesh)
{
	if (mesh->GetElementUVCount() == 0) {
		Log::error() << "There is no UV information.\n";
		return false;
	}
	else if (mesh->GetElementNormalCount() == 0) {
		Log::error() << "There is no normal vector information.\n";
		return false;
	}
	else if (mesh->GetElementTangentCount() == 0) {
		Log::error() << "There is no tangent vector information.\n";
		return false;
	}
	else if (mesh->GetElementBinormalCount() == 0) {
		Log::error() << "There is no binormal vector information.\n";
		return false;
	}

	return true;
}

static void import_user_property(FbxNode* node, MDB_file::Material& material,
                                 const char* name1, const char* name2,
                                 MDB_file::Material_flags flag)
{
	auto prop = node->FindProperty(name1, false);

	if (!prop.IsValid())
		prop = node->FindProperty(name2, false);

	if (prop.IsValid())
		material.flags |= prop.Get<float>() == 0 ? 0 : flag;
}

void import_user_properties(FbxNode* node, MDB_file::Material& material)
{
	import_user_property(node, material, "NWN2MDK_TRANSPARENCY_MASK",
	                     "TRANSPARENCY_MASK", MDB_file::ALPHA_TEST);
	import_user_property(node, material, "NWN2MDK_ENVIRONMENT_MAP",
	                     "ENVIRONMENT_MAP", MDB_file::ENVIRONMENT_MAPPING);
	import_user_property(node, material, "NWN2MDK_HEAD", "HEAD",
	                     MDB_file::CUTSCENE_MESH);
	import_user_property(node, material, "NWN2MDK_GLOW", "GLOW",
	                     MDB_file::GLOW);
	import_user_property(node, material, "NWN2MDK_DONT_CAST_SHADOWS",
	                     "DONT_CAST_SHADOWS", MDB_file::CAST_NO_SHADOWS);
	import_user_property(node, material, "NWN2MDK_PROJECTED_TEXTURES",
	                     "PROJECTED_TEXTURES",
	                     MDB_file::PROJECTED_TEXTURES);
}

void import_rigid_mesh(MDB_file& mdb, FbxNode* node)
{
	auto mesh = node->GetMesh();

	if (!mesh)
		return;

	cout << "Exporting RIGD: " << node->GetName() << endl;

	print_mesh(mesh);

	if (!validate_rigid_mesh(mesh))
		return;

	auto rigid_mesh = make_unique<MDB_file::Rigid_mesh>();
	set_packet_name(rigid_mesh->header.name, node->GetName());

	import_material(rigid_mesh->header.material, mesh);

	for(int i = 0; i < mesh->GetPolygonCount(); ++i)
		import_polygon(*rigid_mesh.get(), mesh, i);

	import_user_properties(node, rigid_mesh->header.material);

	mdb.add_packet(move(rigid_mesh));
}

FbxNode* skeleton_node(FbxNode* node)
{	
	if (!node || node == node->GetScene()->GetRootNode() ||
		node->GetParent() == node->GetScene()->GetRootNode())
		return nullptr;

	while (node->GetParent() != node->GetScene()->GetRootNode())
		node = node->GetParent();

	return node;
}

FbxNode* skeleton_node(FbxMesh* mesh)
{
	auto s = skin(mesh);

	if (!s)
		return nullptr;

	auto cluster = s->GetCluster(0);

	return skeleton_node(cluster->GetLink());
}

void import_polygon(MDB_file::Skin& skin, Fbx_bones& fbx_bones, FbxMesh* mesh,
	int polygon_index)
{
	if (mesh->GetPolygonSize(polygon_index) != 3) {
		Log::error() << "Polygon is not a triangle.\n";
		return;
	}

	MDB_file::Skin_vertex poly_vertices[3];
	import_positions(mesh, polygon_index, poly_vertices);
	import_normals(mesh, polygon_index, poly_vertices);
	import_tangents(mesh, polygon_index, poly_vertices);
	import_binormals(mesh, polygon_index, poly_vertices);
	import_uv(mesh, polygon_index, poly_vertices);
	import_skinning(mesh, polygon_index, fbx_bones, poly_vertices);

	MDB_file::Face face;

	for (int i = 0; i < 3; ++i)
		face.vertex_indices[i] =
		push_vertex(skin, poly_vertices[i]);

	skin.faces.push_back(face);
}

void gather_fbx_bones(FbxNode* node, Fbx_bones& fbx_bones)
{
	fbx_bones.bones.push_back(node);

	if (strncmp(node->GetName(), "ap_", 3) == 0) {
		// Discard "ap_..." bones as they are not used in skinning.		
	}
	else if (strncmp(node->GetName(), "f_", 2) == 0) {
		fbx_bones.face_bones.push_back(node);
	}
	else if (strcmp(node->GetName(), "Ribcage") == 0) {		
		fbx_bones.ribcage = node;		
	}
	else {
		fbx_bones.body_bones.push_back(node);
	}

	for (int i = 0; i < node->GetChildCount(); ++i)
		gather_fbx_bones(node->GetChild(i), fbx_bones);
}

void print_vertices(MDB_file::Skin& skin)
{
	for (auto& v : skin.verts) {
		cout << "  Vertex weights:";

		for (int i = 0; i < 4; ++i)
			cout << ' ' << v.bone_weights[i];

		cout << '\n';
	}
}

void import_skin(MDB_file& mdb, FbxNode* node)
{
	auto mesh = node->GetMesh();

	if (!mesh)
		return;

	cout << "Exporting SKIN: " << node->GetName() << endl;

	print_mesh(mesh);

	if (!validate_rigid_mesh(mesh))
		return;

	auto skin = make_unique<MDB_file::Skin>();
	set_packet_name(skin->header.name, node->GetName());
	auto skel_node = skeleton_node(mesh);
	cout << "  Skeleton name: " << skel_node->GetName() << endl;

	strncpy(skin->header.skeleton_name, skel_node->GetName(), 32);	

	import_material(skin->header.material, mesh);

	Fbx_bones fbx_bones;
	gather_fbx_bones(skel_node->GetChild(0), fbx_bones);

	for (int i = 0; i < mesh->GetPolygonCount(); ++i)
		import_polygon(*skin.get(), fbx_bones, mesh, i);

	import_user_properties(node, skin->header.material);

	print_vertices(*skin);

	mdb.add_packet(move(skin));
}

void import_meshes(MDB_file& mdb, FbxNode* node)
{
	if (ends_with(node->GetName(), "_C2"))
		import_collision_mesh(mdb, node);
	else if (ends_with(node->GetName(), "_C3"))
		import_collision_mesh(mdb, node);
	else if (is_walk_mesh(node))
		import_walk_mesh(mdb, node);
	else if (is_hook_packet(node))
		import_hook_point(mdb, node);
	else if (is_hair_packet(node))
		import_hair(mdb, node);
	else if (is_helm_packet(node))
		import_helm(mdb, node);
	else if (skin(node))
		import_skin(mdb, node);
	else if (!starts_with(node->GetName(), "COLS"))
		import_rigid_mesh(mdb, node);

	for (int i = 0; i < node->GetChildCount(); ++i)
		import_meshes(mdb, node->GetChild(i));
}

void import_meshes(MDB_file& mdb, FbxScene* scene)
{	
	import_meshes(mdb, scene->GetRootNode());
}

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
	Virtual_ptr<GR2_animation> animations[1];
	Vector3<double> bone_scaling;
	std::list<GR2_skeleton> skeletons;
	std::list<std::vector<GR2_bone>> bone_arrays;
	std::vector<Virtual_ptr<GR2_skeleton>> skeleton_pointers;
	std::list<GR2_model> models;
	std::vector<Virtual_ptr<GR2_model>> model_pointers;
	std::vector<GR2_track_group_info> track_groups;
	std::list<GR2_curve_data_D3Constant32f> d3c_curves;
	std::list<GR2_curve_data_DaConstant32f> dac_curves;
	std::list<GR2_curve_data_DaK32fC32f> da_curves;
	std::list<GR2_curve_data_DaIdentity> id_curves;
	std::list<std::vector<float>> float_arrays;
	String_collection strings;
	std::vector<Virtual_ptr<GR2_track_group>> track_group_pointers;
};

void init_file_info(GR2_file_info& file_info)
{
	file_info.textures_count = 0;
	file_info.materials_count = 0;
	file_info.skeletons_count = 0;
	file_info.vertex_datas_count = 0;
	file_info.tri_topologies_count = 0;
	file_info.meshes_count = 0;
	file_info.models_count = 0;
	file_info.track_groups_count = 0;
	file_info.animations_count = 0;
	file_info.extended_data.keys = nullptr;
	file_info.extended_data.values = nullptr;
}

void import_art_tool_info(GR2_import_info& import_info)
{
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
}

void import_exporter_info(GR2_import_info& import_info)
{
	import_info.exporter_info.exporter_name =
		import_info.strings.get("NWN2 MDK 0.14");
	import_info.exporter_info.exporter_major_revision = 2;
	import_info.exporter_info.exporter_minor_revision = 6;
	import_info.exporter_info.exporter_customization = 0;
	import_info.exporter_info.exporter_build_number = 10;
	import_info.file_info.exporter_info = &import_info.exporter_info;
}

bool is_pivot_node(FbxNode* node)
{
	return ends_with(node->GetName(), ".PIVOT");
}

bool is_skeleton(FbxNode* node)
{
	// A skeleton should have atleast a bone.
	if (node->GetChildCount() == 0)
		return false;
	
	return has_skeleton_attribute(node->GetChild(0))
		|| is_pivot_node(node);
}

bool is_facial_bone(FbxNode* node)
{
	return starts_with(node->GetName(), "f_");
}

bool is_attachment_point(FbxNode* node)
{
	return starts_with(node->GetName(), "ap_");
}

bool is_rendering_bone(FbxNode *node)
{
	return has_skeleton_attribute(node) && !is_facial_bone(node)
		&& !is_attachment_point(node);
}

int rendering_bone_count(FbxNode* node)
{
	int c = 0;

	if (is_rendering_bone(node))
		c++;

	for (int i = 0; i < node->GetChildCount(); ++i)
		c += rendering_bone_count(node->GetChild(i));

	return c;
}

bool validate_skeleton(FbxNode* node)
{
	if (is_pivot_node(node)) {
		if (node->GetChildCount() > 1) {
			Log::error() << "PIVOT has more than one child.\n";
			return false;
		}
	}
	else {
		if (node->GetChildCount() == 0) {
			Log::error() << "Skeleton has no root bone.\n";
			return false;
		}
		else if (node->GetChildCount() > 1) {
			Log::error() << "Skeleton has more than one root bone.\n";
			return false;
		}
		else if (strcmp(node->GetName(), node->GetChild(0)->GetName()) != 0) {
			Log::error() << "Skeleton name is not equal to root bone name: " <<
				node->GetName() << " != " << node->GetChild(0)->GetName() << '\n';
			return false;
		}
		else if (rendering_bone_count(node) > 54) {
			Log::error() << "Skeleton has more than 54 render bones.\n";
			return false;
		}
	}

	return true;
}

void print_bone(GR2_bone& bone)
{
	cout << "    Translation: " << bone.transform.translation.x;
	cout << ' ' << bone.transform.translation.y;
	cout << ' ' << bone.transform.translation.z << endl;

	cout << "    Rotation: " << bone.transform.rotation.x;
	cout << ' ' << bone.transform.rotation.y;
	cout << ' ' << bone.transform.rotation.z;
	cout << ' ' << bone.transform.rotation.w << endl;

	cout << "    Scale: " << bone.transform.scale_shear[0];
	cout << ' ' << bone.transform.scale_shear[4];
	cout << ' ' << bone.transform.scale_shear[8] << endl;

	cout << "    Inverse World Transform:\n";
	for (int row = 0; row < 4; ++row) {
		cout << "        ";
		for (int col = 0; col < 4; ++col) {
			cout << ' ' << bone.inverse_world_transform[col * 4 + row];
		}
		cout << endl;
	}
}

void import_bone_translation(FbxNode* node, GR2_bone& bone)
{
	bone.transform.translation.x = float(node->LclTranslation.Get()[0]);
	bone.transform.translation.y = float(node->LclTranslation.Get()[1]);
	bone.transform.translation.z = float(node->LclTranslation.Get()[2]);

	if (bone.transform.translation.x != 0 || bone.transform.translation.y != 0 || bone.transform.translation.z != 0)
		bone.transform.flags |= GR2_has_position;
}

void import_bone_rotation(FbxNode* node, GR2_bone& bone)
{
	auto rotation = euler_to_quaternion(node->LclRotation.Get());
	bone.transform.rotation.x = float(rotation[0]);
	bone.transform.rotation.y = float(rotation[1]);
	bone.transform.rotation.z = float(rotation[2]);
	bone.transform.rotation.w = float(rotation[3]);

	if (bone.transform.rotation.x != 0 || bone.transform.rotation.y != 0 || bone.transform.rotation.z != 0 || bone.transform.rotation.w != 1)
		bone.transform.flags |= GR2_has_rotation;
}

void import_bone_scale(FbxNode* node, GR2_bone& bone)
{
	bone.transform.scale_shear[0] = float(node->LclScaling.Get()[0]);
	bone.transform.scale_shear[1] = 0;
	bone.transform.scale_shear[2] = 0;
	bone.transform.scale_shear[3] = 0;
	bone.transform.scale_shear[4] = float(node->LclScaling.Get()[1]);
	bone.transform.scale_shear[5] = 0;
	bone.transform.scale_shear[6] = 0;
	bone.transform.scale_shear[7] = 0;
	bone.transform.scale_shear[8] = float(node->LclScaling.Get()[2]);

	if (bone.transform.scale_shear[0] != 1 || bone.transform.scale_shear[4] != 1 || bone.transform.scale_shear[8] != 1)
		bone.transform.flags |= GR2_has_scale_shear;
}

void import_bone_transform(FbxNode* node, GR2_bone& bone)
{
	bone.transform.flags = 0;
	import_bone_translation(node, bone);
	import_bone_rotation(node, bone);
	import_bone_scale(node, bone);
}

void import_bone_inv_world_transform(FbxNode* node, GR2_bone& bone)
{	
	auto inv_world_transform = node->EvaluateGlobalTransform(FBXSDK_TIME_INFINITE, FbxNode::eSourcePivot, false, true).Inverse();
	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			bone.inverse_world_transform[row * 4 + col] = float(inv_world_transform.Get(row, col));
		}
	}
}

void import_bone(GR2_import_info& import_info, FbxNode* node, int32_t parent_index, std::vector<GR2_bone>& bones)
{
	cout << "  Exporting bone: " << node->GetName() << endl;

	auto translation = node->LclTranslation.Get();
	translation[0] *= import_info.bone_scaling.x;
	translation[1] *= import_info.bone_scaling.y;
	translation[2] *= import_info.bone_scaling.z;
	node->LclTranslation.Set(translation);

	GR2_bone bone;
	bone.name = import_info.strings.get(node->GetName());
	bone.parent_index = parent_index;
	import_bone_transform(node, bone);
	import_bone_inv_world_transform(node, bone);
	bone.light_info = nullptr;
	bone.camera_info = nullptr;
	bone.extended_data.keys = nullptr;
	bone.extended_data.values = nullptr;
	bones.push_back(bone);

	print_bone(bone);
}

void import_bones(GR2_import_info& import_info, FbxNode* node, int32_t parent_index, std::vector<GR2_bone>& bones)
{
	for (int i = 0; i < node->GetChildCount(); ++i) {
		import_bone(import_info, node->GetChild(i), parent_index, bones);
		import_bones(import_info, node->GetChild(i), bones.size() - 1, bones);
	}
}

void import_model(GR2_import_info& import_info, GR2_skeleton* skel)
{
	GR2_model model;
	model.name = skel->name;
	model.skeleton = skel;
	model.initial_placement.flags = 0;
	model.initial_placement.translation = Vector3<float>(0, 0, 0);
	model.initial_placement.rotation = Vector4<float>(0, 0, 0, 1);
	model.initial_placement.scale_shear[0] = 1;
	model.initial_placement.scale_shear[1] = 0;
	model.initial_placement.scale_shear[2] = 0;
	model.initial_placement.scale_shear[3] = 0;
	model.initial_placement.scale_shear[4] = 1;
	model.initial_placement.scale_shear[5] = 0;
	model.initial_placement.scale_shear[6] = 0;
	model.initial_placement.scale_shear[7] = 0;
	model.initial_placement.scale_shear[8] = 1;
	model.mesh_bindings_count = 0;
	model.mesh_bindings = nullptr;
	import_info.models.push_back(model);
	import_info.model_pointers.push_back(&import_info.models.back());
}

void import_skeleton(GR2_import_info& import_info, FbxNode* node)
{
	cout << "Exporting skeleton: " << node->GetName() << endl;

	if (!validate_skeleton(node))
		return;

	import_info.bone_scaling.x = node->LclScaling.Get()[0];
	import_info.bone_scaling.y = node->LclScaling.Get()[1];
	import_info.bone_scaling.z = node->LclScaling.Get()[2];

	node->LclTranslation.Set(FbxDouble3(0, 0, 0));
	node->LclRotation.Set(FbxDouble3(0, 0, 0));
	node->LclScaling.Set(FbxDouble3(1, 1, 1));

	import_info.bone_arrays.emplace_back();
	import_bones(import_info, node, -1, import_info.bone_arrays.back());

	GR2_skeleton skel;
	skel.name = import_info.strings.get(path(node->GetName()).stem().string().c_str());
	skel.bones_count = import_info.bone_arrays.back().size();
	skel.bones = import_info.bone_arrays.back().data();
	import_info.skeletons.push_back(skel);
	import_info.skeleton_pointers.push_back(&import_info.skeletons.back());

	import_model(import_info, &import_info.skeletons.back());	
}

void import_skeletons(FbxScene* scene, const Import_info& info)
{
	auto old_error_count = Log::error_count;

	GR2_import_info import_info;

	init_file_info(import_info.file_info);
	import_art_tool_info(import_info);
	import_exporter_info(import_info);
	import_info.file_info.from_file_name =
		import_info.strings.get(path(info.input_path).filename().string().c_str());

	for (int i = 0; i < scene->GetRootNode()->GetChildCount(); ++i) {
		auto node = scene->GetRootNode()->GetChild(i);
		if (is_skeleton(node))
			import_skeleton(import_info, node);
	}

	if (import_info.skeletons.empty())
		return;

	import_info.file_info.skeletons_count = import_info.skeleton_pointers.size();
	import_info.file_info.skeletons = import_info.skeleton_pointers.data();

	import_info.file_info.models_count = import_info.model_pointers.size();
	import_info.file_info.models = import_info.model_pointers.data();

	if (Log::error_count > old_error_count) {
		Log::error() << "skel.gr2 not generated due to errors found during the conversion.\n";
	}
	else {
		GR2_file gr2;
		gr2.read(&import_info.file_info);
		string output_filename = string(info.output_path) + ".gr2";
		gr2.write(output_filename.c_str());
		cout << "\nOutput is " << output_filename << endl;
	}
}

GR2_track_group_info &track_group(GR2_import_info& import_info, const char *name)
{
	// Find track group
	for(auto &tg : import_info.track_groups)
		if(strcmp(tg.track_group.name, name) == 0)
			return tg;
	
	auto& tg = import_info.track_groups.emplace_back();
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

void import_position_DaK32fC32f(GR2_import_info& import_info, FbxNode* node,
	GR2_transform_track& tt)
{
	cout << "  Positions:\n";

	tt.position_curve.keys = DaK32fC32f_def;

	auto& curve = import_info.da_curves.emplace_back();
	curve.curve_data_header_DaK32fC32f.format = DaK32fC32f;
	curve.curve_data_header_DaK32fC32f.degree = 1;

	auto& knots = import_info.float_arrays.emplace_back();
	auto& controls = import_info.float_arrays.emplace_back();

	FbxTime time = import_info.anim_stack->LocalStart;
	FbxTime dt;
	dt.SetSecondDouble(time_step);

	while (time <= import_info.anim_stack->LocalStop) {
		float t = float((time - import_info.anim_stack->LocalStart).GetSecondDouble());
		knots.push_back(t);

		auto p = node->LclTranslation.EvaluateValue(time);
		controls.push_back(float(p[0] * import_info.bone_scaling.x));
		controls.push_back(float(p[1] * import_info.bone_scaling.y));
		controls.push_back(float(p[2] * import_info.bone_scaling.z));

		cout << "    " << knots.back() << ": " << p[0] << ' '
			<< p[1] << ' ' << p[2] << endl;

		time += dt;
	}

	curve.knots_count = knots.size();
	curve.knots = knots.data();
	curve.controls_count = controls.size();
	curve.controls = controls.data();

	tt.position_curve.curve_data = reinterpret_cast<GR2_curve_data*>(&curve);
}

void import_position_D3Constant32f(GR2_import_info& import_info, FbxNode* node,
	GR2_transform_track& tt)
{
	tt.position_curve.keys = D3Constant32f_def;

	auto& curve = import_info.d3c_curves.emplace_back();
	curve.curve_data_header_D3Constant32f.format = D3Constant32f;
	curve.curve_data_header_D3Constant32f.degree = 0;
	curve.controls[0] = float(node->LclTranslation.Get()[0] * import_info.bone_scaling.x);
	curve.controls[1] = float(node->LclTranslation.Get()[1] * import_info.bone_scaling.y);
	curve.controls[2] = float(node->LclTranslation.Get()[2] * import_info.bone_scaling.z);
	tt.position_curve.curve_data =
		reinterpret_cast<GR2_curve_data*>(&curve);
}

void import_position_anim(GR2_import_info& import_info, FbxNode* node, 
	FbxAnimLayer* layer, GR2_transform_track& tt)
{	
	if (node->LclTranslation.GetCurveNode(layer)) {
		import_position_DaK32fC32f(import_info, node, tt);
	}
	else {
		import_position_D3Constant32f(import_info, node, tt);
	}
}

void import_rotation_anim(GR2_import_info& import_info, FbxNode* node,
	FbxAnimLayer* layer, GR2_transform_track& tt)
{
	cout << "  Rotations:\n";

	tt.orientation_curve.keys = DaK32fC32f_def;

	auto& curve = import_info.da_curves.emplace_back();
	curve.curve_data_header_DaK32fC32f.format = DaK32fC32f;
	curve.curve_data_header_DaK32fC32f.degree = 1;

	auto& knots = import_info.float_arrays.emplace_back();
	auto& controls = import_info.float_arrays.emplace_back();

	FbxTime time = import_info.anim_stack->LocalStart;
	FbxTime dt;
	dt.SetSecondDouble(time_step);

	FbxQuaternion prev_quat(0, 0, 0, 1);

	while (time <= import_info.anim_stack->LocalStop) {
		float t = float((time - import_info.anim_stack->LocalStart).GetSecondDouble());
		knots.push_back(t);

		auto p = node->LclRotation.EvaluateValue(time);
		
		auto quat = euler_to_quaternion(p);		

		// GR2 animation requires two consecutive quaternions have the
		// shortest path.
		if (quat.DotProduct(prev_quat) < 0)
			quat = -quat;

		controls.push_back(float(quat[0]));
		controls.push_back(float(quat[1]));
		controls.push_back(float(quat[2]));
		controls.push_back(float(quat[3]));
		prev_quat = quat;

		cout << "    " << knots.back() << ": ";
		cout << '[' << p[0] << ' ' << p[1] << ' ' << p[2] << "] "<< quat[0] << ' '
		     << quat[1] << ' ' << quat[2] << ' ' << quat[3] << endl;

		time += dt;
	}

	curve.knots_count = knots.size();
	curve.knots = knots.data();
	curve.controls_count = controls.size();
	curve.controls = controls.data();

	tt.orientation_curve.curve_data = reinterpret_cast<GR2_curve_data*>(&curve);
}

void import_scaleshear_DaK32fC32f(GR2_import_info& import_info, FbxNode* node,
	GR2_transform_track& tt)
{
	cout << "  Scaling:\n";

	auto& knots = import_info.float_arrays.emplace_back();
	auto& controls = import_info.float_arrays.emplace_back();

	FbxTime time = import_info.anim_stack->LocalStart;
	FbxTime dt;
	dt.SetSecondDouble(time_step);

	while (time <= import_info.anim_stack->LocalStop) {
		float t = float((time - import_info.anim_stack->LocalStart).GetSecondDouble());
		knots.push_back(t);

		auto s = node->LclScaling.EvaluateValue(time);
		controls.push_back(float(s[0]));
		controls.push_back(0);
		controls.push_back(0);
		controls.push_back(0);
		controls.push_back(float(s[1]));
		controls.push_back(0);
		controls.push_back(0);
		controls.push_back(0);
		controls.push_back(float(s[2]));

		cout << "    " << knots.back() << ": " << s[0] << ' '
			<< s[1] << ' ' << s[2] << endl;

		time += dt;		
	}

	auto& curve = import_info.da_curves.emplace_back();
	curve.curve_data_header_DaK32fC32f.format = DaK32fC32f;
	curve.curve_data_header_DaK32fC32f.degree = 1;
	curve.padding = 0;
	curve.knots_count = knots.size();
	curve.knots = knots.data();
	curve.controls_count = controls.size();
	curve.controls = controls.data();

	tt.scale_shear_curve.keys = DaK32fC32f_def;
	tt.scale_shear_curve.curve_data = reinterpret_cast<GR2_curve_data*>(&curve);
}

void import_scaleshear_DaConstant32f(GR2_import_info& import_info, FbxNode* node,
	GR2_transform_track& tt)
{
	auto& controls = import_info.float_arrays.emplace_back();
	controls.push_back(float(node->LclScaling.Get()[0]));
	controls.push_back(0);
	controls.push_back(0);
	controls.push_back(0);
	controls.push_back(float(node->LclScaling.Get()[1]));
	controls.push_back(0);
	controls.push_back(0);
	controls.push_back(0);
	controls.push_back(float(node->LclScaling.Get()[2]));

	auto& curve = import_info.dac_curves.emplace_back();
	curve.curve_data_header_DaConstant32f.format = DaConstant32f;
	curve.curve_data_header_DaConstant32f.degree = 0;
	curve.padding = 0;
	curve.controls_count = controls.size();
	curve.controls = controls.data();

	tt.scale_shear_curve.keys = DaConstant32f_def;
	tt.scale_shear_curve.curve_data = reinterpret_cast<GR2_curve_data*>(&curve);
}

void import_scaleshear_DaIdentity(GR2_import_info& import_info, GR2_transform_track& tt)
{
	import_info.id_curves.emplace_back();
	auto& curve = import_info.id_curves.back();
	curve.curve_data_header_DaIdentity.format = DaIdentity;
	curve.curve_data_header_DaIdentity.degree = 0;
	curve.dimension = 9;
	tt.scale_shear_curve.keys = DaIdentity_def;
	tt.scale_shear_curve.curve_data = reinterpret_cast<GR2_curve_data*>(&curve);
}

void import_scaleshear(GR2_import_info& import_info, FbxNode* node,
	FbxAnimLayer* layer, GR2_transform_track& tt)
{
	if (node->LclScaling.GetCurveNode(layer))
		import_scaleshear_DaK32fC32f(import_info, node, tt);
	else if (node->LclScaling.Get()[0] != 1 || node->LclScaling.Get()[1] != 1 || node->LclScaling.Get()[2] != 1)
		import_scaleshear_DaConstant32f(import_info, node, tt);
	else
		import_scaleshear_DaIdentity(import_info, tt);
}

void import_anim_layer(GR2_import_info& import_info, FbxAnimLayer* layer,
                       FbxNode* node)
{
	cout << "Exporting animation: " << node->GetName() << endl;
	
	auto skel_node = skeleton_node(node);

	if(skel_node && (has_skeleton_attribute(node) || is_pivot_node(skel_node))) {
		import_info.bone_scaling.x = skel_node->LclScaling.Get()[0];
		import_info.bone_scaling.y = skel_node->LclScaling.Get()[1];
		import_info.bone_scaling.z = skel_node->LclScaling.Get()[2];

		auto &tg = track_group(import_info, path(skel_node->GetName()).stem().string().c_str());

		auto& tt = tg.transform_tracks.emplace_back();
		tt.name = import_info.strings.get(node->GetName());

		import_position_anim(import_info, node, layer, tt);
		import_rotation_anim(import_info, node, layer, tt);
		import_scaleshear(import_info, node, layer, tt);
	}

	for(int i = 0; i < node->GetChildCount(); ++i)
		import_anim_layer(import_info, layer, node->GetChild(i));
}

void import_anim_layer(GR2_import_info& import_info, FbxAnimLayer* layer)
{
	cout << "    - Name: " << layer->GetName() << endl;
	cout << "      Animation curve nodes: " << layer->GetMemberCount<FbxAnimCurveNode>() << endl;

	import_anim_layer(import_info, layer, layer->GetScene()->GetRootNode());
}

void import_animation(FbxAnimStack *stack, const Import_info& info)
{
	auto old_error_count = Log::error_count;

	GR2_import_info import_info;

	import_info.anim_stack = stack;

	init_file_info(import_info.file_info);
	import_art_tool_info(import_info);
	import_exporter_info(import_info);
	import_info.file_info.from_file_name =
		import_info.strings.get(path(info.input_path).filename().string().c_str());

	cout << "  Animation layers: " << stack->GetMemberCount<FbxAnimLayer>() << endl;
	for(int i = 0; i < stack->GetMemberCount<FbxAnimLayer>(); ++i) {
		import_anim_layer(import_info,
		                  stack->GetMember<FbxAnimLayer>(i));
	}

	for(auto &tg : import_info.track_groups) {
		// GR2 animation requires transform tracks sorted by name.
		sort(tg.transform_tracks.begin(), tg.transform_tracks.end(), 
			[](GR2_transform_track &t1, GR2_transform_track &t2) {
			return strcmp(t1.name, t2.name) < 0;
		});		
		tg.track_group.transform_tracks_count = tg.transform_tracks.size();		
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

	if (Log::error_count > old_error_count) {
		Log::error() << "anim.gr2 not generated due to errors found during the conversion.\n";
	}
	else {
		GR2_file gr2;
		gr2.read(&import_info.file_info);
		string output_filename = string(info.output_path) + ".gr2";
		gr2.write(output_filename.c_str());
		cout << "\nOutput is " << output_filename << endl;
	}
}

void import_animations(FbxScene* scene, const Import_info& import_info)
{
	cout << "\nAnimation stacks: " << scene->GetSrcObjectCount<FbxAnimStack>() << endl;
	for(int i = 0; i < scene->GetSrcObjectCount<FbxAnimStack>(); ++i)
		import_animation(scene->GetSrcObject<FbxAnimStack>(i), import_info);
}

struct Bone_info {
	uint32_t index;
	FbxVector4 translation;
};

uint32_t gather_bone_info(FbxNode* node, uint32_t bone_index, std::vector<Bone_info>& bone_infos)
{
	auto attr = node->GetNodeAttribute();
	if (attr && attr->GetAttributeType() == FbxNodeAttribute::eSkeleton) {
		Bone_info info;
		info.index = bone_index;
		info.translation = node->EvaluateGlobalTransform().GetT();
		bone_infos.push_back(info);
		++bone_index;
	}

	for (int i = 0; i < node->GetChildCount(); ++i) {
		if (node == node->GetScene()->GetRootNode())
			bone_index = 0; // Reset index

		bone_index = gather_bone_info(node->GetChild(i), bone_index, bone_infos);
	}

	return bone_index;
}

// Returns the nearest bone index from a given position.
uint32_t nearest_bone_index(const std::vector<Bone_info>& bone_infos,
                            const FbxVector4& position)
{
	double min_distance = 1e6;
	uint32_t bone_index = 0;

	for (auto& bone_info : bone_infos) {
		double d = bone_info.translation.Distance(position);
		if (d < min_distance) {
			min_distance = d;
			bone_index = bone_info.index;
		}
	}

	return bone_index;
}

float sphere_radius(FbxNode *node)
{
	auto mesh = node->GetMesh();

	if (!mesh || mesh->GetControlPointsCount() == 0)
		return 0;

	// For collision spheres, we assume the mesh is a sphere with origin at
	// the center, so the radius is the length of any vertex.
	return float(mesh->GetControlPointAt(0).Length());
}

static void import_collision_sphere(MDB_file::Collision_spheres& cs,
                                    FbxNode* node,
                                    const std::vector<Bone_info>& bone_infos)
{
	auto mesh = node->GetMesh();

	if (!mesh)
		return;

	cout << "Exporting COLS: " << node->GetName() << endl;

	MDB_file::Collision_sphere s;
	s.bone_index = nearest_bone_index(bone_infos, node->EvaluateGlobalTransform().GetT());
	s.radius = sphere_radius(node);
	cs.spheres.push_back(s);
}

static void import_collision_spheres(MDB_file::Collision_spheres& cs,
                                     FbxNode* node,
                                     const vector<Bone_info>& bone_infos)
{
	for (int i = 0; i < node->GetChildCount(); ++i) {
		auto child = node->GetChild(i);

		if (starts_with(child->GetName(), "COLS"))
			import_collision_sphere(cs, child, bone_infos);

		import_collision_spheres(cs, child, bone_infos);
	}
}

static void import_collision_spheres(MDB_file& mdb, FbxScene* scene)
{
	vector<Bone_info> bone_infos;
	gather_bone_info(scene->GetRootNode(), 0, bone_infos);

	auto cs = make_unique<MDB_file::Collision_spheres>();
	import_collision_spheres(*cs, scene->GetRootNode(), bone_infos);

	sort(cs->spheres.begin(), cs->spheres.end(),
		[](MDB_file::Collision_sphere &s0, MDB_file::Collision_sphere &s1) {
		return s0.bone_index < s1.bone_index;
	});

	cs->header.sphere_count = cs->spheres.size();

	if (cs->header.sphere_count > 0)
		mdb.add_packet(move(cs));
}

void import_models(FbxScene* scene, const char* filename)
{
	auto old_error_count = Log::error_count;

	MDB_file mdb;

	import_meshes(mdb, scene);
	import_collision_spheres(mdb, scene);

	if (Log::error_count > old_error_count) {
		Log::error() << "MDB not generated due to errors found during the conversion.\n";
	}
	else if (mdb.packet_count() > 0) {
		string output_filename = string(filename) + ".mdb";
		mdb.save(output_filename.c_str());
		cout << "\nOutput is " << output_filename << endl;
	}
}

void import_scene(FbxScene* scene, const Import_info& import_info)
{
	if (import_info.output_type == Output_type::mdb ||
	    import_info.output_type == Output_type::any)
		import_models(scene, import_info.output_path.c_str());

	if (import_info.output_type == Output_type::mdb)
		return;

	if (scene->GetSrcObjectCount<FbxAnimStack>() == 0)
		import_skeletons(scene, import_info);
	else
		import_animations(scene, import_info);
}

bool import_fbx(const Import_info& import_info)
{
	auto manager = FbxManager::Create();
	if (!manager) {
		Log::error() << "Unable to create FBX manager\n";
		return false;
	}

	// Create an IOSettings object. This object holds all import/export
	// settings.
	auto ios = FbxIOSettings::Create(manager, IOSROOT);
	manager->SetIOSettings(ios);

	if (!exists(import_info.input_path)) {
		Log::error() << import_info.input_path << " not found\n";
		return false;
	}

	// Create an importer.
	auto importer = FbxImporter::Create(manager, "");
	if (!importer->Initialize(import_info.input_path.c_str(), -1,
	                          manager->GetIOSettings())) {
		Log::error() << importer->GetStatus().GetErrorString() << endl;
		return false;
	}

	// Create a new scene so it can be populated by the imported file.
	FbxScene* scene = FbxScene::Create(manager, "");

	// Import the contents of the file into the scene.
	importer->Import(scene);

	// The file has been imported; we can get rid of the importer.
	importer->Destroy();

	import_scene(scene, import_info);

	// Destroy the sdk manager and all other objects it was handling.
	manager->Destroy();

	return true;
}

int main(int argc, char* argv[])
{
	Redirect_output_handle redirect_output_handle;

	Config config((path(argv[0]).parent_path() / "config.yml").string().c_str());

	if (config.nwn2_home.empty())
		return 1;

	GR2_file::granny2dll_filename = config.nwn2_home + "\\granny2.dll";

	if(argc < 2) {
		cout << "Usage: fbx2nw <file>\n";
		return 1;
	}

	Import_info import_info;

	if (!parse_args(argc, argv, import_info))
			return 1;

	if (!import_fbx(import_info))
		return 1;	

	return Log::error_count == 0 ? 0 : 1;
}
