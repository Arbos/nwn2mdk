#pragma once

class GR2_file;

#include <vector>

#include "fbxsdk.h"

void export_gr2(const char *filename, FbxScene *scene,
	std::vector<FbxNode*> &fbx_bones);
void export_gr2(GR2_file& gr2, FbxScene *scene,
	std::vector<FbxNode*> &fbx_bones);