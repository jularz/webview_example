
// interface class

result_t generate_viewshed_layer(double lon, double lat, double radius, double elevation){

    // 1. check dem data()
    // initialize check
    // if not -> failed return

    // 2. generate viewshed data
    // using gdal
    // GDALDriver : MEM
    // output : GDALDataset
    GDALDataset viewhsed_dataset = gdal_viewshed_generate(lon, lat, radius, elevation);

    // 3. convert GDALDataset to 2-div array

}

typedef std::vector<TerrainPoint> TerrainDataVector;

struct TerrainPoint{
    double x;
    double y;
    double elevation;
};



Class SpatialDataAnalyzer {

private:

    


public:
    
    GDALDataSet analyze_viewshed_data();
    a();
    
    

}

class SpartialDataUtility{
    BitBlt dataset_to_bitmap();
    SVG dataset_to_svg();
    SVG bitBlt_to_svg();
}


class ViewshedProcessor {
public:
    GDALDataSet analyze(double center_coordinates[2], double radius, double elevation, DEMModel &dem_model) {
        // Viewshed 분석 코드
    }
};

class TerrainProcessor {
public:
    TerrainDataVector analyze(double start_point[2], double end_point[2], DEMModel &dem_model) {
        // 단면도 분석 코드
    }
};

class SpatialDataAnalyzer {
private:
    ViewshedProcessor viewshed_processor;
    TerrainProcessor terrain_processor;
    SpartialDataUtility spartial_data_utility;
    

    GDALDataSetH dem_data;
    GDALDriver memory_driver;

public:

    void init();
    result_t initialize_dem();
    result_t transfer_bitBlt_to_ol();

    bool check_dem_data() {
        // dem data가 initlaized 되어 있는지 확인
        return true;
    }

    void analyze_viewshed(double center_coordinates[2], double radius, double elevation, DEMModel &dem_model) {
        
        // 0. dem 데이터 확인
        bool result = check_dem_data();

        // if result == true;
        // 1. viewshed 분석 수행
        GDALDataSet processed_dataset = viewshed_processor.analyze(center_coordinates, radius, elevation, dem_model);
        
        // 2. gdal dataset to svg 변환
        SVG svg_result = spartial_data_utility.dataset_to_svg(processed_dataset);

        // 3. openlayers 영역으로 svg 전달
        // execute javascript

    }

    TerrainDataVector analyze_cross_section(double start_point[2], double end_point[2], double interval DEMModel &dem_model) {
        
        // 0. dem 데이터 확인
        bool result = check_dem_data();

        // if result == true;
        // 1. terrain 분석 실행
        TerrainDataVector analyzed_result = terrain_processor.analyze(start_point, end_point, dem_model);
        
        // 2. 사용자에게 정보 반환
        return analyzed_result;
    }
};