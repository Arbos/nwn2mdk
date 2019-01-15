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

#include <experimental/filesystem>
#include <fstream>
#include <iostream>

#include "config.h"
#include "yaml-cpp/yaml.h"

using namespace std;
using namespace std::experimental::filesystem::v1;

static const char* nwn2_dirs[] = {
    "C:\\Program Files\\Atari\\Neverwinter Nights 2",
    "C:\\Program Files (x86)\\Atari\\Neverwinter Nights 2",
    "C:\\GOG Games\\Neverwinter Nights 2 Complete"};

static void create_config_file(const char *filename)
{
	ofstream out(filename);
	out << "# (Optional) Directory where NWN2 is installed.\n";
	out << "# nwn2_home: C:\\Program Files\\Atari\\Neverwinter Nights 2\n";
}

static void find_nwn2_home(Config& config, YAML::Node& config_file)
{
	if (config_file["nwn2_home"]) {
		auto nwn2_home = config_file["nwn2_home"].as<string>("");
		if (exists(nwn2_home))
			config.nwn2_home = nwn2_home;
		else
			cout << "ERROR: The NWN2 installation directory specified in config.yml doesn't exist: \"" << nwn2_home << "\"\n";
	}
	else {
		for (unsigned i = 0; i < sizeof(nwn2_dirs) / sizeof(char*); ++i)
			if (exists(nwn2_dirs[i]))
				config.nwn2_home = nwn2_dirs[i];			
	}
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

	if (nwn2_home.empty()) {
		cout << "Cannot find a NWN2 installation directory. Edit the "
			"config.yml file and put the directory where NWN2 is "
			"installed.\n";
	}
}