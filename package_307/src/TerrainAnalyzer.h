#pragma once

#include <iostream>
#include <fstream>
#include <vector>
#include <gdal_priv.h>

struct TerrainPoint {
	double x;
	double y;
	double elevation;
};
typedef std::vector<TerrainPoint> TerrainDataVector;

class TerrainAnalyzer
{
public:
	TerrainDataVector* analyze_cross_section();
	TerrainDataVector* generate_terrain_data_vector();
};

