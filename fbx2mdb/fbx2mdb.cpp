#include <iostream>
#include <experimental/filesystem>

#include "fbxsdk.h"
#include "mdb_file.h"

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

template <typename T>
void import_positions(FbxMesh* mesh, int polygon_index, T* poly_vertices)
{
	for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
		int index = mesh->GetPolygonVertex(polygon_index, i);
		FbxVector4 p = mesh->GetControlPointAt(index);
		poly_vertices[i].position.x = p[0];
		poly_vertices[i].position.y = p[1];
		poly_vertices[i].position.z = p[2];
	}
}

template <typename T>
void import_normals(FbxMesh* mesh, int polygon_index, T* poly_vertices)
{
	for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
		FbxVector4 normal;
		mesh->GetPolygonVertexNormal(polygon_index, i, normal);
		poly_vertices[i].normal.x = normal[0];
		poly_vertices[i].normal.y = normal[1];
		poly_vertices[i].normal.z = normal[2];
	}
}

void import_tangents(FbxMesh* mesh, int polygon_index,
                     MDB_file::Rigid_mesh_vertex* poly_vertices)
{
	if(mesh->GetElementTangentCount() <= 0)
		return;

	auto *e = mesh->GetElementTangent(0);
	switch(e->GetMappingMode()) {
	case FbxGeometryElement::eByPolygonVertex:
		for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
			int index = mesh->GetPolygonVertexIndex(polygon_index) + i;
			FbxVector4 v = e->GetDirectArray().GetAt(index);
			poly_vertices[i].tangent.x = v[0];
			poly_vertices[i].tangent.y = v[1];
			poly_vertices[i].tangent.z = v[2];
		}
		break;
	case FbxGeometryElement::eByControlPoint:
		for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
			int index = mesh->GetPolygonVertex(polygon_index, i);
			FbxVector4 v = e->GetDirectArray().GetAt(index);
			poly_vertices[i].tangent.x = v[0];
			poly_vertices[i].tangent.y = v[1];
			poly_vertices[i].tangent.z = v[2];
		}
		break;
	default:
		break;
	}
}

void import_binormals(FbxMesh* mesh, int polygon_index,
                      MDB_file::Rigid_mesh_vertex* poly_vertices)
{
	if(mesh->GetElementBinormalCount() <= 0)
		return;

	auto *e = mesh->GetElementBinormal(0);
	switch(e->GetMappingMode()) {
	case FbxGeometryElement::eByPolygonVertex:
		for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
			int index = mesh->GetPolygonVertexIndex(polygon_index) + i;
			FbxVector4 v = e->GetDirectArray().GetAt(index);
			poly_vertices[i].binormal.x = v[0];
			poly_vertices[i].binormal.y = v[1];
			poly_vertices[i].binormal.z = v[2];
		}
		break;
	case FbxGeometryElement::eByControlPoint:
		for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
			int index = mesh->GetPolygonVertex(polygon_index, i);
			FbxVector4 v = e->GetDirectArray().GetAt(index);
			poly_vertices[i].binormal.x = v[0];
			poly_vertices[i].binormal.y = v[1];
			poly_vertices[i].binormal.z = v[2];
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
			poly_vertices[i].uvw.x = v[0];
			poly_vertices[i].uvw.y = -v[1];
			poly_vertices[i].uvw.z = 1;
		}
		break;
	case FbxGeometryElement::eByControlPoint:
		for(int i = 0; i < mesh->GetPolygonSize(polygon_index); ++i) {
			int index = mesh->GetPolygonVertex(polygon_index, i);
			FbxVector2 v = uv->GetDirectArray().GetAt(index);
			poly_vertices[i].uvw.x = v[0];
			poly_vertices[i].uvw.y = -v[1];
			poly_vertices[i].uvw.z = 1;
		}
		break;
	default:
		break;
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
	material.diffuse_color.x = d[0];
	material.diffuse_color.y = d[1];
	material.diffuse_color.z = d[2];

	FbxDouble3 s = m->Specular.Get();
	material.specular_color.x = s[0];
	material.specular_color.y = s[1];
	material.specular_color.z = s[2];

	material.specular_value = m->SpecularFactor.Get()*200.0;
	material.specular_power = m->Shininess.Get()*2.5/100.0;
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
		cout << "Polygon is not a triangle\n";
		return;
	}
	
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
	cout << "Polygons: " << mesh->GetPolygonCount() << endl;

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
		cout << "Polygon is not a triangle\n";
		return;
	}
	
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

void import_rigid_mesh(MDB_file& mdb, FbxMesh* mesh)
{
	cout << "Layers: " << mesh->GetLayerCount() << endl;

	cout << "UV elements: " << mesh->GetElementUVCount();
	if(mesh->GetElementUVCount() > 0) {
		FbxGeometryElementUV *e = mesh->GetElementUV(0);
		cout << ' ' << mapping_mode_str(e->GetMappingMode()) << ' '
		     << reference_mode_str(e->GetReferenceMode());
	}
	cout << endl;

	cout << "Normal elements: " << mesh->GetElementNormalCount();
	if(mesh->GetElementNormalCount() > 0) {
		auto e = mesh->GetElementNormal(0);
		cout << ' ' << mapping_mode_str(e->GetMappingMode()) << ' '
		     << reference_mode_str(e->GetReferenceMode());
	}
	cout << endl;

	cout << "Tangent elements: " << mesh->GetElementTangentCount();
	if(mesh->GetElementTangentCount() > 0) {
		auto e = mesh->GetElementTangent(0);
		cout << ' ' << mapping_mode_str(e->GetMappingMode()) << ' '
		     << reference_mode_str(e->GetReferenceMode());
	}
	cout << endl;

	cout << "Binormal elements: " << mesh->GetElementBinormalCount();
	if(mesh->GetElementBinormalCount() > 0) {
		auto e = mesh->GetElementBinormal(0);
		cout << ' ' << mapping_mode_str(e->GetMappingMode()) << ' '
		     << reference_mode_str(e->GetReferenceMode());
	}
	cout << endl;

	cout << "Polygons: " << mesh->GetPolygonCount() << endl;

	auto rigid_mesh = make_unique<MDB_file::Rigid_mesh>();
	strncpy(rigid_mesh->header.name, mesh->GetName(), 32);

	import_material(rigid_mesh->header.material, mesh);

	for(int i = 0; i < mesh->GetPolygonCount(); ++i)
		import_polygon(*rigid_mesh.get(), mesh, i);

	mdb.add_packet(move(rigid_mesh));
}

void import_mesh(MDB_file& mdb, FbxMesh* mesh)
{
	cout << mesh->GetName() << endl;

	if(ends_with(mesh->GetName(), "_C2"))
		import_collision_mesh(mdb, mesh);
	else if(ends_with(mesh->GetName(), "_C3"))
		import_collision_mesh(mdb, mesh);
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

int main(int argc, char* argv[])
{
	if(argc < 2) {
		cout << "Usage: fbx2mdb <file>\n";
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

	string output_filename = path(argv[1]).stem().string() + ".MDB";
	mdb.save(output_filename.c_str());

	cout << "Output is " << output_filename << endl;

	// Destroy the sdk manager and all other objects it was handling.
	manager->Destroy();

	return 0;
}
