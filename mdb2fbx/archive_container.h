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

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "miniz.h"

class Archive_container {
public:
	struct Find_result {
		unsigned matches;
		unsigned archive_index;
		unsigned file_index;
	};

	bool add_archive(const char* filename);
	unsigned archive_count() const;
	bool extract_file(unsigned archive_index, unsigned file_index,
	                  const char* dest_filename) const;
	std::string filename(unsigned archive_index, unsigned file_index) const;
	Find_result find_file(const char* str) const;

private:
	struct Archive_entry {
		std::string filename;
		std::unique_ptr<mz_zip_archive> zip;
	};

	std::vector<Archive_entry> archives;

	std::pair<unsigned, unsigned> find_file(const Archive_entry& entry,
	                                        const char* str) const;
};
