#pragma once

class Config;
class MDB_file;

bool export_mdb(const MDB_file& mdb, const char* filename, const Config& config);