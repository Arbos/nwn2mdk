#include <iostream>
#include <set>
#include <map>
#include <string>

#include "gr2_file.h"

using namespace std;

void print_keys(GR2_property_key* k, std::set<GR2_property_key*>& printed,
                int padding_size);

void print_key(GR2_property_key* k, std::set<GR2_property_key*>& printed,
               int padding_size)
{
	cout << string(padding_size, ' ').c_str();
	cout << k->name << ": " << k->type << " ("
	     << property_type_to_str(k->type) << ')';
	if (k->length > 0)
		cout << " [" << k->length << ']';
	cout << endl;

	// Print subkeys
	if (k->keys)
		print_keys(k->keys, printed, padding_size + 2);
}

void print_keys(GR2_property_key* k, std::set<GR2_property_key*>& printed,
                int padding_size)
{
	if (printed.find(k) != printed.end())
		return;

	printed.insert(k);

	while (k->type != 0) {
		print_key(k, printed, padding_size);
		++k;
	}
}

void print_keys(GR2_property_key* k)
{
	if (!k)
		return;

	cout << "Types\n";
	cout << "=====\n";

	// This set keeps tracks of keys already printed so we don't print
	// the same key more than once. This is needed because keys have
	// circular references.
	std::set<GR2_property_key*> printed;
	print_keys(k, printed, 0);

	cout << endl;
}

void print_float4(float q[4])
{
	cout << '[' << q[0] << ", " << q[1] << ", " << q[2] << ", " << q[3]
	     << ']';
}

template <typename T>
void print_vector(const Vector3<T>& v)
{
	cout << '[' << +v.x << ", " << +v.y << ", " << +v.z << ']';
}

template <typename T>
void print_vector(const Vector4<T>& v)
{
	cout << '[' << +v.x << ", " << +v.y << ", " << +v.z << ", " << +v.w
	     << ']';
}

void print_art_tool_info(GR2_art_tool_info* ArtToolInfo)
{
	cout << "Art Tool Info\n";
	cout << "=============\n";
	cout << "Name:            " << ArtToolInfo->from_art_tool_name << endl;
	cout << "Major Revision:  " << ArtToolInfo->art_tool_major_revision
	     << endl;
	cout << "Minor Revision:  " << ArtToolInfo->art_tool_minor_revision
	     << endl;
	cout << "Units per meter: " << ArtToolInfo->units_per_meter << endl;
	cout << "Origin:          ";
	print_vector(ArtToolInfo->origin);
	cout << endl;
	cout << "Right Vector:    ";
	print_vector(ArtToolInfo->right_vector);
	cout << endl;
	cout << "Up Vector:       ";
	print_vector(ArtToolInfo->up_vector);
	cout << endl;
	cout << "Back Vector:     ";
	print_vector(ArtToolInfo->back_vector);
	cout << endl;
	cout << endl;
}

void print_exporter_info(GR2_exporter_info* ExporterInfo)
{
	cout << "Exporter Info\n";
	cout << "=============\n";
	cout << "Name:           " << ExporterInfo->exporter_name << endl;
	cout << "Major Revision: " << ExporterInfo->exporter_major_revision
	     << endl;
	cout << "Minor Revision: " << ExporterInfo->exporter_minor_revision
	     << endl;
	cout << "Customization:  " << ExporterInfo->exporter_customization
	     << endl;
	cout << "Build Number:   " << ExporterInfo->exporter_build_number
	     << endl;
	cout << endl;
}

void print_file_info(GR2_file_info* info)
{
	cout << "File Info\n";
	cout << "==========\n";
	cout << "From File Name:      " << info->from_file_name << endl;
	cout << "Textures Count:      " << info->textures_count << endl;
	cout << "Materials Count:     " << info->materials_count << endl;
	cout << "Skeletons Count:     " << info->skeletons_count << endl;
	cout << "VertexDatas Count:   " << info->vertex_datas_count << endl;
	cout << "TriTopologies Count: " << info->tri_topologies_count << endl;
	cout << "Meshes Count:        " << info->meshes_count << endl;
	cout << "Models Count:        " << info->models_count << endl;
	cout << "TrackGroups Count:   " << info->track_groups_count << endl;
	cout << "Animations Count:    " << info->animations_count << endl;
	cout << endl;
}

void print_indent(int num_spaces)
{
	while (num_spaces > 0) {
		cout << ' ';
		--num_spaces;
	}
}

void print_transform(GR2_transform& Transform, const char* label,
                     int indent = 0)
{
	print_indent(indent);
	cout << label << ":\n";
	print_indent(indent);
	cout << "  Flags: " << Transform.flags << endl;
	print_indent(indent);
	cout << "  Translation: ";
	print_vector(Transform.translation);
	cout << endl;
	print_indent(indent);
	cout << "  Rotation: ";
	print_vector(Transform.rotation);
	cout << endl;
	print_indent(indent);
	cout << "  ScaleShear: [" << Transform.scale_shear[0] << ", "
	     << Transform.scale_shear[3] << ", " << Transform.scale_shear[6]
	     << endl;
	print_indent(indent);
	cout << "               " << Transform.scale_shear[1] << ", "
	     << Transform.scale_shear[4] << ", " << Transform.scale_shear[7]
	     << endl;
	print_indent(indent);
	cout << "               " << Transform.scale_shear[2] << ", "
	     << Transform.scale_shear[5] << ", " << Transform.scale_shear[8]
	     << ']' << endl;
}

void print_extended_data_value(GR2_extended_data& extended_data, int k, int v)
{
	switch (extended_data.keys[k].type) {
	case GR2_type_int32:
		cout << extended_data.values[v].int32;
		break;
	case GR2_type_real32:
		cout << extended_data.values[v].real32;
		break;
	case GR2_type_text:
		cout << extended_data.values[v].text;
		break;
	default:
		cout << "# WARNING: Unhandled case\n";
	}
}

void print_extended_data(GR2_extended_data& extended_data)
{
	if (!extended_data.keys)
		return;

	cout << "    ExtendedData:\n";

	for (int k = 0, v = 0; extended_data.keys[k].type != GR2_type_none;
	     ++k) {
		cout << "      " << extended_data.keys[k].name << ':';

		int n = extended_data.keys[k].length;
		if (n == 0)
			n = 1;

		while (n > 0) {
			cout << ' ';
			print_extended_data_value(extended_data, k, v);
			++v;
			--n;
		}

		cout << endl;
	}
}

void print_bone(GR2_bone& bone)
{
	cout << "- Name: " << bone.name << endl;
	cout << "  ParentIndex: " << bone.parent_index << endl;
	print_transform(bone.transform, "Transform", 2);
	cout << "  InverseWorldTransform: [";

	for (int i = 0; i < 16; ++i) {
		cout << bone.inverse_world_transform[i];

		if ((i + 1)%4 == 0) {
			if (i == 15)
				cout << "]\n";
			else
				cout << "\n                          ";
		}
		else {
			cout << ", ";
		}
	}

	print_extended_data(bone.extended_data);
}

void print_bones(GR2_skeleton* skel)
{
	for (int i = 0; i < skel->bones_count; ++i) {
		cout << "# " << i << endl;
		print_bone(skel->bones[i]);
	}
}

void print_skeleton(GR2_skeleton* skel)
{
	cout << "Skeleton\n";
	cout << "========\n";
	cout << "Name:        " << skel->name << endl;
	cout << "Bones Count: " << skel->bones_count << endl;
	print_bones(skel);
	cout << endl;
}

void print_skeletons(GR2_file_info* info)
{
	for (int i = 0; i < info->skeletons_count; ++i)
		print_skeleton(info->skeletons[i]);
}

void print_model(GR2_model* model)
{
	cout << "Model\n";
	cout << "=====\n";
	cout << "Name:     " << model->name << endl;
	cout << "Skeleton: ";

	if (model->skeleton)
		cout << model->skeleton->name << endl;
	else
		cout << "NULL\n";

	print_transform(model->initial_placement, "Initial Placement");
	cout << "Mesh Bindings Count: " << model->mesh_bindings_count << endl;
	cout << endl;
}

void print_models(GR2_file_info* info)
{
	for (int i = 0; i < info->models_count; ++i)
		print_model(info->models[i]);
}

void print_offsets(float offsets[4])
{
	cout << "    Offsets: ";
	print_float4(offsets);
	cout << endl;
}

void print_scales(float scales[4])
{
	cout << "    Scales: ";
	print_float4(scales);
	cout << endl;
}

void print_selectors(uint16_t selectors[4])
{
	cout << "    Selectors: [";
	for (int i = 0; i < 4; ++i) {
		cout << selectors[i];
		if (i < 3)
			cout << ", ";
	}
	cout << "]\n";
}

template <typename T>
void print_knots(const T& view)
{
	cout << "    Knots: # " << view.knots().size() << endl;
	for (unsigned i = 0; i < view.knots().size(); ++i) {
		cout << "      - " << +view.encoded_knots()[i] << " # "
		     << view.knots()[i] << endl;
	}
}

template <typename T>
void print_controls(const T& view)
{
	cout << "    Controls: # " << view.controls().size() << endl;
	for (unsigned i = 0; i < view.controls().size(); ++i) {
		cout << "      - ";
		print_vector(view.encoded_controls()[i]);
		cout << " # ";
		print_vector(view.controls()[i]);
		cout << endl;
	}
}

void print_curve_positions(GR2_curve_data_DaK32fC32f* data)
{
	for (int i = 0; i < data->knots_count; ++i) {
		float x = data->controls[i * 3 + 0];
		float y = data->controls[i * 3 + 1];
		float z = data->controls[i * 3 + 2];
		cout << "      - [" << x << ", " << y << ", " << z
			<< "]\n";
	}
}

void print_curve_rotations(GR2_curve_data_DaK32fC32f* data)
{
	for (int i = 0; i < data->knots_count; ++i) {
		float x = data->controls[i * 4 + 0];
		float y = data->controls[i * 4 + 1];
		float z = data->controls[i * 4 + 2];
		float w = data->controls[i * 4 + 3];
		cout << "      - [" << x << ", " << y << ", " << z
			<< ", " << w << "]\n";
	}
}

void print_curve_scaleshear(GR2_curve_data_DaK32fC32f* data)
{
	for (int i = 0; i < data->knots_count; ++i) {
		cout << "      - [";
		for (int j = 0; j < 9; ++j) {
			cout << data->controls[i * 9 + j];
			if (j != 8)
				cout << ", ";
		}
		cout << "]\n";
	}
}

void print_curve_data(GR2_curve_data_DaK32fC32f* data)
{
	cout << "    Padding: " << data->padding << endl;
	cout << "    Knots: # " << data->knots_count << endl;
	for (int i = 0; i < data->knots_count; ++i)
		cout << "      - " << data->knots[i] << endl;
	cout << "    Controls: # " << data->controls_count << endl;

	if (data->knots_count * 3 == data->controls_count)
		print_curve_positions(data);
	else if (data->knots_count * 4 == data->controls_count)
		print_curve_rotations(data);
	else if (data->knots_count * 9 == data->controls_count)
		print_curve_scaleshear(data);
	else
		cout << "# WARNING: Unhandled case\n";
}

void print_curve_data(GR2_curve_data_DaConstant32f* data)
{
	cout << "    Controls: #" << data->controls_count << endl;
		
	for (int i = 0; i < data->controls_count; ++i)
		cout << "      - " << data->controls[i] << endl;	
}

void print_curve_data(GR2_curve_data_DaK16uC16u* data)
{
	cout << "    OneOverKnotScaleTrunc: " << data->one_over_knot_scale_trunc << endl;
	cout << "    ControlScaleOffsets: # " << data->control_scale_offsets_count << endl;

	for (int i = 0; i < data->control_scale_offsets_count; ++i)
		cout << "      - " << data->control_scale_offsets[i] << endl;

	GR2_DaK16uC16u_view view(*data);

	print_knots(view);		
	
	cout << "    Controls: # " << view.controls().size() << endl;	

	for (unsigned i = 0; i < view.controls().size(); ++i)
		cout << "      - " << view.encoded_controls()[i] << " # " << view.controls()[i] << endl;
}

void print_curve_data(GR2_curve_data_D4nK16uC15u* data)
{
	cout << "    ScaleOffsetTableEntries: "
	     << data->scale_offset_table_entries << endl;

	GR2_D4nK16uC15u_view view(*data);

	print_selectors(view.selectors);
	print_scales(view.scales);
	print_offsets(view.offsets);
	cout << "    OneOverKnotScale: " << data->one_over_knot_scale << endl;
	cout << "    KnotsControls_count: " << data->knots_controls_count
	     << endl;

	print_knots(view);
	print_controls(view);
}

void print_curve_data(GR2_curve_data_D4nK8uC7u* data)
{
	cout << "    ScaleOffsetTableEntries: "
	     << data->scale_offset_table_entries << endl;

	GR2_D4nK8uC7u_view view(*data);

	print_selectors(view.selectors);
	print_scales(view.scales);
	print_offsets(view.offsets);
	cout << "    OneOverKnotScale: " << data->one_over_knot_scale << endl;
	cout << "    KnotsControls_count: " << data->knots_controls_count
	     << endl;

	print_knots(view);
	print_controls(view);
}

void print_curve_data(GR2_curve_data_D3K16uC16u* data)
{
	cout << "    OneOverKnotScaleTrunc: " << data->one_over_knot_scale_trunc
		<< endl;
	cout << "    ControlScales: [" << data->control_scales[0] << ", "
		<< data->control_scales[1] << ", " << data->control_scales[2]
		<< "]\n";
	cout << "    ControlOffsets: [" << data->control_offsets[0] << ", "
		<< data->control_offsets[1] << ", " << data->control_offsets[2]
		<< "]\n";
	cout << "    KnotsControls_count: " << data->knots_controls_count
		<< endl;

	GR2_D3K16uC16u_view view(*data);

	print_knots(view);
	print_controls(view);
}

void print_curve_data(GR2_curve_data_D3K8uC8u* data)
{
	cout << "    OneOverKnotScaleTrunc: " << data->one_over_knot_scale_trunc
	     << endl;
	cout << "    ControlScales: [" << data->control_scales[0] << ", "
	     << data->control_scales[1] << ", " << data->control_scales[2]
	     << "]\n";
	cout << "    ControlOffsets: [" << data->control_offsets[0] << ", "
	     << data->control_offsets[1] << ", " << data->control_offsets[2]
	     << "]\n";
	cout << "    KnotsControls_count: " << data->knots_controls_count
	     << endl;

	GR2_D3K8uC8u_view view(*data);

	print_knots(view);
	print_controls(view);
}

void print_curve_data(GR2_curve_data_DaKeyframes32f* data)
{
	cout << "    Dimension: " << data->dimension << endl;
	cout << "    Controls: # " << data->controls_count << endl;

	if (data->dimension == 3) {
		for (int i = 0; i < data->controls_count/3; ++i) {
			float x = data->controls[i * 3 + 0];
			float y = data->controls[i * 3 + 1];
			float z = data->controls[i * 3 + 2];
			cout << "      - [" << x << ", " << y << ", " << z
				<< "]\n";
		}
	}
	else if (data->dimension == 4) {
		for (int i = 0; i < data->controls_count/4; ++i) {
			float x = data->controls[i * 4 + 0];
			float y = data->controls[i * 4 + 1];
			float z = data->controls[i * 4 + 2];
			float w = data->controls[i * 4 + 3];
			cout << "      - [" << x << ", " << y << ", " << z
				<< ", " << w << "]\n";
		}
	}
	else if (data->dimension == 9) {
		for (int i = 0; i < data->controls_count/9; ++i) {
			cout << "      - [";
			for (int j = 0; j < 9; ++j) {
				cout << data->controls[i * 9 + j];
				if (j != 8)
					cout << ", ";
			}
			cout << "]\n";
		}
	}
	else {
		cout << "# WARNING: Unhandled case\n";
	}
}

void print_curve(GR2_curve& curve)
{
	cout << "    Format: " << +curve.curve_data->curve_data_header.format;
	cout << " ("
	     << curve_format_to_str(curve.curve_data->curve_data_header.format)
	     << ")\n";
	cout << "    Degree: " << +curve.curve_data->curve_data_header.degree
	     << endl;

	if (curve.curve_data->curve_data_header.format == DaK32fC32f) {
		GR2_curve_data_DaK32fC32f* data =
		    (GR2_curve_data_DaK32fC32f*)curve.curve_data.get();
		print_curve_data(data);
	}
	else if (curve.curve_data->curve_data_header.format == DaIdentity) {
		GR2_curve_data_DaIdentity* data =
		    (GR2_curve_data_DaIdentity*)curve.curve_data.get();
		cout << "    Dimension: " << data->dimension << endl;
	}
	else if (curve.curve_data->curve_data_header.format == DaConstant32f) {
		GR2_curve_data_DaConstant32f* data =
			(GR2_curve_data_DaConstant32f*)curve.curve_data.get();
		print_curve_data(data);		
	}
	else if (curve.curve_data->curve_data_header.format == DaK16uC16u) {
		GR2_curve_data_DaK16uC16u* data =
			(GR2_curve_data_DaK16uC16u*)curve.curve_data.get();
		print_curve_data(data);
	}
	else if (curve.curve_data->curve_data_header.format == D3Constant32f) {
		GR2_curve_data_D3Constant32f* data =
		    (GR2_curve_data_D3Constant32f*)curve.curve_data.get();
		cout << "    Padding: " << data->padding << endl;
		cout << "    Controls: [" << data->controls[0] << ", "
		     << data->controls[1] << ", " << data->controls[2] << "]\n";
	}
	else if (curve.curve_data->curve_data_header.format == D4Constant32f) {
		GR2_curve_data_D4Constant32f* data =
		    (GR2_curve_data_D4Constant32f*)curve.curve_data.get();
		cout << "    Padding: " << data->padding << endl;
		cout << "    Controls: [" << data->controls[0] << ", "
		     << data->controls[1] << ", " << data->controls[2] << ", "
		     << data->controls[3] << "]\n";
	}
	else if (curve.curve_data->curve_data_header.format == D4nK16uC15u) {
		GR2_curve_data_D4nK16uC15u* data =
		    (GR2_curve_data_D4nK16uC15u*)curve.curve_data.get();
		print_curve_data(data);
	}
	else if (curve.curve_data->curve_data_header.format == D4nK8uC7u) {
		GR2_curve_data_D4nK8uC7u* data =
		    (GR2_curve_data_D4nK8uC7u*)curve.curve_data.get();
		print_curve_data(data);
	}
	else if (curve.curve_data->curve_data_header.format == D3K16uC16u) {
		GR2_curve_data_D3K16uC16u* data =
			(GR2_curve_data_D3K16uC16u*)curve.curve_data.get();
		print_curve_data(data);
	}
	else if (curve.curve_data->curve_data_header.format == D3K8uC8u) {
		GR2_curve_data_D3K8uC8u* data =
		    (GR2_curve_data_D3K8uC8u*)curve.curve_data.get();
		print_curve_data(data);
	}
	else if (curve.curve_data->curve_data_header.format == DaKeyframes32f) {
		GR2_curve_data_DaKeyframes32f* data =
		    (GR2_curve_data_DaKeyframes32f*)curve.curve_data.get();
		print_curve_data(data);
	}
	else {
		cout << "# WARNING: Unhandled case\n";
	}
}

void print_vector_tracks(GR2_track_group* track_group)
{
	cout << "VectorTracks Count: " << track_group->vector_tracks_count
	     << endl;

	for (int32_t i = 0; i < track_group->vector_tracks_count; ++i) {
		auto& vector_track = track_group->vector_tracks[i];
		cout << "- Name: " << vector_track.name << endl;
		cout << "  Dimension: " << vector_track.dimension << endl;
		// print_curve(vector_track.ValueCurve);
	}
}

void print_transform_track(GR2_transform_track& tt)
{
	cout << "- Name: " << tt.name << endl;
	cout << "  PositionCurve:\n";
	print_curve(tt.position_curve);
	cout << "  OrientationCurve:\n";
	print_curve(tt.orientation_curve);
	cout << "  ScaleShearCurve:\n";
	print_curve(tt.scale_shear_curve);	
}

void print_transform_tracks(GR2_track_group* track_group)
{
	cout << "TransformTracks Count: " << track_group->transform_tracks_count
	     << endl;

	for (int32_t i = 0; i < track_group->transform_tracks_count; ++i)
		print_transform_track(track_group->transform_tracks[i]);
}

void print_transform_LOD_errors(GR2_track_group* track_group)
{
	cout << "TransformLODErrors Count: "
	     << track_group->transform_LOD_errors_count << endl;

	for (int32_t i = 0; i < track_group->transform_LOD_errors_count; ++i)
		cout << "- " << track_group->transform_LOD_errors[i] << endl;
}

void print_trackgroup(GR2_track_group* track_group)
{
	cout << "TrackGroup\n";
	cout << "----------\n";
	cout << "Name:               " << track_group->name << endl;
	print_vector_tracks(track_group);
	print_transform_tracks(track_group);
	print_transform_LOD_errors(track_group);
	cout << "TextTracks Count: " << track_group->text_tracks_count << endl;
	cout << endl;
}

void print_trackgroups(GR2_file_info* info)
{
	for (int i = 0; i < info->track_groups_count; ++i) {
		cout << endl;
		print_trackgroup(info->track_groups[i]);
	}
}

void print_animation(GR2_animation* animation)
{
	cout << "Animation\n";
	cout << "=========\n";
	cout << "Name:              " << animation->name << endl;
	cout << "Duration:          " << animation->duration << endl;
	cout << "Time Step:         " << animation->time_step << endl;
	cout << "Oversampling:      " << animation->oversampling << endl;
	cout << "TrackGroups:       " << animation->track_groups_count << endl;
	cout << endl;

	for (int32_t i = 0; i < animation->track_groups_count; ++i)
		print_trackgroup(animation->track_groups[i]);

	cout << endl;
}

void print_animations(GR2_file_info* info)
{
	for (int i = 0; i < info->animations_count; ++i)
		print_animation(info->animations[i]);
}

void print_header(GR2_file& gr2)
{
	cout << "Header\n";
	cout << "======\n";
	cout << "Magic:          ";
	for (int i = 0; i < 4; ++i)
		cout << ' ' << hex << gr2.header.magic[i] << dec;
	cout << endl;
	cout << "Header size:     " << gr2.header.size << endl;
	cout << "Format:          " << gr2.header.format << endl;
	cout << "Reserved:        " << gr2.header.reserved[0] << ' '
	     << gr2.header.reserved[1] << endl;
	cout << "Version:         " << gr2.header.info.version << endl;
	cout << "File size:       " << gr2.header.info.file_size << endl;
	cout << "CRC32:           " << hex << gr2.header.info.crc32 << dec
	     << endl;
	cout << "Sections offset: " << gr2.header.info.sections_offset << endl;
	cout << "Sections:        " << gr2.header.info.sections_count << endl;
	cout << "Type section:    " << gr2.header.info.type_section << endl;
	cout << "Type offset:     " << gr2.header.info.type_offset << endl;
	cout << "Root section:    " << gr2.header.info.root_section << endl;
	cout << "Root offset:     " << gr2.header.info.root_offset << endl;
	cout << "Tag:             0x" << hex << gr2.header.info.tag << dec
	     << endl;
	cout << "Extra:           " << gr2.header.info.extra[0] << ' '
	     << gr2.header.info.extra[1] << ' ' << gr2.header.info.extra[2]
	     << ' ' << gr2.header.info.extra[3] << endl;
	cout << endl;
}

void print_section(GR2_file::Section_header& section)
{
	cout << "Section\n";
	cout << "=======\n";
	cout << "Compression:         " << section.compression << endl;
	cout << "Data offset:         " << section.data_offset << endl;
	cout << "Data size:           " << section.data_size << endl;
	cout << "Decompressed size:   " << section.decompressed_size << endl;
	cout << "Alignment:           " << section.alignment << endl;
	cout << "First 16 bit:        " << section.first16bit << endl;
	cout << "First 8 bit:         " << section.first8bit << endl;
	cout << "Relocations offset:  " << section.relocations_offset << endl;
	cout << "Relocations count:   " << section.relocations_count << endl;
	cout << "Marshallings offset: " << section.marshallings_offset << endl;
	cout << "Marshallings count:  " << section.marshallings_count << endl;
	cout << endl;
}

void print_sections(GR2_file& gr2)
{
	for (auto& section : gr2.section_headers)
		print_section(section);
}

void print_gr2(GR2_file& gr2)
{
	print_header(gr2);
	print_sections(gr2);
	print_file_info(gr2.file_info);
	print_art_tool_info(gr2.file_info->art_tool_info);
	print_exporter_info(gr2.file_info->exporter_info);
	print_skeletons(gr2.file_info);
	print_models(gr2.file_info);
	print_animations(gr2.file_info);
}

void print_var(GR2_property_key* key, const char *name, std::map<GR2_property_key*, std::string>& visited)
{
	if (visited.find(key) != visited.end())
		return;

	visited[key] = name;

	for (auto k = key; k->type != 0; ++k)
		if(k->keys)
			print_var(k->keys, k->name, visited);	

	cout << "static GR2_property_key " << name << "_def[] = {\n";		
	for (auto k = key; k->type != 0; ++k) {
		cout << "\t{ ";
		cout << "GR2_property_type(" << k->type << "), ";
		cout << '"' << k->name << "\", ";
		if (k->keys)
			//cout << k->name << "_def /* " << k->keys << "*/, ";
			cout << visited[k->keys] << "_def, ";
		else
			cout << "nullptr, ";
		cout << k->length << ", ";
		cout << k->unknown1 << ", ";
		cout << k->unknown2 << ", ";
		cout << k->unknown3 << ", ";
		cout << k->unknown4;
		cout << " },\n";
	}
	cout << "\t{ GR2_type_none, 0, 0, 0, 0, 0, 0, 0}\n";
	cout << "};\n\n";
}

void print_var(GR2_property_key* k)
{
	std::map<GR2_property_key*, std::string> visited;
	print_var(k, "gr2_type", visited);	
}

int main(int argc, char* argv[])
{
	if (argc < 2) {
		cout << "Usage: dumpgr2 <file>\n";
		return 1;
	}

	GR2_file::granny2dll_filename = "C:\\GOG Games\\Neverwinter Nights 2 Complete\\granny2.dll";

	GR2_file gr2(argv[1]);
	if (!gr2) {
		cout << gr2.error_string() << endl;
		return 1;
	}

	print_gr2(gr2);	

	return 0;
}
