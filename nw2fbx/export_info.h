#pragma once

#include <map>
#include <string>
#include <vector>

#include "archive_container.h"
#include "fbxsdk.h"

class Config;
struct Export_info;
class MDB_file;

struct Dependency {
	bool extracted;
	std::string extracted_path;
	bool exported;
	std::vector<FbxNode*> fbx_bones;
	std::vector<FbxNode*> fbx_body_bones;
	std::vector<FbxNode*> fbx_face_bones;
};

struct Export_info {
	const Config &config;
	std::vector<std::string> input_strings;
	std::string output_path;
	Archive_container materials;
	const MDB_file *mdb;
	FbxScene *scene;
	std::map<std::string, Dependency> dependencies;
};

void process_fbx_bones(Dependency& dep);
Archive_container& model_archives(const Config& config);
std::vector<unsigned char> load_resource(const Config& config,
                                         const char* filename);
