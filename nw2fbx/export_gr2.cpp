#include <iostream>
#include <string>

#include "export_gr2.h"
#include "export_info.h"
#include "gr2_file.h"
#include "memstream.h"

using namespace std;

const double time_step = 1 / 30.0;

static FbxVector4 quat_to_euler(FbxQuaternion &q)
{
	FbxAMatrix m;
	m.SetQ(q);
	return m.GetR();
}

static void export_bones(FbxScene* scene, FbxNode* parent_node,
                         GR2_skeleton* skel, int32_t parent_index,
                         std::vector<FbxNode*>& fbx_bones);

static void export_bone_translation(FbxNode* node, GR2_bone& bone)
{
	if (bone.transform.flags & 0x1) { // Has translation
		node->LclTranslation.Set(FbxDouble3(bone.transform.translation[0]/100,
			bone.transform.translation[1]/100,
			bone.transform.translation[2]/100));
	}
	else
		node->LclTranslation.Set(FbxDouble3(0, 0, 0));
}

static void export_bone_rotation(FbxNode* node, GR2_bone& bone)
{
	if (bone.transform.flags & 0x2) { // Has rotation
		FbxQuaternion rotation(bone.transform.rotation[0],
			bone.transform.rotation[1],
			bone.transform.rotation[2],
			bone.transform.rotation[3]);
		node->LclRotation.Set(quat_to_euler(rotation));
	}
	else
		node->LclRotation.Set(FbxDouble3(0, 0, 0));
}

static void export_bone_scaleshear(FbxNode* node, GR2_bone& bone)
{
	if (bone.transform.flags & 0x4) { // Has scale-shear
		// FBX doesn't support local shear. We only export scale.
		node->LclScaling.Set(FbxDouble3(bone.transform.scale_shear[0],
			bone.transform.scale_shear[4],
			bone.transform.scale_shear[8]));
	}
	else
		node->LclScaling.Set(FbxDouble3(1, 1, 1));
}

static void export_bone_transform(FbxNode* node, GR2_bone& bone)
{
	export_bone_translation(node, bone);
	export_bone_rotation(node, bone);
	export_bone_scaleshear(node, bone);	
}

static void export_bone(FbxScene *scene, FbxNode *parent_node, GR2_skeleton *skel,
	int32_t bone_index, std::vector<FbxNode*> &fbx_bones)
{
	GR2_bone &bone = skel->bones[bone_index];
	cout << "  Exporting bone: " << bone.name << endl;
	auto node = FbxNode::Create(scene, bone.name);
	export_bone_transform(node, bone);

	FbxSkeleton *skel_attr = FbxSkeleton::Create(scene, bone.name);
	skel_attr->SetSkeletonType(FbxSkeleton::eLimbNode);
	node->SetNodeAttribute(skel_attr);

	parent_node->AddChild(node);
	fbx_bones[bone_index] = node;

	export_bones(scene, node, skel, bone_index, fbx_bones);	
}

static void export_bones(FbxScene *scene, FbxNode *parent_node, GR2_skeleton *skel,
	int32_t parent_index, std::vector<FbxNode*> &fbx_bones)
{
	fbx_bones.resize(skel->bones_count);

	for (int32_t i = 0; i < skel->bones_count; ++i) {
		GR2_bone &bone = skel->bones[i];
		if (bone.parent_index == parent_index) {
			export_bone(scene, parent_node, skel, i, fbx_bones);			
		}
	}
}

static void export_skeleton(FbxScene *scene, GR2_skeleton *skel,
	std::vector<FbxNode*> &fbx_bones)
{
	cout << "Exporting skeleton: " << skel->name << endl;

	auto node = FbxNode::Create(scene, skel->name);
	node->LclRotation.Set(FbxDouble3(-90, 0, 0));
	node->LclScaling.Set(FbxDouble3(100, 100, 100));

	auto null_attr = FbxNull::Create(scene, skel->name);
	node->SetNodeAttribute(null_attr);

	scene->GetRootNode()->AddChild(node);

	export_bones(scene, node, skel, -1, fbx_bones);	
}

void export_skeletons(GR2_file& gr2, FbxScene* scene,
	std::vector<FbxNode*>& fbx_bones)
{
	for (int i = 0; i < gr2.file_info->models_count; ++i) {
		if(gr2.file_info->models[i]->skeleton)
			export_skeleton(scene,
			                gr2.file_info->models[i]->skeleton,
			                fbx_bones);
	}
}

std::vector<float> padded_knots(const std::vector<float>& knots, unsigned degree)
{
	std::vector<float> v;

	v.push_back(0);

	for (auto t : knots)
		v.push_back(t);

	for (unsigned i = 1; i <= degree; ++i) {
		v[i] = 0;
		v.push_back(knots.back());
	}

	return v;
}

// De Boor's algorithm to evaluate a B-spline
Vector3<float> de_boor_position(unsigned k, const std::vector<float>& knots, const std::vector<Vector4<float>>& controls, float t)
{	
	unsigned i = k;

	while (i < knots.size() - k - 1 && knots[i] <= t)
		++i;

	i = i - 1;

	std::vector<Vector3<float>> d;

	for (unsigned j = 0; j <= k; ++j) {
		auto r = controls[j + i - k];
		d.emplace_back(r.x, r.y, r.z);
	}

	for (unsigned r = 1; r <= k; ++r) {
		for (unsigned j = k; j >= r; --j) {
			float alpha = 0;
			if (knots[j + 1 + i - r] != knots[j + i - k])
				alpha = (t - knots[j + i - k]) / (knots[j + 1 + i - r] - knots[j + i - k]);
			d[j].x = d[j - 1].x * (1 - alpha) + d[j].x * alpha;
			d[j].y = d[j - 1].y * (1 - alpha) + d[j].y * alpha;
			d[j].z = d[j - 1].z * (1 - alpha) + d[j].z * alpha;
		}
	}

	return d[k];
}

Vector3<float> de_boor_position(unsigned degree, float t, GR2_curve_view &v)
{
	if (v.knots().size() == 1 || v.knots().size() < degree + 1)
		return Vector3<float>(v.controls()[0].x, v.controls()[0].y, v.controls()[0].z);

	return de_boor_position(degree, padded_knots(v.knots(), degree), v.controls(), t);
}

// De Boor's algorithm to evaluate a B-spline
FbxQuaternion de_boor_rotation(unsigned k, const std::vector<float>& knots, const std::vector<Vector4<float>>& controls, float t)
{
	unsigned i = k;

	while (i < knots.size() - k - 1 && knots[i] <= t)
		++i;

	i = i - 1;

	std::vector<FbxQuaternion> d;

	for (unsigned j = 0; j <= k; ++j) {
		auto r = controls[j + i - k];
		d.emplace_back(r.x, r.y, r.z, r.w);
	}

	for (unsigned r = 1; r <= k; ++r) {
		for (unsigned j = k; j >= r; --j) {
			float alpha = 1;
			if (knots[j + 1 + i - r] != knots[j + i - k])
				alpha = (t - knots[j + i - k]) / (knots[j + 1 + i - r] - knots[j + i - k]);
			d[j] = d[j - 1].Slerp(d[j], alpha);
		}
	}

	return d[k];
}

FbxQuaternion de_boor_rotation(unsigned degree, float t, GR2_curve_view &v)
{
	if (v.knots().size() == 1 || v.knots().size() < degree + 1)
		return FbxQuaternion(v.controls()[0].x, v.controls()[0].y, v.controls()[0].z, v.controls()[0].w);	

	return de_boor_rotation(degree, padded_knots(v.knots(), degree), v.controls(), t);
}

void add_position_keyframe(FbxAnimCurve* curvex, FbxAnimCurve* curvey,
	FbxAnimCurve* curvez, GR2_curve_view &view, float t)
{
	auto p = de_boor_position(view.degree(), t, view);

	FbxTime time;
	time.SetSecondDouble(t);

	auto k = curvex->KeyAdd(time);
	curvex->KeySetInterpolation(k, FbxAnimCurveDef::eInterpolationLinear);
	curvex->KeySetValue(k, p.x/100);

	k = curvey->KeyAdd(time);
	curvey->KeySetInterpolation(k, FbxAnimCurveDef::eInterpolationLinear);
	curvey->KeySetValue(k, p.y/100);

	k = curvez->KeyAdd(time);
	curvez->KeySetInterpolation(k, FbxAnimCurveDef::eInterpolationLinear);
	curvez->KeySetValue(k, p.z/100);
}

void create_anim_position(FbxNode *node, FbxAnimLayer *anim_layer, GR2_animation *anim,
	GR2_transform_track &transform_track)
{
	GR2_curve_view view(transform_track.position_curve);

	if (view.knots().empty())
		return;

	auto curvex = node->LclTranslation.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_X, true);	
	auto curvey = node->LclTranslation.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_Y, true);	
	auto curvez = node->LclTranslation.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_Z, true);	

	curvex->KeyModifyBegin();
	curvey->KeyModifyBegin();
	curvez->KeyModifyBegin();	

	for (double i = 0, t = 0; t < anim->duration + time_step / 2; ++i, t = i*time_step)
		add_position_keyframe(curvex, curvey, curvez, view, float(t));

	curvex->KeyModifyEnd();
	curvey->KeyModifyEnd();
	curvez->KeyModifyEnd();
}

void add_rotation_keyframe(FbxAnimCurve* curvex, FbxAnimCurve* curvey,
	FbxAnimCurve* curvez, GR2_curve_view &view, float t)
{
	auto r = de_boor_rotation(view.degree(), t, view);
	auto e = quat_to_euler(r);

	FbxTime time;
	time.SetSecondDouble(t);

	auto k = curvex->KeyAdd(time);
	curvex->KeySetInterpolation(k, FbxAnimCurveDef::eInterpolationLinear);
	curvex->KeySetValue(k, float(e[0]));

	k = curvey->KeyAdd(time);
	curvey->KeySetInterpolation(k, FbxAnimCurveDef::eInterpolationLinear);
	curvey->KeySetValue(k, float(e[1]));

	k = curvez->KeyAdd(time);
	curvez->KeySetInterpolation(k, FbxAnimCurveDef::eInterpolationLinear);
	curvez->KeySetValue(k, float(e[2]));
}

void create_anim_rotation(FbxNode *node, FbxAnimLayer *anim_layer, GR2_animation *anim, GR2_transform_track &transform_track)
{
	GR2_curve_view view(transform_track.orientation_curve);

	if (view.knots().empty())
		return;

	auto curvex = node->LclRotation.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_X, true);	
	auto curvey = node->LclRotation.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_Y, true);	
	auto curvez = node->LclRotation.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_Z, true);

	curvex->KeyModifyBegin();
	curvey->KeyModifyBegin();
	curvez->KeyModifyBegin();	

	for (double i = 0, t = 0; t < anim->duration + time_step / 2; ++i, t = i*time_step)
		add_rotation_keyframe(curvex, curvey, curvez, view, float(t));

	curvex->KeyModifyEnd();
	curvey->KeyModifyEnd();
	curvez->KeyModifyEnd();
}

std::pair<std::vector<float>, std::vector<float>> scaleshear_curve_view(GR2_transform_track &transform_track)
{
	std::vector<float> knots;
	std::vector<float> controls;

	if (transform_track.scale_shear_curve.curve_data->curve_data_header.format == DaConstant32f) {
		knots.push_back(0);
		auto data = (GR2_curve_data_DaConstant32f*)transform_track.scale_shear_curve.curve_data.get();
		for (int i = 0; i < 9; ++i)
			controls.push_back(data->controls[i]);
	}
	else if (transform_track.scale_shear_curve.curve_data->curve_data_header.format == DaK16uC16u) {
		auto data = (GR2_curve_data_DaK16uC16u*)transform_track.scale_shear_curve.curve_data.get();
		GR2_DaK16uC16u_view view(*data);
		return { view.knots(), view.controls() };
	}
	else if (transform_track.scale_shear_curve.curve_data->curve_data_header.format == DaK32fC32f) {
		auto data = (GR2_curve_data_DaK32fC32f*)transform_track.scale_shear_curve.curve_data.get();
		for (int i = 0; i < data->knots_count; ++i)
			knots.push_back(data->knots[i]);
		for (int i = 0; i < data->controls_count; ++i)
			controls.push_back(data->controls[i]);		
	}

	return { knots, controls };
}

void add_scaling_keyframe(FbxAnimCurve* curvex, FbxAnimCurve* curvey,
	FbxAnimCurve* curvez, std::vector<float>& knots, std::vector<float>& controls,
	unsigned i)
{
	FbxTime time;
	time.SetSecondDouble(knots[i]);

	auto k = curvex->KeyAdd(time);
	curvex->KeySetInterpolation(k, FbxAnimCurveDef::eInterpolationLinear);
	curvex->KeySetValue(k, controls[i * 9 + 0]);

	k = curvey->KeyAdd(time);
	curvey->KeySetInterpolation(k, FbxAnimCurveDef::eInterpolationLinear);
	curvey->KeySetValue(k, controls[i * 9 + 4]);

	k = curvez->KeyAdd(time);
	curvez->KeySetInterpolation(k, FbxAnimCurveDef::eInterpolationLinear);
	curvez->KeySetValue(k, controls[i * 9 + 8]);
}

void create_anim_scaling(FbxNode *node, FbxAnimLayer *anim_layer, GR2_animation *anim, GR2_transform_track &transform_track)
{	
	auto [knots, controls] = scaleshear_curve_view(transform_track);

	if (knots.empty())
		return;

	auto curvex = node->LclScaling.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_X, true);
	auto curvey = node->LclScaling.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_Y, true);
	auto curvez = node->LclScaling.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_Z, true);

	curvex->KeyModifyBegin();
	curvey->KeyModifyBegin();
	curvez->KeyModifyBegin();

	for (unsigned i = 0; i < knots.size(); ++i)
		add_scaling_keyframe(curvex, curvey, curvez, knots, controls, i);

	curvex->KeyModifyEnd();
	curvey->KeyModifyEnd();
	curvez->KeyModifyEnd();
}

static void create_animation(FbxNode* node,	FbxAnimLayer *anim_layer,
	GR2_animation *anim, GR2_transform_track &transform_track)
{
	create_anim_position(node, anim_layer, anim, transform_track);
	create_anim_rotation(node, anim_layer, anim, transform_track);
	create_anim_scaling(node, anim_layer, anim, transform_track);
}

static bool node_is_animated(FbxNode* node, GR2_transform_track &transform_track)
{
	return strcmp(node->GetName(), transform_track.name) == 0 &&
		(node->GetChildCount() == 0 || strcmp(node->GetName(), node->GetChild(0)->GetName()) != 0);
}

static FbxNode* create_animation_pivot(FbxNode* node, GR2_track_group *track_group)
{
	string pivot_name = track_group->name.get();
	if(strcmp(node->GetName(), track_group->name) == 0)
		pivot_name += ".PIVOT";

	auto pivot = node->GetScene()->FindNodeByName(pivot_name.c_str());

	if (!pivot) {
		pivot = FbxNode::Create(node->GetScene(), pivot_name.c_str());
		pivot->LclRotation.Set(FbxDouble3(-90, 0, 0));
		pivot->LclScaling.Set(FbxDouble3(100, 100, 100));
		node->GetScene()->GetRootNode()->AddChild(pivot);
	}

	return pivot;
}

static void parent_to_animation_pivot(FbxNode* node, GR2_track_group *track_group)
{
	if (!node->GetMesh())
		return; // Animation pivot is only for meshes

	auto pivot = create_animation_pivot(node, track_group);

	// Make node child of pivot
	node->GetParent()->RemoveChild(node);
	pivot->AddChild(node);

	// Reset rotation and scaling since the node is not root anymore
	node->LclRotation.Set(FbxDouble3(0, 0, 0));
	node->LclScaling.Set(FbxDouble3(1, 1, 1));
}

static bool export_animation(FbxNode* node, FbxAnimLayer *anim_layer,
	GR2_animation *anim, GR2_track_group *track_group, GR2_transform_track &transform_track)
{
	if (node_is_animated(node, transform_track)) {		
		create_animation(node, anim_layer, anim, transform_track);
		parent_to_animation_pivot(node, track_group);
		return true;
	}

	for (int i = 0; i < node->GetChildCount(); ++i)
		if (export_animation(node->GetChild(i), anim_layer, anim, track_group, transform_track))
			return true;

	return false;
}

static void export_animation(FbxScene *scene, GR2_animation *anim, GR2_track_group *track_group,
	FbxAnimLayer *anim_layer)
{
	cout << "  Exporting track group: " << track_group->name << endl;
	
	for (int i = 0; i < track_group->transform_tracks_count; ++i) {
		cout << "    Exporting transform track: " << track_group->transform_tracks[i].name << endl;

		export_animation(scene->GetRootNode(), anim_layer, anim, track_group, track_group->transform_tracks[i]);
	}
}

static void export_animation(FbxScene *scene, GR2_animation *anim)
{
	cout << "Exporting animation: " << anim->name << endl;

	auto anim_stack = FbxAnimStack::Create(scene, anim->name);
	auto anim_layer = FbxAnimLayer::Create(scene, "Layer");
	anim_stack->AddMember(anim_layer);

	for (int i = 0; i < anim->track_groups_count; ++i)
		export_animation(scene, anim, anim->track_groups[i], anim_layer);
}

void export_animations(GR2_file& gr2, FbxScene* scene)
{
	for (int i = 0; i < gr2.file_info->animations_count; ++i) {
		export_animation(scene, gr2.file_info->animations[i]);
	}
}

void export_gr2(const char *filename, FbxScene *scene,
	std::vector<FbxNode*> &fbx_bones)
{
	GR2_file gr2(filename);
	if (!gr2) {
		cout << gr2.error_string() << endl;
		return;
	}

	export_gr2(gr2, scene, fbx_bones);	
}

void export_gr2(GR2_file& gr2, FbxScene *scene,
	std::vector<FbxNode*> &fbx_bones)
{
	export_skeletons(gr2, scene, fbx_bones);
	export_animations(gr2, scene);
}

Dependency& export_gr2(Export_info& export_info, const char* filename)
{
	auto it = export_info.dependencies.find(filename);

	if (it != export_info.dependencies.end()) {
		// Already exported
		return it->second;
	}

	auto& dep = export_info.dependencies[filename];
	dep.exported = false;

	auto buffer = load_resource(export_info.config, filename);

	if (buffer.empty())
		return dep;

	Memstream stream(buffer.data(), buffer.size());
	GR2_file gr2(stream);

	if (!gr2)
		return dep;

	export_gr2(gr2, export_info.scene, dep.fbx_bones);
	process_fbx_bones(dep);
	dep.exported = true;

	return dep;
}

void print_gr2_info(GR2_file& gr2)
{
	cout << endl;
	cout << "===\n";
	cout << "GR2\n";
	cout << "===\n";
	cout << "Skeletons: " << gr2.file_info->skeletons_count << endl;
	cout << "Models: " << gr2.file_info->models_count << endl;
	cout << "Animations: " << gr2.file_info->animations_count << endl;
	cout << endl;
}
