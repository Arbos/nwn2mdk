#pragma once

#include <cstdint>
#include <vector>

#include "cgmath.h"
#include "virtual_ptr.h"

enum GR2_property_type {
	GR2_type_none = 0,
	GR2_type_inline = 1,
	GR2_type_reference = 2,
	GR2_type_pointer = 3,
	GR2_type_array_of_references = 4,
	GR2_type_variant_reference = 5,
	GR2_type_text = 8,
	GR2_type_real32 = 10,
	GR2_type_uint8 = 12,
	GR2_type_int16 = 15,
	GR2_type_uint16 = 16,
	GR2_type_int32 = 19
};

enum GR2_curve_format {
	DaKeyframes32f = 0,
	DaK32fC32f = 1,
	DaIdentity = 2,
	DaConstant32f = 3,
	D3Constant32f = 4,
	D4Constant32f = 5,
	DaK16uC16u = 6,
	DaK8uC8u = 7,
	D4nK16uC15u = 8,
	D4nK8uC7u = 9,
	D3K16uC16u = 10,
	D3K8uC8u = 11
};

enum GR2_transform_flags {
	GR2_has_position = 0x01,
	GR2_has_rotation = 0x02,
	GR2_has_scale_shear = 0x04
};

struct GR2_transform {
	/// GR2_transform_flags
	uint32_t flags;
	Vector3<float> translation;
	/// Quaternion (x, y, z, w).
	Vector4<float> rotation;
	/// 3x3 Matrix (in column major order).
	float scale_shear[9];
};

struct GR2_property_key {
	GR2_property_type type;
	Virtual_ptr<const char> name;
	Virtual_ptr<GR2_property_key> keys;
	int32_t length;
	/// Always seems 0.
	int32_t unknown1;
	/// Always seems 0.
	int32_t unknown2;
	/// Always seems 0.
	int32_t unknown3;
	/// Always seems 0.
	int32_t unknown4;
};

union GR2_property_value {
	int32_t int32;
	float real32;
	Virtual_ptr<char> text;
};

struct GR2_extended_data {
	Virtual_ptr<GR2_property_key> keys;
	Virtual_ptr<GR2_property_value> values;
};

struct GR2_art_tool_info {
	Virtual_ptr<char> from_art_tool_name;
	int32_t art_tool_major_revision;
	int32_t art_tool_minor_revision;
	float units_per_meter;
	Vector3<float> origin;
	Vector3<float> right_vector;
	Vector3<float> up_vector;
	Vector3<float> back_vector;
};

struct GR2_exporter_info {
	Virtual_ptr<char> exporter_name;
	int32_t exporter_major_revision;
	int32_t exporter_minor_revision;
	int32_t exporter_customization;
	int32_t exporter_build_number;
};

struct GR2_bone {
	Virtual_ptr<char> name;
	int32_t parent_index;
	GR2_transform transform;
	float inverse_world_transform[16];
	/// Always seems 0.
	Virtual_ptr<void> light_info;
	/// Always seems 0.
	Virtual_ptr<void> camera_info;
	GR2_extended_data extended_data;
};

struct GR2_skeleton {
	Virtual_ptr<char> name;
	int32_t bones_count;
	Virtual_ptr<GR2_bone> bones;
};

struct GR2_model {
	Virtual_ptr<char> name;
	Virtual_ptr<GR2_skeleton> skeleton;
	GR2_transform initial_placement;
	/// Always seems 0.
	int32_t mesh_bindings_count;
	/// Always seems 0.
	Virtual_ptr<void> mesh_bindings;
};

struct GR2_curve_data_header {
	/// @see GR2_curve_format
	uint8_t format;
	/// Degree of the B-spline (0: constant; 1: linear; 2: quadratic; 3: cubic).
	uint8_t degree;
};

struct GR2_curve_data {
	GR2_curve_data_header curve_data_header;
};

struct GR2_curve_data_D3Constant32f {
	GR2_curve_data_header curve_data_header_D3Constant32f;
	/// Unused. It's declared here to pad the header, forcing proper alignment.
	int16_t padding;
	float controls[3];
};

struct GR2_curve_data_D4Constant32f {
	GR2_curve_data_header curve_data_header_D4Constant32f;
	/// Unused. It's declared here to pad the header, forcing proper alignment.
	int16_t padding;
	float controls[4];
};

struct GR2_curve_data_D3K16uC16u {
	GR2_curve_data_header curve_data_header_D3K16uC16u;
	uint16_t one_over_knot_scale_trunc;
	float control_scales[3];
	float control_offsets[3];
	int32_t knots_controls_count;
	Virtual_ptr<uint16_t> knots_controls;
};

class GR2_D3K16uC16u_view {
public:
	GR2_D3K16uC16u_view(GR2_curve_data_D3K16uC16u& data);

	const std::vector<uint16_t>& encoded_knots() const;
	const std::vector<float>& knots() const;
	const std::vector<Vector3<uint16_t>>& encoded_controls() const;
	const std::vector<Vector3<float>>& controls() const;

private:
	std::vector<uint16_t> encoded_knots_;
	std::vector<float> knots_;
	std::vector<Vector3<uint16_t>> encoded_controls_;
	std::vector<Vector3<float>> controls_;
};

struct GR2_curve_data_D3K8uC8u {
	GR2_curve_data_header curve_data_header_D3K8uC8u;
	uint16_t one_over_knot_scale_trunc;
	float control_scales[3];
	float control_offsets[3];
	int32_t knots_controls_count;
	Virtual_ptr<uint8_t> knots_controls;
};

class GR2_D3K8uC8u_view {
public:
	GR2_D3K8uC8u_view(GR2_curve_data_D3K8uC8u& data);

	const std::vector<uint8_t>& encoded_knots() const;
	const std::vector<float>& knots() const;
	const std::vector<Vector3<uint8_t>>& encoded_controls() const;
	const std::vector<Vector3<float>>& controls() const;

private:
	std::vector<uint8_t> encoded_knots_;
	std::vector<float> knots_;
	std::vector<Vector3<uint8_t>> encoded_controls_;
	std::vector<Vector3<float>> controls_;
};

struct GR2_curve_data_D4nK16uC15u {
	GR2_curve_data_header curve_data_header_D4nK16uC15u;
	uint16_t scale_offset_table_entries;
	float one_over_knot_scale;
	int32_t knots_controls_count;
	Virtual_ptr<uint16_t> knots_controls;
};

class GR2_D4nK16uC15u_view {
public:
	uint16_t selectors[4];
	float scales[4];
	float offsets[4];

	GR2_D4nK16uC15u_view(GR2_curve_data_D4nK16uC15u& data);

	const std::vector<uint16_t>& encoded_knots() const;
	const std::vector<float>& knots() const;
	const std::vector<Vector3<uint16_t>>& encoded_controls() const;
	const std::vector<Vector4<float>>& controls() const;

private:
	std::vector<uint16_t> encoded_knots_;
	std::vector<float> knots_;
	std::vector<Vector3<uint16_t>> encoded_controls_;
	std::vector<Vector4<float>> controls_;
};

struct GR2_curve_data_D4nK8uC7u {
	GR2_curve_data_header curve_data_header_D4nK8uC7u;
	uint16_t scale_offset_table_entries;
	float one_over_knot_scale;
	int32_t knots_controls_count;
	Virtual_ptr<uint8_t> knots_controls;
};

class GR2_D4nK8uC7u_view {
public:
	uint16_t selectors[4];
	float scales[4];
	float offsets[4];

	GR2_D4nK8uC7u_view(GR2_curve_data_D4nK8uC7u& data);

	const std::vector<uint8_t>& encoded_knots() const;
	const std::vector<float>& knots() const;
	const std::vector<Vector3<uint8_t>>& encoded_controls() const;
	const std::vector<Vector4<float>>& controls() const;

private:
	std::vector<uint8_t> encoded_knots_;
	std::vector<float> knots_;
	std::vector<Vector3<uint8_t>> encoded_controls_;
	std::vector<Vector4<float>> controls_;
};

struct GR2_curve_data_DaIdentity {
	GR2_curve_data_header curve_data_header_DaIdentity;
	int16_t dimension;
};

struct GR2_curve_data_DaConstant32f {
	GR2_curve_data_header curve_data_header_DaConstant32f;
	/// Unused. It's declared here to pad the header, forcing proper alignment.
	int16_t padding;
	int32_t controls_count;
	Virtual_ptr<float> controls;
};

struct GR2_curve_data_DaK16uC16u {
	GR2_curve_data_header curve_data_header_DaK16uC16u;
	uint16_t one_over_knot_scale_trunc;
	int32_t control_scale_offsets_count;
	Virtual_ptr<float> control_scale_offsets;
	int32_t knots_controls_count;
	Virtual_ptr<uint16_t> knots_controls;
};

class GR2_DaK16uC16u_view {
public:
	GR2_DaK16uC16u_view(GR2_curve_data_DaK16uC16u& data);
	
	const std::vector<uint16_t>& encoded_knots() const;
	const std::vector<float>& knots() const;
	const std::vector<uint16_t>& encoded_controls() const;
	const std::vector<float>& controls() const;

private:
	std::vector<uint16_t> encoded_knots_;
	std::vector<float> knots_;
	std::vector<uint16_t> encoded_controls_;
	std::vector<float> controls_;
};

struct GR2_curve_data_DaK8uC8u {
	GR2_curve_data_header curve_data_header_DaK8uC8u;
	uint16_t one_over_knot_scale_trunc;
	int32_t control_scale_offsets_count;
	Virtual_ptr<float> control_scale_offsets;
	int32_t knots_controls_count;
	Virtual_ptr<uint8_t> knots_controls;
};

class GR2_DaK8uC8u_view {
public:
	GR2_DaK8uC8u_view(GR2_curve_data_DaK8uC8u& data);

	const std::vector<uint8_t>& encoded_knots() const;
	const std::vector<float>& knots() const;
	const std::vector<uint8_t>& encoded_controls() const;
	const std::vector<float>& controls() const;

private:
	std::vector<uint8_t> encoded_knots_;
	std::vector<float> knots_;
	std::vector<uint8_t> encoded_controls_;
	std::vector<float> controls_;
};

struct GR2_curve_data_DaK32fC32f {
	GR2_curve_data_header curve_data_header_DaK32fC32f;
	/// Unused. It's declared here to pad the header, forcing proper alignment.
	int16_t padding;
	int32_t knots_count;
	Virtual_ptr<float> knots;
	int32_t controls_count;
	Virtual_ptr<float> controls;
};

class GR2_DaK32fC32f_view {
public:
	GR2_DaK32fC32f_view(GR2_curve_data_DaK32fC32f& data);
	
	const std::vector<float>& knots() const;
	const std::vector<Vector4<float>>& controls() const;

private:
	std::vector<float> knots_;
	std::vector<Vector4<float>> controls_;
};

struct GR2_curve_data_DaKeyframes32f {
	GR2_curve_data_header curve_data_header_DaKeyframes32f;
	int16_t dimension;
	int32_t controls_count;
	Virtual_ptr<float> controls;
};

static_assert(sizeof(GR2_curve_data_DaKeyframes32f) == 12);

class GR2_DaKeyframes32f_view {
public:
	int16_t dimension;

	GR2_DaKeyframes32f_view(float duration,
	                        GR2_curve_data_DaKeyframes32f& data);

	const std::vector<float>& knots() const;
	const std::vector<Vector4<float>>& controls() const;

private:
	std::vector<float> knots_;
	std::vector<Vector4<float>> controls_;
};

struct GR2_curve {
	Virtual_ptr<GR2_property_key> keys;
	Virtual_ptr<GR2_curve_data> curve_data;
};

class GR2_curve_view {
public:
	GR2_curve_view(float duration, GR2_curve& curve);

	uint8_t degree() const;
	const std::vector<float>& knots() const;
	const std::vector<Vector4<float>>& controls() const;

private:
	uint8_t degree_;
	std::vector<float> knots_;
	std::vector<Vector4<float>> controls_;
};

struct GR2_vector_track {
	Virtual_ptr<char> name;
	int32_t dimension; // Flags?
	GR2_curve value_curve;
};

struct GR2_transform_track {
	Virtual_ptr<char> name;
	GR2_curve position_curve;
	GR2_curve orientation_curve;
	GR2_curve scale_shear_curve;
};

struct GR2_track_group {
	Virtual_ptr<char> name;

	/// NWN2 animations files have vector tracks, but they doesn't seem to
	/// be used.
	int32_t vector_tracks_count;
	Virtual_ptr<GR2_vector_track> vector_tracks;

	int32_t transform_tracks_count;
	Virtual_ptr<GR2_transform_track> transform_tracks;

	/// NWN2 animations files have transform LOD errors, but they doesn't
	/// seem to be used.
	int32_t transform_LOD_errors_count;
	Virtual_ptr<float> transform_LOD_errors;

	int32_t text_tracks_count;
	Virtual_ptr<void> text_tracks;

	GR2_transform initial_placement;
	int32_t accumulation_flags;
	float loop_translation[3];
	/// Always seems 0
	Virtual_ptr<void> periodic_loop;
	/// Always seems 0
	Virtual_ptr<void> root_motion;
	/// Seems always empty
	GR2_extended_data extended_data;
};

struct GR2_animation {
	Virtual_ptr<char> name;
	float duration;
	float time_step;
	float oversampling;
	int32_t track_groups_count;
	Virtual_ptr<Virtual_ptr<GR2_track_group>> track_groups;
};

struct GR2_file_info {
	Virtual_ptr<GR2_art_tool_info> art_tool_info;
	Virtual_ptr<GR2_exporter_info> exporter_info;
	Virtual_ptr<char> from_file_name;
	int32_t textures_count;
	/// Not used in NWN2, always 0.
	Virtual_ptr<Virtual_ptr<void>> textures;
	int32_t materials_count;
	/// Not used in NWN2, always 0.
	Virtual_ptr<Virtual_ptr<void>> materials;
	int32_t skeletons_count;
	Virtual_ptr<Virtual_ptr<GR2_skeleton>> skeletons;
	int32_t vertex_datas_count;
	/// Not used in NWN2, always 0.
	Virtual_ptr<Virtual_ptr<void>> vertex_datas;
	int32_t tri_topologies_count;
	/// Not used in NWN2, always 0.
	Virtual_ptr<Virtual_ptr<void>> tri_topologies;
	int32_t meshes_count;
	/// Not used in NWN2, always 0.
	Virtual_ptr<Virtual_ptr<void>> meshes;
	int32_t models_count;
	Virtual_ptr<Virtual_ptr<GR2_model>> models;
	int32_t track_groups_count;
	Virtual_ptr<Virtual_ptr<GR2_track_group>> track_groups;
	int32_t animations_count;
	Virtual_ptr<Virtual_ptr<GR2_animation>> animations;
	GR2_extended_data extended_data;
};

const char* curve_format_to_str(uint8_t format);
const char* property_type_to_str(GR2_property_type type);
