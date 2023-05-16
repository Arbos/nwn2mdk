#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <set>
#include <assert.h>
#include <string.h>

#include "crc32.h"
#include "gr2_decompress.h"
#include "gr2_file.h"

#ifdef USE_GRANNY32DLL
#include "granny2dll_handle.h"
#endif

#include "string_collection.h"

const uint32_t magic0 = 0xcab067b8;
const uint32_t magic1 = 0xfb16df8;
const uint32_t magic2 = 0x7e8c7284;
const uint32_t magic3 = 0x1e00195e;

static GR2_property_key ArtToolInfo_def[] = {
	{ GR2_property_type(8), "FromArtToolName", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "ArtToolMajorRevision", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "ArtToolMinorRevision", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(10), "UnitsPerMeter", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(10), "Origin", nullptr, 3, 0, 0, 0, 0 },
	{ GR2_property_type(10), "RightVector", nullptr, 3, 0, 0, 0, 0 },
	{ GR2_property_type(10), "UpVector", nullptr, 3, 0, 0, 0, 0 },
	{ GR2_property_type(10), "BackVector", nullptr, 3, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key ExporterInfo_def[] = {
	{ GR2_property_type(8), "ExporterName", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "ExporterMajorRevision", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "ExporterMinorRevision", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "ExporterCustomization", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "ExporterBuildNumber", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Layout_def[] = {
	{ GR2_property_type(19), "BytesPerPixel", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "ShiftForComponent", nullptr, 4, 0, 0, 0, 0 },
	{ GR2_property_type(19), "BitsForComponent", nullptr, 4, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Pixels_def[] = {
	{ GR2_property_type(12), "UInt8", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key MIPLevels_def[] = {
	{ GR2_property_type(19), "Stride", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "Pixels", Pixels_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Images_def[] = {
	{ GR2_property_type(3), "MIPLevels", MIPLevels_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Textures_def[] = {
	{ GR2_property_type(8), "FromFileName", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "TextureType", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "Width", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "Height", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "Encoding", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "SubFormat", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(1), "Layout", Layout_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "Images", Images_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(5), "ExtendedData", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key *Maps_def();

static GR2_property_key Materials_def[] = {
	{ GR2_property_type(8), "Name", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "Maps", Maps_def(), 0, 0, 0, 0, 0 },
	{ GR2_property_type(2), "Texture", Textures_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(5), "ExtendedData", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key *Maps_def()
{
	static GR2_property_key k[] = {
		{ GR2_property_type(8), "Usage", nullptr, 0, 0, 0, 0, 0 },
		{ GR2_property_type(2), "Map", Materials_def, 0, 0, 0, 0, 0 },
		{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
	};

	return k;
};

static GR2_property_key LightInfo_def[] = {
	{ GR2_property_type(5), "ExtendedData", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key CameraInfo_def[] = {
	{ GR2_property_type(5), "ExtendedData", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Bones_def[] = {
	{ GR2_property_type(8), "Name", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "ParentIndex", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(9), "Transform", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(10), "InverseWorldTransform", nullptr, 16, 0, 0, 0, 0 },
	{ GR2_property_type(2), "LightInfo", LightInfo_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(2), "CameraInfo", CameraInfo_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(5), "ExtendedData", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Skeletons_def[] = {
	{ GR2_property_type(8), "Name", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "Bones", Bones_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key VertexComponentNames_def[] = {
	{ GR2_property_type(8), "String", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key VertexAnnotationIndices_def[] = {
	{ GR2_property_type(19), "Int32", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key VertexAnnotationSets_def[] = {
	{ GR2_property_type(8), "Name", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(7), "VertexAnnotations", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "IndicesMapFromVertexToAnnotation", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "VertexAnnotationIndices", VertexAnnotationIndices_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key VertexDatas_def[] = {
	{ GR2_property_type(7), "Vertices", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "VertexComponentNames", VertexComponentNames_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "VertexAnnotationSets", VertexAnnotationSets_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Groups_def[] = {
	{ GR2_property_type(19), "MaterialIndex", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "TriFirst", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "TriCount", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Indices16_def[] = {
	{ GR2_property_type(15), "Int16", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key TriAnnotationSets_def[] = {
	{ GR2_property_type(8), "Name", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(7), "TriAnnotations", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "IndicesMapFromTriToAnnotation", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "TriAnnotationIndices", VertexAnnotationIndices_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key TriTopologies_def[] = {
	{ GR2_property_type(3), "Groups", Groups_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "Indices", VertexAnnotationIndices_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "Indices16", Indices16_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "VertexToVertexMap", VertexAnnotationIndices_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "VertexToTriangleMap", VertexAnnotationIndices_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "SideToNeighborMap", VertexAnnotationIndices_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "BonesForTriangle", VertexAnnotationIndices_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "TriangleToBoneIndices", VertexAnnotationIndices_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "TriAnnotationSets", TriAnnotationSets_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key MorphTargets_def[] = {
	{ GR2_property_type(8), "ScalarName", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(2), "VertexData", VertexDatas_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key MaterialBindings_def[] = {
	{ GR2_property_type(2), "Material", Materials_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key BoneBindings_def[] = {
	{ GR2_property_type(8), "BoneName", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(10), "OBBMin", nullptr, 3, 0, 0, 0, 0 },
	{ GR2_property_type(10), "OBBMax", nullptr, 3, 0, 0, 0, 0 },
	{ GR2_property_type(3), "TriangleIndices", VertexAnnotationIndices_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Meshes_def[] = {
	{ GR2_property_type(8), "Name", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(2), "PrimaryVertexData", VertexDatas_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "MorphTargets", MorphTargets_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(2), "PrimaryTopology", TriTopologies_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "MaterialBindings", MaterialBindings_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "BoneBindings", BoneBindings_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(5), "ExtendedData", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key MeshBindings_def[] = {
	{ GR2_property_type(2), "Mesh", Meshes_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Models_def[] = {
	{ GR2_property_type(8), "Name", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(2), "Skeleton", Skeletons_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(9), "InitialPlacement", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "MeshBindings", MeshBindings_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key ValueCurve_def[] = {
	{ GR2_property_type(5), "CurveData", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key VectorTracks_def[] = {
	{ GR2_property_type(8), "Name", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "Dimension", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(1), "ValueCurve", ValueCurve_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key TransformTracks_def[] = {
	{ GR2_property_type(8), "Name", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(1), "PositionCurve", ValueCurve_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(1), "OrientationCurve", ValueCurve_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(1), "ScaleShearCurve", ValueCurve_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key TransformLODErrors_def[] = {
	{ GR2_property_type(10), "Real32", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Entries_def[] = {
	{ GR2_property_type(10), "TimeStamp", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(8), "Text", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key TextTracks_def[] = {
	{ GR2_property_type(8), "Name", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "Entries", Entries_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key PeriodicLoop_def[] = {
	{ GR2_property_type(10), "Radius", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(10), "dAngle", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(10), "dZ", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(10), "BasisX", nullptr, 3, 0, 0, 0, 0 },
	{ GR2_property_type(10), "BasisY", nullptr, 3, 0, 0, 0, 0 },
	{ GR2_property_type(10), "Axis", nullptr, 3, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key TrackGroups_def[] = {
	{ GR2_property_type(8), "Name", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "VectorTracks", VectorTracks_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "TransformTracks", TransformTracks_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "TransformLODErrors", TransformLODErrors_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(3), "TextTracks", TextTracks_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(9), "InitialPlacement", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(19), "AccumulationFlags", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(10), "LoopTranslation", nullptr, 3, 0, 0, 0, 0 },
	{ GR2_property_type(2), "PeriodicLoop", PeriodicLoop_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(2), "RootMotion", TransformTracks_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(5), "ExtendedData", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key Animations_def[] = {
	{ GR2_property_type(8), "Name", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(10), "Duration", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(10), "TimeStep", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(10), "Oversampling", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(4), "TrackGroups", TrackGroups_def, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

static GR2_property_key gr2_type_def[] = {
	{ GR2_property_type(2), "ArtToolInfo", ArtToolInfo_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(2), "ExporterInfo", ExporterInfo_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(8), "FromFileName", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_property_type(4), "Textures", Textures_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(4), "Materials", Materials_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(4), "Skeletons", Skeletons_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(4), "VertexDatas", VertexDatas_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(4), "TriTopologies", TriTopologies_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(4), "Meshes", Meshes_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(4), "Models", Models_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(4), "TrackGroups", TrackGroups_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(4), "Animations", Animations_def, 0, 0, 0, 0, 0 },
	{ GR2_property_type(5), "ExtendedData", nullptr, 0, 0, 0, 0, 0 },
	{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0 }
};

struct GR2_export_info {
	std::stringstream streams[7];
	std::vector<GR2_file::Relocation> relocations[6];
	String_collection strings;
	std::map<GR2_property_key*, uint32_t> keys;
	std::map<GR2_skeleton*, uint32_t> skeletons;
	std::map<GR2_track_group*, uint32_t> track_groups;
};

using namespace std;

uint32_t export_key(GR2_export_info& export_info, GR2_property_key *key)
{
	auto it = export_info.keys.find(key);
	if (it != export_info.keys.end())
		return it->second;

	uint32_t offset = uint32_t(export_info.streams[0].tellp());
	export_info.keys[key] = offset;

	auto k = key;
	while (k->type != 0) {
		export_info.streams[0].write((char*)k, sizeof(GR2_property_key));
		++k;
	}
	export_info.streams[0].write((char*)k, sizeof(GR2_property_key));

	k = key;
	int i = 0;
	while (k->type != 0) {
		if (k->name) {
			auto target_offset = export_info.strings.write(k->name, export_info.streams[6]);
			export_info.relocations[0].push_back(
			{ offset + sizeof(GR2_property_key)*i + offsetof(GR2_property_key, name), 6, target_offset });
		}

		if (k->keys) {
			uint32_t target_offset = export_key(export_info, k->keys);
			export_info.relocations[0].push_back(
			{ offset + sizeof(GR2_property_key)*i + offsetof(GR2_property_key, keys), 0, target_offset });
		}

		++k;
		++i;
	}

	return offset;
}

uint32_t export_art_tool_info(GR2_export_info& export_info, GR2_art_tool_info* art_tool_info)
{
	uint32_t offset = uint32_t(export_info.streams[0].tellp());

	export_info.streams[0].write((char*)art_tool_info, sizeof(GR2_art_tool_info));

	if (art_tool_info->from_art_tool_name) {
		auto target_offset = export_info.strings.write(art_tool_info->from_art_tool_name, export_info.streams[6]);
		export_info.relocations[0].push_back({ offset, 6, target_offset });
	}

	return offset;
}

uint32_t export_exporter_info(GR2_export_info& export_info, GR2_exporter_info* exporter_info)
{
	uint32_t offset = uint32_t(export_info.streams[1].tellp());

	export_info.streams[1].write((char*)exporter_info, sizeof(GR2_exporter_info));

	if (exporter_info->exporter_name) {
		auto target_offset = export_info.strings.write(exporter_info->exporter_name, export_info.streams[6]);
		export_info.relocations[1].push_back({ offset, 6, target_offset });
	}

	return offset;
}

uint32_t export_bone(GR2_export_info& export_info, GR2_bone* bone)
{
	uint32_t offset = uint32_t(export_info.streams[2].tellp());

	// Make a copy of the bone and empty the extended data, which
	// is not exported.
	GR2_bone b = *bone;
	b.extended_data.keys = nullptr;
	b.extended_data.values = nullptr;
	export_info.streams[2].write((char*)&b, sizeof(GR2_bone));

	if (bone->name) {
		auto target_offset = export_info.strings.write(bone->name, export_info.streams[6]);
		export_info.relocations[2].push_back(
			{ offset + offsetof(GR2_bone, name), 6, target_offset });
	}

	return offset;
}

uint32_t export_bones(GR2_export_info& export_info, GR2_skeleton* skel)
{
	uint32_t offset = uint32_t(export_info.streams[2].tellp());

	for (int32_t i = 0; i < skel->bones_count; ++i)
		export_bone(export_info, &skel->bones[i]);

	return offset;
}

uint32_t export_skeleton(GR2_export_info& export_info, GR2_skeleton* skel)
{
	uint32_t offset = uint32_t(export_info.streams[2].tellp());
	export_info.skeletons[skel] = offset;

	export_info.streams[2].write((char*)skel, sizeof(GR2_skeleton));

	if (skel->name) {
		auto target_offset = export_info.strings.write(skel->name, export_info.streams[6]);
		export_info.relocations[2].push_back(
			{ offset + offsetof(GR2_skeleton, name), 6, target_offset });
	}

	auto target_offset = export_bones(export_info, skel);
	export_info.relocations[2].push_back(
		{ offset + offsetof(GR2_skeleton, bones), 2, target_offset });

	return offset;
}

uint32_t export_skeletons(GR2_export_info& export_info, GR2_file_info* fi)
{
	uint32_t offset = uint32_t(export_info.streams[0].tellp());

	// Write array of pointers to skeletons
	for (int32_t i = 0; i < fi->skeletons_count; ++i) {
		// The pointer is initially null, it'll be relocated later.
		int32_t p = 0;
		export_info.streams[0].write((char*)&p, 4);
	}

	for (int32_t i = 0; i < fi->skeletons_count; ++i) {
		auto target_offset = export_skeleton(export_info, fi->skeletons[i]);
		export_info.relocations[0].push_back({ offset + 4 * i, 2, target_offset });
	}

	return offset;
}

uint32_t export_model(GR2_export_info& export_info, GR2_model* model)
{
	uint32_t offset = uint32_t(export_info.streams[3].tellp());	

	export_info.streams[3].write((char*)model, sizeof(GR2_model));

	if (model->name) {
		auto target_offset = export_info.strings.write(model->name, export_info.streams[6]);
		export_info.relocations[3].push_back(
		{ offset + offsetof(GR2_model, name), 6, target_offset });
	}

	export_info.relocations[3].push_back({ offset + offsetof(GR2_model, skeleton), 2, export_info.skeletons[model->skeleton] });

	return offset;
}

uint32_t export_models(GR2_export_info& export_info, GR2_file_info* fi)
{
	uint32_t offset = uint32_t(export_info.streams[0].tellp());

	// Write array of pointers to models
	for (int32_t i = 0; i < fi->models_count; ++i) {
		// The pointer is initially null, it'll be relocated later.
		int32_t p = 0;
		export_info.streams[0].write((char*)&p, 4);
	}

	for (int32_t i = 0; i < fi->models_count; ++i) {
		auto target_offset = export_model(export_info, fi->models[i]);
		export_info.relocations[0].push_back({ offset + 4 * i, 3, target_offset });
	}

	return offset;
}

uint32_t export_curve_data(GR2_export_info& export_info, GR2_curve_data* cd)
{
	uint32_t offset = uint32_t(export_info.streams[6].tellp());

	if (cd->curve_data_header.format == DaK32fC32f) {
		GR2_curve_data_DaK32fC32f* data =
			(GR2_curve_data_DaK32fC32f*)cd;
		export_info.streams[6].write((char*)cd, sizeof(GR2_curve_data_DaK32fC32f));

		uint32_t target_offset = uint32_t(export_info.streams[6].tellp());
		export_info.streams[6].write((char*)data->knots.get(), data->knots_count * sizeof(float));
		export_info.relocations[0].push_back(
		{ offset + offsetof(GR2_curve_data_DaK32fC32f, knots), 66, target_offset });

		target_offset = uint32_t(export_info.streams[6].tellp());
		export_info.streams[6].write((char*)data->controls.get(), data->controls_count * sizeof(float));
		export_info.relocations[0].push_back(
		{ offset + offsetof(GR2_curve_data_DaK32fC32f, controls), 66, target_offset });
	}
	else if (cd->curve_data_header.format == DaIdentity) {
		export_info.streams[6].write((char*)cd, sizeof(GR2_curve_data_DaIdentity));
	}
	else if (cd->curve_data_header.format == DaConstant32f) {
		GR2_curve_data_DaConstant32f* data =
			(GR2_curve_data_DaConstant32f*)cd;
		export_info.streams[6].write((char*)cd, sizeof(GR2_curve_data_DaConstant32f));

		uint32_t target_offset = uint32_t(export_info.streams[6].tellp());
		export_info.streams[6].write((char*)data->controls.get(), data->controls_count * sizeof(float));
		export_info.relocations[0].push_back(
		{ offset + offsetof(GR2_curve_data_DaConstant32f, controls), 66, target_offset });
	}
	else if (cd->curve_data_header.format == D3Constant32f) {
		export_info.streams[6].write((char*)cd, sizeof(GR2_curve_data_D3Constant32f));
	}
	else if (cd->curve_data_header.format == D4nK16uC15u) {
		cout << "ERROR: Curve format D4nK16uC15u not supported\n";
	}
	else if (cd->curve_data_header.format == D4nK8uC7u) {
		GR2_curve_data_D4nK8uC7u* data =
			(GR2_curve_data_D4nK8uC7u*)cd;
		export_info.streams[6].write((char*)cd, sizeof(GR2_curve_data_D4nK8uC7u));
		uint32_t target_offset = uint32_t(export_info.streams[6].tellp());
		export_info.streams[6].write((char*)data->knots_controls.get(), data->knots_controls_count * sizeof(uint8_t));
		export_info.relocations[0].push_back(
		{ offset + offsetof(GR2_curve_data_D4nK8uC7u, knots_controls), 66, target_offset });
	}
	else if (cd->curve_data_header.format == D3K8uC8u) {
		cout << "ERROR: Curve format D3K8uC8u not supported\n";
	}
	else {
		cout << "ERROR: Curve format " << cd->curve_data_header.format
		     << "not supported\n";
	}

	return offset;
}

void export_curve(GR2_export_info& export_info, uint32_t offset, GR2_curve* curve)
{
	auto target_offset = export_key(export_info, curve->keys);
	export_info.relocations[4].push_back(
	{ offset + offsetof(GR2_curve, keys), 0, target_offset });

	target_offset = export_curve_data(export_info, curve->curve_data);
	export_info.relocations[4].push_back(
	{ offset + offsetof(GR2_curve, curve_data), 6, target_offset });
}

uint32_t export_transform_track(GR2_export_info& export_info, GR2_transform_track* tt)
{
	uint32_t offset = uint32_t(export_info.streams[4].tellp());

	export_info.streams[4].write((char*)tt, sizeof(GR2_transform_track));

	if (tt->name) {
		auto target_offset = export_info.strings.write(tt->name, export_info.streams[6]);
		export_info.relocations[4].push_back(
		{ offset + offsetof(GR2_transform_track, name), 6, target_offset });
	}

	export_curve(export_info, offset + offsetof(GR2_transform_track, position_curve), &tt->position_curve);
	export_curve(export_info, offset + offsetof(GR2_transform_track, orientation_curve), &tt->orientation_curve);
	export_curve(export_info, offset + offsetof(GR2_transform_track, scale_shear_curve), &tt->scale_shear_curve);

	return offset;
}

uint32_t export_transform_tracks(GR2_export_info& export_info, GR2_track_group* tg)
{
	uint32_t offset = uint32_t(export_info.streams[4].tellp());

	for (int32_t i = 0; i < tg->transform_tracks_count; ++i)
		export_transform_track(export_info, &tg->transform_tracks[i]);

	return offset;
}

uint32_t export_track_group(GR2_export_info& export_info, GR2_track_group* tg)
{
	uint32_t offset = uint32_t(export_info.streams[4].tellp());
	export_info.track_groups[tg] = offset;

	export_info.streams[4].write((char*)tg, sizeof(GR2_track_group));

	if (tg->name) {
		auto target_offset = export_info.strings.write(tg->name, export_info.streams[6]);
		export_info.relocations[4].push_back(
		{ offset + offsetof(GR2_track_group, name), 6, target_offset });
	}

	auto target_offset = export_transform_tracks(export_info, tg);
	export_info.relocations[4].push_back(
	{ offset + offsetof(GR2_track_group, transform_tracks), 4, target_offset });

	return offset;
}

uint32_t export_track_groups(GR2_export_info& export_info, GR2_file_info* fi)
{
	uint32_t offset = uint32_t(export_info.streams[0].tellp());

	for (int32_t i = 0; i < fi->track_groups_count; ++i) {
		int32_t x = 0;
		export_info.streams[0].write((char*)&x, 4);
	}

	for (int32_t i = 0; i < fi->track_groups_count; ++i) {
		auto target_offset = export_track_group(export_info, fi->track_groups[i]);
		export_info.relocations[0].push_back({ offset + 4 * i, 4, target_offset });
	}

	return offset;
}

uint32_t export_animation(GR2_export_info& export_info, GR2_animation* anim)
{
	uint32_t offset = uint32_t(export_info.streams[5].tellp());

	export_info.streams[5].write((char*)anim, sizeof(GR2_animation));

	if (anim->name) {
		auto target_offset = export_info.strings.write(anim->name, export_info.streams[6]);
		export_info.relocations[5].push_back(
		{ offset + offsetof(GR2_animation, name), 6, target_offset });
	}

	auto target_offset = uint32_t(export_info.streams[5].tellp());
	export_info.relocations[5].push_back({ offset + offsetof(GR2_animation, track_groups), 5, target_offset });

	for (int32_t i = 0; i < anim->track_groups_count; ++i) {
		int32_t x = 0;
		export_info.streams[5].write((char*)&x, 4);
		export_info.relocations[5].push_back({ offset + sizeof(GR2_animation) + 4 * i, 4, export_info.track_groups[anim->track_groups[i]] });
	}

	return offset;
}

uint32_t export_animations(GR2_export_info& export_info, GR2_file_info *fi)
{
	uint32_t offset = uint32_t(export_info.streams[0].tellp());

	for (int32_t i = 0; i < fi->animations_count; ++i) {
		int32_t x = 0;
		export_info.streams[0].write((char*)&x, 4);
	}

	for (int32_t i = 0; i < fi->animations_count; ++i) {
		auto target_offset = export_animation(export_info, fi->animations[i]);
		export_info.relocations[0].push_back({ offset + 4 * i, 5, target_offset });
	}

	return offset;
}

std::string GR2_file::granny2dll_filename = "granny2.dll";

GR2_file::GR2_file() : section_headers(6)
{
	header.magic[0] = magic0;
	header.magic[1] = magic1;
	header.magic[2] = magic2;
	header.magic[3] = magic3;
	header.size = 352;
	header.format = 0;
	header.reserved[0] = header.reserved[1] = 0;
	header.info.version = 6;
	header.info.file_size = 0;
	header.info.crc32 = 0;
	header.info.sections_offset = 56;
	header.info.sections_count = 6;
	header.info.type_section = 0;
	header.info.type_offset = 0;
	header.info.root_section = 0;
	header.info.root_offset = 0;
	header.info.tag = 0x80000015;
	header.info.extra[0] = 0;
	header.info.extra[1] = 0;
	header.info.extra[2] = 0;
	header.info.extra[3] = 0;

	for (auto &h : section_headers) {		
		h.compression = 0;
		h.data_offset = 0;
		h.data_size = 0;
		h.decompressed_size = 0;
		h.alignment = 4;
		h.first16bit = 0;
		h.first8bit = 0;
		h.relocations_offset = 0;
		h.relocations_count = 0;		
		h.marshallings_offset = 0;
		h.marshallings_count = 0;	
	}

	is_good = true;
}

GR2_file::GR2_file(const char* path)
{
	is_good = true;

	ifstream in(path, std::ios::in | std::ios::binary);

	if (!in) {
		is_good = false;
		error_string_ = "cannot open file";
		return;
	}

	read(in);
}

GR2_file::GR2_file(std::istream& in)
{
	is_good = true;
	read(in);
}

void GR2_file::apply_marshalling(std::istream&)
{
	// We don't expect to load GR2 files with an endianness that doesn't
	// match the CPU endianness, so no need to apply marshalling.
}

void GR2_file::apply_marshalling(std::istream& in, unsigned index)
{
	Section_header& section = section_headers[index];

	if (section.marshallings_count <= 0)
		return;

	in.seekg(section.marshallings_offset);
	std::vector<Marshalling> marshallings(section.marshallings_count);
	in.read((char*)marshallings.data(),
	        section.marshallings_count * sizeof(Marshalling));
	for (unsigned i = 0; i < marshallings.size(); ++i)
		apply_marshalling(index, marshallings[i]);
}

void GR2_file::apply_marshalling(unsigned index, Marshalling& m)
{
	auto type_def = (GR2_property_key*)(sections_data.data() +
	                                    section_offsets[m.target_section] +
	                                    m.target_offset);
	if (type_def->type != GR2_type_inline) {
		cout << "WARNING: unhandled case\n";
		assert(false);
	}
	else if (type_def->keys) {
		GR2_property_key* p = type_def->keys;
		while (p->type != GR2_type_none) {
			if (p->type != GR2_type_uint8) {
				cout << "WARNING: unhandled case\n";
				assert(false);
			}
			++p;
		}
	}
}

void GR2_file::apply_relocations()
{
	for (unsigned i = 0; i < header.info.sections_count; ++i)
		apply_relocations(i);
}

void GR2_file::apply_relocations(unsigned index)
{
	for (const auto& relocation : relocations[index]) {
		unsigned source_offset =
		    section_offsets[index] + relocation.offset;
		unsigned target_offset =
		    section_offsets[relocation.target_section] +
		    relocation.target_offset;
		unsigned char* target_address =
		    sections_data.data() + target_offset;
		auto encoded_ptr = encode_ptr(target_address);
		memcpy(&sections_data[source_offset], &encoded_ptr, 4);
	}
}

bool GR2_file::decompress_section_data(unsigned section_index, unsigned char* section_data, unsigned char* decompressed_buffer)
{
	Section_header& section = section_headers[section_index];

	switch (section.compression) {
	case 0: // Uncompressed
		memcpy(decompressed_buffer, section_data, section.data_size);
		break;
	case 1: // Oodle0
		is_good = false;
		error_string_ = "oodle0 compression method unsupported";
		return false;
	case 2: // Oodle1
		gr2_decompress(section.data_size, section_data,
			section.first16bit, section.first8bit, section.decompressed_size,
			decompressed_buffer);		
		break;
	default:
		is_good = false;
		error_string_ = "unknown compression method";
		return false;
	}

	return true;
}

#ifdef USE_GRANNY32DLL
bool GR2_file::decompress_section_data_dll(unsigned section_index, unsigned char* section_data, unsigned char* decompressed_buffer)
{
	static Granny2dll_handle granny2dll(granny2dll_filename.c_str());

	if (!granny2dll) {
		is_good = false;
		error_string_ = "cannot load library " + granny2dll_filename;
		return false;
	}

	if (!granny2dll.GrannyDecompressData) {
		is_good = false;
		error_string_ = "cannot get address of GrannyDecompressData";
		return false;
	}

	Section_header& section = section_headers[section_index];

	int ret = granny2dll.GrannyDecompressData(
		section.compression, 0, section.data_size, section_data,
		section.first16bit, section.first8bit, section.decompressed_size,
		decompressed_buffer);

	if (!ret) {
		is_good = false;
		error_string_ = "cannot decompress section";
		return false;
	}

#ifdef TEST_GR2_DECOMPRESSION
	std::vector<unsigned char> decompressed_buffer_alt(section.decompressed_size);
	decompress_section_data(section_index, section_data, decompressed_buffer_alt.data());	
	assert(memcmp(decompressed_buffer, decompressed_buffer_alt.data(), section.decompressed_size) == 0);
#endif

	return true;
}
#endif

void GR2_file::check_crc(std::istream& in)
{
	std::vector<unsigned char> buffer(header.info.file_size -
	                                  sizeof(Header));
	in.seekg(sizeof(Header));
	in.read((char*)buffer.data(), buffer.size());
	auto crc32 = crc32c(0, buffer.data(), buffer.size());
	if (header.info.crc32 != crc32) {
		is_good = false;
		error_string_ = "CRC32 error";
	}
}

void GR2_file::check_magic()
{
	if (header.magic[0] != magic0 || header.magic[1] != magic1 ||
	    header.magic[2] != magic2 || header.magic[3] != magic3) {
		is_good = false;
		error_string_ = "wrong magic numbers";
	}
}

void GR2_file::read(std::istream& in)
{
	read_header(in);
	if (!is_good)
		return;

	check_magic();
	if (!is_good)
		return;

	read_section_headers(in);
	if (!is_good)
		return;

	check_crc(in);
	if (!is_good)
		return;

	read_sections(in);
	read_relocations(in);
	apply_relocations();
	apply_marshalling(in);

	file_info = (GR2_file_info*)sections_data.data();
	type_definition =
	    (GR2_property_key*)(sections_data.data() + section_offsets[header.info.type_section] + header.info.type_offset);
}

void GR2_file::read_header(std::istream& in)
{
	in.read((char*)&header, sizeof(Header));
	if (!in) {
		is_good = false;
		error_string_ = "cannot read header";
	}
}

void GR2_file::read_relocations(std::istream& in)
{
	for (auto& section : section_headers)
		read_relocations(in, section);
}

void GR2_file::read_relocations(std::istream& in, Section_header& section)
{
	in.seekg(section.relocations_offset);
	relocations.emplace_back(section.relocations_count);
	if (section.relocations_count > 0)
		in.read((char*)relocations.back().data(),
		        relocations.back().size() * sizeof(Relocation));
}

void GR2_file::read_section(std::istream& in, unsigned index)
{
	Section_header& section = section_headers[index];

	if (section.data_size == 0)
		return;

	in.seekg(section.data_offset); // Seek to the start of section data
	// Add 4 bytes in case decompressing must pad to multiple of 4.
	vector<unsigned char> section_data(section.data_size + 4);
	in.read((char*)section_data.data(), section.data_size);
	if (in.eof()) {
		is_good = false;
		error_string_ = "unexpected end of file";
		return;
	}
	else if (!in) {
		is_good = false;
		error_string_ = "cannot read section";
		return;
	}

#ifdef USE_GRANNY32DLL
	decompress_section_data_dll(index, section_data.data(), sections_data.data() + section_offsets[index]);		
#else
	decompress_section_data(index, section_data.data(), sections_data.data() + section_offsets[index]);
#endif
}

void GR2_file::read_section_headers(std::istream& in)
{
	section_headers.resize(header.info.sections_count);
	in.read((char*)section_headers.data(),
	        sizeof(Section_header) * header.info.sections_count);
	if (!in) {
		is_good = false;
		error_string_ = "cannot read section headers";
		return;
	}
}

void GR2_file::read_sections(std::istream& in)
{
	int total_size = 0;
	for (auto& h : section_headers) {
		section_offsets.push_back(total_size);
		total_size += h.decompressed_size;
	}

	sections_data.resize(total_size);

	for (unsigned i = 0; i < header.info.sections_count; ++i)
		read_section(in, i);
}

GR2_file::operator bool() const
{
	return is_good;
}

std::string GR2_file::error_string() const
{
	return error_string_;
}

void GR2_file::read(GR2_file_info* file_info)
{	
	GR2_export_info export_info;

	uint32_t offset = uint32_t(export_info.streams[0].tellp());

	export_info.streams[0].write((char*)file_info, sizeof(GR2_file_info));

	auto target_offset = export_art_tool_info(export_info, file_info->art_tool_info);
	export_info.relocations[0].push_back({ offset, 0, target_offset });

	target_offset = export_exporter_info(export_info, file_info->exporter_info);
	export_info.relocations[0].push_back({ offset + offsetof(GR2_file_info, exporter_info), 1, target_offset });

	if (file_info->from_file_name) {
		auto target_offset = export_info.strings.write(file_info->from_file_name, export_info.streams[6]);
		export_info.relocations[0].push_back({ offset + offsetof(GR2_file_info, from_file_name), 6, target_offset });
	}

	target_offset = uint32_t(export_info.streams[0].tellp());
	export_info.relocations[0].push_back({ offset + offsetof(GR2_file_info, skeletons), 0, target_offset });

	export_skeletons(export_info, file_info);

	target_offset = uint32_t(export_info.streams[0].tellp());
	export_info.relocations[0].push_back({ offset + offsetof(GR2_file_info, models), 0, target_offset });

	export_models(export_info, file_info);

	target_offset = uint32_t(export_info.streams[0].tellp());
	export_info.relocations[0].push_back({ offset + offsetof(GR2_file_info, track_groups), 0, target_offset });

	export_track_groups(export_info, file_info);

	target_offset = uint32_t(export_info.streams[0].tellp());
	export_info.relocations[0].push_back({ offset + offsetof(GR2_file_info, animations), 0, target_offset });

	export_animations(export_info, file_info);	
	
	header.info.type_offset = export_key(export_info, gr2_type_def);

	target_offset = uint32_t(export_info.streams[0].tellp());
	for (int i = 0; i < 6; ++i) {
		for (auto &r : export_info.relocations[i]) {
			if (r.target_section == 6) {
				r.target_section = 0;
				r.target_offset += target_offset;
			}
			else if (r.target_section == 66) {
				r.offset += target_offset;
				r.target_section = 0;
				r.target_offset += target_offset;
			}
		}
	}

	section_offsets.resize(6);
	section_offsets[0] = 0;
	section_offsets[1] = uint32_t(export_info.streams[0].tellp() + export_info.streams[6].tellp());
	section_offsets[2] = uint32_t(section_offsets[1] + export_info.streams[1].tellp());
	section_offsets[3] = uint32_t(section_offsets[2] + export_info.streams[2].tellp());
	section_offsets[4] = uint32_t(section_offsets[3] + export_info.streams[3].tellp());
	section_offsets[5] = uint32_t(section_offsets[4] + export_info.streams[4].tellp());	

	sections_data.resize(section_offsets[5] + uint32_t(export_info.streams[5].tellp()));

	section_headers[0].data_size = section_offsets[1] - section_offsets[0];
	section_headers[1].data_size = section_offsets[2] - section_offsets[1];
	section_headers[2].data_size = section_offsets[3] - section_offsets[2];
	section_headers[3].data_size = section_offsets[4] - section_offsets[3];
	section_headers[4].data_size = section_offsets[5] - section_offsets[4];
	section_headers[5].data_size = sections_data.size() - section_offsets[5];
	
	relocations.resize(6);
	for (int i = 0; i < 6; ++i)
		relocations[i] = export_info.relocations[i];
	
	export_info.streams[0].read((char*)sections_data.data(), export_info.streams[0].tellp());
	export_info.streams[6].read((char*)sections_data.data() + int(export_info.streams[0].tellp()), export_info.streams[6].tellp());
	export_info.streams[1].read((char*)sections_data.data() + section_offsets[1], export_info.streams[1].tellp());
	export_info.streams[2].read((char*)sections_data.data() + section_offsets[2], export_info.streams[2].tellp());
	export_info.streams[3].read((char*)sections_data.data() + section_offsets[3], export_info.streams[3].tellp());
	export_info.streams[4].read((char*)sections_data.data() + section_offsets[4], export_info.streams[4].tellp());
	export_info.streams[5].read((char*)sections_data.data() + section_offsets[5], export_info.streams[5].tellp());

	apply_relocations();

	this->file_info = (GR2_file_info*)sections_data.data();
	type_definition = gr2_type_def;

	offset = sizeof(Header) + section_headers.size() * sizeof(Section_header);
	for (unsigned i = 0; i < section_headers.size(); ++i) {
		Section_header& section = section_headers[i];
		section.compression = 0;
		section.relocations_offset = offset;
		section.relocations_count = relocations[i].size();
		offset += section.relocations_count * sizeof(Relocation);
		section.marshallings_offset = offset;
		section.marshallings_count = 0;
		offset += section.marshallings_count * sizeof(Marshalling);
		section.data_offset = offset;		
		section.decompressed_size = section.data_size;
		section.alignment = 4;
		section.first16bit = 0;
		section.first8bit = 0;
		offset += section.data_size;
	}

	header.info.file_size = offset;
}

void GR2_file::write(const char* filename)
{	
	Header header;
	header.magic[0] = magic0;
	header.magic[1] = magic1;
	header.magic[2] = magic2;
	header.magic[3] = magic3;
	header.size = 352;
	header.format = 0;
	header.reserved[0] = header.reserved[1] = 0;
	header.info.version = 6;
	header.info.sections_offset = 56;
	header.info.sections_count = 6;
	header.info.type_section = 0;
	header.info.type_offset = this->header.info.type_offset;
	header.info.root_section = 0;
	header.info.root_offset = 0;
	header.info.tag = 0x80000015;
	header.info.extra[0] = 0;
	header.info.extra[1] = 0;
	header.info.extra[2] = 0;
	header.info.extra[3] = 0;

	unsigned offset = sizeof(Header) + 6 * sizeof(Section_header);
	std::vector<Section_header> sections(6);
	for (unsigned i = 0; i < 6; ++i) {
		Section_header& section = sections[i];
		section.compression = 0;
		section.relocations_offset = offset;
		section.relocations_count = relocations[i].size();
		offset += section.relocations_count * sizeof(Relocation);
		section.marshallings_offset = offset;
		section.marshallings_count = 0;
		offset += section.marshallings_count * sizeof(Marshalling);
		section.data_offset = offset;
		section.data_size = this->section_headers[i].decompressed_size;
		section.decompressed_size = section.data_size;
		section.alignment = 4;
		section.first16bit = 0;
		section.first8bit = 0;
		offset += section.data_size;
	}

	header.info.file_size = offset;

	header.info.crc32 = crc32c(0, (unsigned char*)sections.data(),
		sections.size() * sizeof(Section_header));
	for (unsigned i = 0; i < header.info.sections_count; ++i) {
		header.info.crc32 = crc32c(
			header.info.crc32, (unsigned char*)relocations[i].data(),
			relocations[i].size() * sizeof(Relocation));
		if (section_offsets[i] < sections_data.size())
			header.info.crc32 = crc32c(
				header.info.crc32,
				(unsigned char*)&sections_data[section_offsets[i]],
				sections[i].data_size);
	}

	ofstream out(filename, std::ios::binary);
	out.write((char*)&header, sizeof(Header));
	out.write((char*)sections.data(),
		sections.size() * sizeof(Section_header));

	for (unsigned i = 0; i < header.info.sections_count; ++i) {
		out.write((char*)relocations[i].data(),
			relocations[i].size() * sizeof(Relocation));
		if (section_offsets[i] < sections_data.size())
			out.write((char*)&sections_data[section_offsets[i]],
				sections[i].data_size);
	}
}
