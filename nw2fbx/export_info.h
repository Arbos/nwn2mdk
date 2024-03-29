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
	/// Command line arguments that are not options.
	std::vector<std::string> input_strings;
	std::string output_path;
	Archive_container materials;
	const MDB_file *mdb;
	std::string mdb_skeleton_name;
	FbxScene *scene;
	std::map<std::string, Dependency> dependencies;

	Dependency* find_skeleton_dependency(const char* skeleton_name);
};

void process_fbx_bones(Dependency& dep);
Archive_container& model_archives(const Config& config);
std::vector<unsigned char> load_resource(const Config& config,
                                         const char* filename);
