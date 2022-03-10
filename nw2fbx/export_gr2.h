#pragma once

struct Dependency;
struct Export_info;
class GR2_file;

#include <vector>

#include "fbxsdk.h"

void export_animations(GR2_file& gr2, FbxScene* scene);
void export_skeletons(GR2_file& gr2, FbxScene* scene,
	std::vector<FbxNode*>& fbx_bones);
void export_gr2(const char *filename, FbxScene *scene,
	std::vector<FbxNode*> &fbx_bones);
void export_gr2(GR2_file& gr2, FbxScene *scene,
	std::vector<FbxNode*> &fbx_bones);
Dependency& export_gr2(Export_info& export_info, const char* filename);
void print_gr2_info(GR2_file& gr2);
