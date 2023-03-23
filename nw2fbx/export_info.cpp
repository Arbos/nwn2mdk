#include <fstream>
#include <filesystem>
#include <iostream>

#include "config.h"
#include "export_info.h"

using namespace std;
namespace fs = std::filesystem;

Dependency* Export_info::find_skeleton_dependency(const char* skeleton_name)
{
	for (auto& dep : dependencies) {
		if (dep.second.exported && dep.second.fbx_bones.size() > 0 &&
		    strcmpi(skeleton_name,
		            dep.second.fbx_bones[0]->GetName()) == 0) {
			return &dep.second;
		}
	}

	return nullptr;
}

void process_fbx_bones(Dependency& dep)
{
	FbxNode* ribcage = nullptr;

	for (auto it = dep.fbx_bones.begin(); it != dep.fbx_bones.end();) {
		if (strncmp((*it)->GetName(), "ap_", 3) == 0) {
			// "ap_..." (attachment point) bones are not used for skinning.
		}
		else if (strncmp((*it)->GetName(), "f_", 2) == 0) {
			// "f_..." (face) bones are only used for head skinning.
			dep.fbx_face_bones.push_back(*it);
		}
		else if (strcmp((*it)->GetName(), "Ribcage") == 0) {
			// Keep "Ribcage" bone to reinsert it later.
			ribcage = *it;
		}
		else {
			// The remaining bones are used for body skinning.
			dep.fbx_body_bones.push_back(*it);
		}
			++it;
	}

	if (ribcage) {
		// Reinsert "Ribcage" bone. For some unknown reason, it seems
		// this bone must be always the last one.
		dep.fbx_body_bones.push_back(ribcage);
	}
}

static Archive_container load_model_archives(const Config& config)
{
	const char* files[] = {
		"Data/NWN2_Models_X2_v121.zip", "Data/NWN2_Models_X2.zip",
		"Data/NWN2_Models_X1_v121.zip", "Data/NWN2_Models_X1.zip",
		"Data/NWN2_Models_v121.zip",    "Data/NWN2_Models_v112.zip",
		"Data/NWN2_Models_v107.zip",    "Data/NWN2_Models_v106.zip",
		"Data/NWN2_Models_v105.zip",    "Data/NWN2_Models_v104.zip",
		"Data/NWN2_Models_v103x1.zip",  "Data/NWN2_Models.zip",
		"Data/lod-merged_X2_v121.zip", "Data/lod-merged_X2.zip",
		"Data/lod-merged_X1_v121.zip", "Data/lod-merged_X1.zip",
		"Data/lod-merged_v121.zip",    "Data/lod-merged_v107.zip",
		"Data/lod-merged_v101.zip",    "Data/lod-merged.zip" };

	Archive_container archives;
	for (unsigned i = 0; i < sizeof(files) / sizeof(char*); ++i) {
		cout << "Indexing: " << files[i];
		fs::path p = fs::path(config.nwn2_home) / fs::path(files[i]);
		if (!archives.add_archive(p.string().c_str())) {
			cout << " : Cannot open zip";
		}
		cout << endl;
	}

	return archives;
}

Archive_container& model_archives(const Config& config)
{
	static auto a = load_model_archives(config);
	return a;
}

static std::vector<unsigned char> read_file(const char* filename)
{
	vector<unsigned char> buffer;

	ifstream in(filename, ios::in | ios::binary);

	if (!in)
		return buffer;

	// Get file size
	in.seekg(0, ios::end);
	auto size = in.tellg();
	in.seekg(0, ios::beg);

	buffer.resize(size);
	in.read((char *)buffer.data(), size);

	return buffer;
}

std::vector<unsigned char> load_resource(const Config& config,
                                         const char* filename)
{
	if (fs::exists(filename))
		return read_file(filename);

	static auto& archives = model_archives(config);
	auto r = archives.find_file(filename);
	if (r.matches == 0)
		return {};

	cout << "Extracting: " << filename << endl;

	vector<unsigned char> buffer;

	if (!archives.extract_file_to_mem(r.archive_index, r.file_index,
	                                  buffer)) {
		cout << "  Cannot extract\n";
		return {};
	}

	return buffer;
}
