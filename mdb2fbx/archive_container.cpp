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

#include <iostream>

#include "archive_container.h"

using namespace std;

static std::string to_upper(std::string s)
{
	for (auto& c : s)
		c = toupper(c);

	return s;
}

bool Archive_container::add_archive(const char* filename)
{
	Archive_entry e;
	e.filename = filename;
	e.zip.reset(new mz_zip_archive);
	mz_zip_zero_struct(e.zip.get());
	auto status =
	    mz_zip_reader_init_file(e.zip.get(), e.filename.c_str(), 0);
	if (!status)
		return false;

	archives.push_back(std::move(e));

	return true;
}

unsigned Archive_container::archive_count() const
{
	return archives.size();
}

bool Archive_container::extract_file(unsigned archive_index,
                                     unsigned file_index,
                                     const char* dest_filename) const
{
	if (archive_index >= archives.size())
		return false;

	if (!mz_zip_reader_extract_to_file(archives[archive_index].zip.get(),
	                                   file_index, dest_filename, 0)) {
		return false;
	}

	return true;
}

std::string Archive_container::filename(unsigned archive_index,
                                        unsigned file_index) const
{
	if (archive_index >= archives.size())
		return "";

	mz_zip_archive_file_stat file_stat;
	if (!mz_zip_reader_file_stat(archives[archive_index].zip.get(),
	                             file_index, &file_stat)) {
		return "";
	}

	return file_stat.m_filename;
}

Archive_container::Find_result
Archive_container::find_file(const char* str) const
{
	cout << "Searching: \"" << str << "\"\n";

	Find_result res;
	res.matches = 0;
	res.archive_index = archives.size();
	res.file_index = -1;
	string str_uppercase = to_upper(str);

	for (unsigned i = 0; i < archives.size(); ++i) {
		auto& entry = archives[i];
		cout << "  - " << entry.filename << endl;
		auto r = find_file(entry, str_uppercase.c_str());

		if (r.first > 0) {
			res.matches += r.first;
			if (res.archive_index >= archives.size()) {
				res.archive_index = i;
				res.file_index = r.second;
			}

			cout << "    # " << r.first << " matches\n";
		}
	}

	cout << "  # " << res.matches << " total matches\n";

	return res;
}

std::pair<unsigned, unsigned>
Archive_container::find_file(const Archive_entry& entry, const char* str) const
{
	unsigned file_index = -1;
	unsigned match_count = 0;
	for (unsigned i = 0; i < mz_zip_reader_get_num_files(entry.zip.get());
	     ++i) {
		mz_zip_archive_file_stat file_stat;
		if (!mz_zip_reader_file_stat(entry.zip.get(), i, &file_stat)) {
			cout << "Cannot get file stat\n";
			return {match_count, file_index};
		}

		string f = to_upper(string(file_stat.m_filename));

		if (f.find(str) != string::npos) {
			file_index = i;
			++match_count;
			cout << "    " << file_stat.m_filename << endl;
		}
	}

	return {match_count, file_index};
}
