#pragma once

struct Export_info;
class MDB_file;

bool export_mdb(Export_info& export_info, const MDB_file& mdb);
