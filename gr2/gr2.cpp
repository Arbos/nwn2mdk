#include <filesystem>
#include <iostream>

#include "config.h"
#include "gr2_file.h"
#include "granny2dll_handle.h"

using namespace std;
namespace fs = std::filesystem;

static void print_usage()
{
	cout << "Usage: gr2 <command> <source file> <target file>\n";
	cout << "\n";
	cout << "Commands:\n";
	cout << "  compress\n";
	cout << "  decompress\n";
}

static int compress(int argc, char* argv[])
{
	if (argc < 4) {
		print_usage();
		return 1;
	}

	if (!fs::exists(argv[2])) {
		cout << "Cannot compress " << argv[2] << ": No such file\n";
		return 1;
	}

	if (fs::equivalent(argv[2], argv[3])) {
		cout << "Cannot compress " << argv[2] << ": Source and target are the same\n";
		return 1;
	}

	static Granny2dll_handle granny2dll(GR2_file::granny2dll_filename.c_str());

	if (!granny2dll) {
		cout << "Cannot compress " << argv[2] << ": Failed to load library granny2.dll\n";
		return 1;
	}

	if (!granny2dll.GrannyRecompressFile) {
		cout << "Cannot compress " << argv[2] << ": Invalid library granny2.dll\n";
		return 1;
	}

	int CompressorMapping[] = { 2, 2, 2 };
	int CompressorMappingCount = sizeof(CompressorMapping) / sizeof(CompressorMapping[0]);
	int r = granny2dll.GrannyRecompressFile(argv[2], argv[3], CompressorMappingCount, CompressorMapping);

	if (!r) {
		cout << "Cannot compress " << argv[2] << ". Possible causes:\n";
		cout << "  - Source is not a valid GR2 file.\n";
		cout << "  - Target already exists but it is not a file.\n";
		return 1;
	}

	return 0;
}

static int decompress(int argc, char* argv[])
{
	if (argc < 4) {
		print_usage();
		return 1;
	}

	if (!fs::exists(argv[2])) {
		cout << "Cannot decompress " << argv[2] << ": No such file\n";
		return 1;
	}

	if (fs::equivalent(argv[2], argv[3])) {
		cout << "Cannot decompress " << argv[2] << ": Source and target are the same\n";
		return 1;
	}

	static Granny2dll_handle granny2dll(GR2_file::granny2dll_filename.c_str());

	if (!granny2dll) {
		cout << "Cannot decompress " << argv[2] << ": Failed to load library granny2.dll\n";
		return 1;
	}

	if (!granny2dll.GrannyRecompressFile) {
		cout << "Cannot decompress " << argv[2] << ": Invalid library granny2.dll\n";
		return 1;
	}

	int CompressorMapping[] = { 0, 0, 0 };
	int CompressorMappingCount = sizeof(CompressorMapping) / sizeof(CompressorMapping[0]);
	int r = granny2dll.GrannyRecompressFile(argv[2], argv[3], CompressorMappingCount, CompressorMapping);

	if (!r) {
		cout << "Cannot decompress " << argv[2] << ": Possible causes:\n";
		cout << "  - Source is not a valid GR2 file.\n";
		cout << "  - Target already exists but it is not a file.\n";
		return 1;
	}

	return 0;
}

int main(int argc, char* argv[])
{
	Config config((fs::path(argv[0]).parent_path() / "config.yml").string().c_str());

	if (config.nwn2_home.empty())
		return 1;

	GR2_file::granny2dll_filename = config.nwn2_home + "\\granny2.dll";

	if (argc <= 1) {
		print_usage();
		return 1;
	}

	if (strcmp(argv[1], "compress") == 0) {
		return compress(argc, argv);
	}
	if (strcmp(argv[1], "decompress") == 0) {
		return decompress(argc, argv);
	}
	else {
		print_usage();
		return 1;
	}

	return 0;
}