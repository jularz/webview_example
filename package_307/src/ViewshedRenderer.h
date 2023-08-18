#pragma once


#ifdef max
#undef max
#undef min
#endif

#include <iostream>
#include <array>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <float.h>

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_version.h"
#include "gdal.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "ogr_api.h"
#include "ogr_srs_api.h"
#include "ogr_spatialref.h"

struct viewshedMemoryType {
    unsigned char* data;
    unsigned int size;
    int nXSize;
    int nYSize;
    double xTopLeft;
    double yTopLeft;
    double xBottomRight;
    double yBottomRight;

};

class ViewshedRenderer
{
public :
    ViewshedRenderer();
    ~ViewshedRenderer();

public:
    void DrawCircle(CDC* pDC);
    viewshedMemoryType* ViewshedGenerate(
        GDALRasterBandH hBand, const char* pszDriverName,
        const char* pszTargetRasterName, CSLConstList papszCreationOptions,
        double dfObserverX, double dfObserverY, double dfObserverHeight,
        double dfTargetHeight, double dfVisibleVal, double dfInvisibleVal,
        double dfOutOfRangeVal, double dfNoDataVal, double dfCurvCoeff,
        GDALViewshedMode eMode, double dfMaxDistance, GDALProgressFunc pfnProgress,
        void* pProgressArg, GDALViewshedOutputType heightMode,
        CSLConstList papszExtraOptions);
    viewshedMemoryType* GenerateViewshedRasterBuffer(CDC* pDC);

    viewshedMemoryType* viewshed_generate_via_vector(
        GDALRasterBandH hBand, const char* pszDriverName,
        const char* pszTargetRasterName, CSLConstList papszCreationOptions,
        double dfObserverX, double dfObserverY, double dfObserverHeight,
        double dfTargetHeight, double dfVisibleVal, double dfInvisibleVal,
        double dfOutOfRangeVal, double dfNoDataVal, double dfCurvCoeff,
        GDALViewshedMode eMode, double dfMaxDistance, GDALProgressFunc pfnProgress,
        void* pProgressArg, GDALViewshedOutputType heightMode,
        CSLConstList papszExtraOptions);
    char* generate_viewshed_vector_buffer(CDC* pDC);
};

