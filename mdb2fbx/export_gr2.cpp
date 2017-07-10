#include <iostream>
#include <string>

#include "export_gr2.h"
#include "gr2_file.h"

using namespace std;

static FbxVector4 quat_to_euler(FbxQuaternion &q)
{
	FbxAMatrix m;
	m.SetQ(q);
	return m.GetR();
}

void export_bones(FbxScene *scene, FbxNode *parent_node, GR2_skeleton *skel,
	int32_t parent_index, std::vector<FbxNode*> &fbx_bones);

static void export_bone(FbxScene *scene, FbxNode *parent_node, GR2_skeleton *skel,
	int32_t bone_index, std::vector<FbxNode*> &fbx_bones)
{
	GR2_bone &bone = skel->bones[bone_index];
	cout << "    Exporting bone: " << bone.name << endl;
	auto node = FbxNode::Create(scene, bone.name);

	node->LclTranslation.Set(FbxDouble3(bone.transform.translation[0],
		bone.transform.translation[1],
		bone.transform.translation[2]));
	if (bone.transform.flags & 0x2) {
		FbxQuaternion rotation(bone.transform.rotation[0],
			bone.transform.rotation[1],
			bone.transform.rotation[2],
			bone.transform.rotation[3]);
		node->LclRotation.Set(quat_to_euler(rotation));
	}

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

static void process_fbx_bones(std::vector<FbxNode*> &fbx_bones)
{
	FbxNode *ribcage = nullptr;
	for (auto it = fbx_bones.begin(); it != fbx_bones.end();) {
		if (strncmp((*it)->GetName(), "ap_", 3) == 0) {
			// Remove "ap_..." bones as they are not used in skinning.
			it = fbx_bones.erase(it);
		}
		else if (strcmp((*it)->GetName(), "Ribcage") == 0) {
			// Erase "Ribcage" bone and keep it to reinsert later.
			ribcage = *it;
			it = fbx_bones.erase(it);
		}
		else
			++it;
	}

	// Reinsert "Ribcage bone"
	if (ribcage)
		fbx_bones.insert(fbx_bones.begin() + 53, ribcage);
}

static void export_skeleton(FbxScene *scene, GR2_skeleton *skel,
	std::vector<FbxNode*> &fbx_bones)
{
	cout << "  Exporting: " << skel->name << endl;

	auto node = FbxNode::Create(scene, (string(skel->name) + ".gr2").c_str());
	node->LclRotation.Set(FbxDouble3(-90, 0, 0));
	node->LclScaling.Set(FbxDouble3(1, 1, 1));

	auto null_attr = FbxNull::Create(scene, skel->name);
	node->SetNodeAttribute(null_attr);

	scene->GetRootNode()->AddChild(node);

	export_bones(scene, node, skel, -1, fbx_bones);

	process_fbx_bones(fbx_bones);
}

static void export_skeletons(FbxScene *scene, GR2_file_info *info,
	std::vector<FbxNode*> &fbx_bones)
{
	for (int i = 0; i < info->skeletons_count; ++i) {
		export_skeleton(scene, info->skeletons[i], fbx_bones);
	}
}

void create_anim_position(FbxNode *node, FbxAnimLayer *anim_layer,
	GR2_transform_track &transform_track)
{
	GR2_curve_view view(transform_track.position_curve);

	auto curvex = node->LclTranslation.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_X, true);
	curvex->KeyModifyBegin();

	for (unsigned i = 0; i < view.knots().size(); ++i) {
		if (i > 0 && view.knots()[i] <= view.knots()[i - 1]) continue;

		FbxTime time;
		time.SetSecondDouble(view.knots()[i]);
		auto k = curvex->KeyAdd(time);
		curvex->KeySetValue(k, view.controls()[i].x);
	}

	curvex->KeyModifyEnd();

	auto curvey = node->LclTranslation.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_Y, true);
	curvey->KeyModifyBegin();

	for (unsigned i = 0; i < view.knots().size(); ++i) {
		if (i > 0 && view.knots()[i] <= view.knots()[i - 1]) continue;

		FbxTime time;
		time.SetSecondDouble(view.knots()[i]);
		auto k = curvey->KeyAdd(time);
		curvey->KeySetValue(k, view.controls()[i].y);
	}

	curvey->KeyModifyEnd();

	auto curvez = node->LclTranslation.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_Z, true);
	curvez->KeyModifyBegin();

	for (unsigned i = 0; i < view.knots().size(); ++i) {
		if (i > 0 && view.knots()[i] <= view.knots()[i - 1]) continue;

		FbxTime time;
		time.SetSecondDouble(view.knots()[i]);
		auto k = curvez->KeyAdd(time);
		curvez->KeySetValue(k, view.controls()[i].z);
	}

	curvez->KeyModifyEnd();
}

void create_anim_rotation(FbxNode *node, FbxAnimLayer *anim_layer, GR2_transform_track &transform_track)
{
	GR2_curve_view view(transform_track.orientation_curve);

	std::vector<FbxVector4> rotations;
	for (auto &c : view.controls())
		rotations.push_back(quat_to_euler(FbxQuaternion(c.x, c.y, c.z, c.w)));

	auto curvex = node->LclRotation.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_X, true);
	curvex->KeyModifyBegin();

	for (unsigned i = 0; i < view.knots().size(); ++i) {
		if (i > 0 && view.knots()[i] <= view.knots()[i - 1]) continue;

		FbxTime time;
		time.SetSecondDouble(view.knots()[i]);
		auto k = curvex->KeyAdd(time);
		curvex->KeySetValue(k, float(rotations[i][0]));
	}

	curvex->KeyModifyEnd();

	auto curvey = node->LclRotation.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_Y, true);
	curvey->KeyModifyBegin();

	for (unsigned i = 0; i < view.knots().size(); ++i) {
		if (i > 0 && view.knots()[i] <= view.knots()[i - 1]) continue;

		FbxTime time;
		time.SetSecondDouble(view.knots()[i]);
		auto k = curvey->KeyAdd(time);
		curvey->KeySetValue(k, float(rotations[i][1]));
	}

	curvey->KeyModifyEnd();

	auto curvez = node->LclRotation.GetCurve(anim_layer, FBXSDK_CURVENODE_COMPONENT_Z, true);
	curvez->KeyModifyBegin();

	for (unsigned i = 0; i < view.knots().size(); ++i) {
		if (i > 0 && view.knots()[i] <= view.knots()[i - 1]) continue;

		FbxTime time;
		time.SetSecondDouble(view.knots()[i]);
		auto k = curvez->KeyAdd(time);
		curvez->KeySetValue(k, float(rotations[i][2]));
	}

	curvez->KeyModifyEnd();
}

void export_animation(FbxScene *scene, GR2_transform_track &transform_track,
	FbxAnimLayer *anim_layer)
{	
	auto node = scene->FindNodeByName(transform_track.name);	
	if (node) {		
		create_anim_position(node, anim_layer, transform_track);
		create_anim_rotation(node, anim_layer, transform_track);
	}
}

static void export_animation(FbxScene *scene, GR2_track_group *track_group,
	FbxAnimLayer *anim_layer)
{
	for (int i = 0; i < track_group->transform_tracks_count; ++i)
		export_animation(scene, track_group->transform_tracks[i]
			, anim_layer);
}

static void export_animation(FbxScene *scene, GR2_animation *anim)
{
	auto anim_stack = FbxAnimStack::Create(scene, anim->name);
	auto anim_layer = FbxAnimLayer::Create(scene, "Layer");
	anim_stack->AddMember(anim_layer);

	for (int i = 0; i < anim->track_groups_count; ++i)
		export_animation(scene, anim->track_groups[i], anim_layer);
}

static void export_animations(FbxScene *scene, GR2_file_info *info)
{
	for (int i = 0; i < info->animations_count; ++i) {
		export_animation(scene, info->animations[i]);
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

	cout << endl;
	cout << "===\n";
	cout << "GR2\n";
	cout << "===\n\n";

	cout << "Skeletons: " << gr2.file_info->skeletons_count << endl;
	cout << endl;

	export_skeletons(scene, gr2.file_info, fbx_bones);
	export_animations(scene, gr2.file_info);
}