#pragma once

#include <vector>

#include "fbxsdk.h"

void export_gr2(const char *filename, FbxScene *scene,
	std::vector<FbxNode*> &fbx_bones);