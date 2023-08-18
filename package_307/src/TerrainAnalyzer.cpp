#include "pch.h"
#include "TerrainAnalyzer.h"




// Bilinear interpolation
double interpolate_elevation(double* scanline, int x_size, int y_size, double x, double y) {
    int x1 = static_cast<int>(x);
    int y1 = static_cast<int>(y);
    int x2 = x1 + 1;
    int y2 = y1 + 1;

    // Ensure the points are within the bounds
    if (x1 < 0 || x2 >= x_size || y1 < 0 || y2 >= y_size) {
        return -9999;  // or any other nodata value
    }

    double q11 = scanline[y1 * x_size + x1];
    double q21 = scanline[y1 * x_size + x2];
    double q12 = scanline[y2 * x_size + x1];
    double q22 = scanline[y2 * x_size + x2];

    double elevation = (1.0 / ((x2 - x1) * (y2 - y1))) *
        (q11 * (x2 - x) * (y2 - y) +
            q21 * (x - x1) * (y2 - y) +
            q12 * (x2 - x) * (y - y1) +
            q22 * (x - x1) * (y - y1));

    return elevation;
}

TerrainDataVector* TerrainAnalyzer::generate_terrain_data_vector() {
    TerrainDataVector data;

    GDALAllRegister();

    const char* src_filename = "37709.img";
    double src_x = 14139344;
    double src_y = 4488531;
    double dst_x = 14149507;
    double dst_y = 4487428;
    double interval = 5;

    GDALDataset* po_dataset = static_cast<GDALDataset*>(GDALOpen(src_filename, GA_ReadOnly));
    if (!po_dataset) {
        std::cerr << "Failed to open dataset" << std::endl;
        return nullptr;
    }

    GDALRasterBand* po_band = po_dataset->GetRasterBand(1);
    int x_size = po_band->GetXSize();
    int y_size = po_band->GetYSize();

    double* scanline = static_cast<double*>(CPLMalloc(sizeof(double) * x_size * y_size));
    if (po_band->RasterIO(GF_Read, 0, 0, x_size, y_size, scanline, x_size, y_size, GDT_Float64, 0, 0) != CE_None) {
        std::cerr << "Failed to read raster data" << std::endl;
        CPLFree(scanline);
        GDALClose(po_dataset);
        return nullptr;
    }

    // Coordinate transformation setup
    OGRSpatialReference o_srs_3857, o_srs_dem;
    o_srs_3857.importFromEPSG(3857);
    o_srs_dem.importFromWkt(po_dataset->GetProjectionRef());
    OGRCoordinateTransformation* transformer_3857_dem = OGRCreateCoordinateTransformation(&o_srs_3857, &o_srs_dem);

    transformer_3857_dem->Transform(1, &src_x, &src_y);
    transformer_3857_dem->Transform(1, &dst_x, &dst_y);
    OGRCoordinateTransformation::DestroyCT(transformer_3857_dem);

    double geo_transform[6];
    po_dataset->GetGeoTransform(geo_transform);

    double origin_x = geo_transform[0];
    double pixel_width = geo_transform[1];
    double origin_y = geo_transform[3];
    double pixel_height = geo_transform[5];

    double distance = sqrt((dst_x - src_x) * (dst_x - src_x) + (dst_y - src_y) * (dst_y - src_y));
    int num_points = static_cast<int>(distance / interval);

    OGRCoordinateTransformation* transformer_dem_3857 = OGRCreateCoordinateTransformation(&o_srs_dem, &o_srs_3857);

    for (int i = 0; i <= num_points; ++i) {
        double x = src_x + (dst_x - src_x) * i / num_points;
        double y = src_y + (dst_y - src_y) * i / num_points;

        double pixel_x = (x - origin_x) / pixel_width;
        double pixel_y = (y - origin_y) / pixel_height;

        double elevation = interpolate_elevation(scanline, x_size, y_size, pixel_x, pixel_y);

        double x_3857 = x;
        double y_3857 = y;
        transformer_dem_3857->Transform(1, &x_3857, &y_3857);

        data.push_back({ x_3857, y_3857, elevation });
    }

    OGRCoordinateTransformation::DestroyCT(transformer_dem_3857);
    CPLFree(scanline);
    GDALClose(po_dataset);

    return &data;
}