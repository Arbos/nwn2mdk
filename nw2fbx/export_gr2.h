#pragma once

struct Export_info;
class GR2_file;

void export_animations(GR2_file& gr2, Export_info& export_info);
void export_skeletons(GR2_file& gr2, Export_info& export_info);
void export_gr2(Export_info& export_info, const char* filename);
void print_gr2_info(GR2_file& gr2);
