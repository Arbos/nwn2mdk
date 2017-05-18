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

Config::Config()
{
	if(!exists("config.yml")) {
		ofstream out("config.yml");
		out << "# (Optional) Directory where NWN2 is installed.\n";
		out << "# nwn2_home: C:\\Program Files\\Atari\\Neverwinter Nights 2\n";
	}

	auto config = YAML::LoadFile("config.yml");
	if(!config) {
		cout << "cannot open config.yml\n";
		return;
	}

	if (config["nwn2_home"]) {
		nwn2_home = config["nwn2_home"].as<string>("");
	}
	else {
		for(unsigned i = 0; i < sizeof(nwn2_dirs)/sizeof(char*); ++i) {
			if(exists(nwn2_dirs[i])) {
				nwn2_home = nwn2_dirs[i];
			}
		}
	}
}
