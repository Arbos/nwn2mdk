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
	int32_t parent_index);

static void export_bone(FbxScene *scene, FbxNode *parent_node, GR2_skeleton *skel,
	int32_t bone_index)
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
	export_bones(scene, node, skel, bone_index);	
}

static void export_bones(FbxScene *scene, FbxNode *parent_node, GR2_skeleton *skel,
	int32_t parent_index)
{
	bool has_children = false;

	for (int32_t i = 0; i < skel->bones_count; ++i) {
		GR2_bone &bone = skel->bones[i];
		if (bone.parent_index == parent_index) {
			export_bone(scene, parent_node, skel, i);
			has_children = true;
		}
	}
}

static void export_skeleton(FbxScene *scene, GR2_skeleton *skel)
{
	cout << "  Exporting: " << skel->name << endl;

	auto node = FbxNode::Create(scene, skel->name);
	node->LclRotation.Set(FbxDouble3(-90, 0, 0));
	node->LclScaling.Set(FbxDouble3(1, 1, 1));

	auto null_attr = FbxNull::Create(scene, skel->name);
	node->SetNodeAttribute(null_attr);

	scene->GetRootNode()->AddChild(node);

	export_bones(scene, node, skel, -1);
}

static void export_skeletons(FbxScene *scene, GR2_file_info *info)
{
	for (int i = 0; i < info->skeletons_count; ++i) {
		export_skeleton(scene, info->skeletons[i]);
	}
}

void export_gr2(const char *filename, FbxScene *scene)
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

	export_skeletons(scene, gr2.file_info);
}