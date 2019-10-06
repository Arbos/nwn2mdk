// Copyright 2017 Jose M. Arbos
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

#include "config.h"
#include "yaml-cpp/yaml.h"

using namespace std;
using namespace std::filesystem;

static void create_config_file(const char *filename)
{
	ofstream out(filename);
	out << "# (Optional) Directory where NWN2 is installed.\n";
	out << "# nwn2_home: C:\\Program Files\\Atari\\Neverwinter Nights 2\n";
}

static bool find_nwn2_home_in_config(Config& config, YAML::Node& config_file)
{
	if (config_file["nwn2_home"]) {
		auto nwn2_home = config_file["nwn2_home"].as<string>("");

		if (exists(nwn2_home))
			config.nwn2_home = nwn2_home;
		else
			cout << "ERROR: The NWN2 installation directory specified in config.yml doesn't exist: \"" << nwn2_home << "\"\n";

		return true;
	}

	return false;
}

static bool find_nwn2_home_in_list(Config& config)
{
	static const char* nwn2_dirs[] = {
		"C:\\Program Files\\Atari\\Neverwinter Nights 2",
		"C:\\Program Files (x86)\\Atari\\Neverwinter Nights 2",
		"C:\\GOG Games\\Neverwinter Nights 2 Complete" };

	for (size_t i = 0; i < size(nwn2_dirs); ++i) {
		if (exists(nwn2_dirs[i])) {
			config.nwn2_home = nwn2_dirs[i];
			return true;
		}
	}

	return false;
}

static bool find_nwn2_home_in_registry(Config& config)
{
#ifdef _WIN32
	HKEY key;

	LSTATUS status = RegOpenKeyExA(HKEY_LOCAL_MACHINE,
		"SOFTWARE\\Obsidian\\NWN 2\\Neverwinter",
		REG_OPTION_RESERVED,
#ifdef _WIN64
		KEY_QUERY_VALUE | KEY_WOW64_32KEY,
#else
		KEY_QUERY_VALUE,
#endif
		&key);

	if (status != NO_ERROR)
		return false;

	char path[MAX_PATH + 1];
	DWORD path_size = MAX_PATH;

	static const char* names[] = {
		"Location", // Steam & GOG NWN2
		"Path" // Retail NWN2
	};

	for (size_t i = 0; i < size(names); ++i) {
		status = RegQueryValueExA(
			key, names[i], NULL, NULL,
			reinterpret_cast<LPBYTE>(path), &path_size);

		if (status == NO_ERROR) {
			// A registry value may not have been stored with the proper
			// terminating null character. Ensure the path is null-terminated.
			if (path_size == 0 || path[path_size - 1] != '\0')
				path[path_size] = '\0';

			if (exists(path)) {
				RegCloseKey(key);
				config.nwn2_home = path;
				return true;
			}
		}
	}

	RegCloseKey(key);
#endif

	return false;
}

static void find_nwn2_home(Config& config, YAML::Node& config_file)
{
	if (find_nwn2_home_in_config(config, config_file))
		return;
	else if (find_nwn2_home_in_registry(config))
		return;
	else if (find_nwn2_home_in_list(config))
		return;

	cout << "ERROR: Cannot find a NWN2 installation directory. Edit the "
		"config.yml file and put the directory where NWN2 is "
		"installed.\n";
}

Config::Config(const char *filename)
{
	if (!exists(filename))
		create_config_file(filename);

	try {
		auto config_file = YAML::LoadFile(filename);
		if (!config_file) {
			cout << "ERROR: Cannot open " << filename << endl;
			return;
		}

		find_nwn2_home(*this, config_file);
	}
	catch (...) {
		cout << "ERROR: Cannot open " << filename << ": It's ill-formed." << endl;
		return;
	}
}