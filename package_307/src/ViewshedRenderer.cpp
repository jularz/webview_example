#include "pch.h"
#include "ViewshedRenderer.h"

ViewshedRenderer::ViewshedRenderer() {

}
ViewshedRenderer::~ViewshedRenderer() {

}

// GDALDataset을 메모리에 저장하고 해당 메모리 주소 반환
char* GetDatasetMemoryAddress(GDALDataset* poDS) {
    return reinterpret_cast<char*>(poDS);
}

void GDALErrorHandler(CPLErr eErrClass, int err_no, const char* msg)
{
    printf("GDAL Error: %s\n", msg); // 오류 메시지를 출력합니다.
}


inline static void SetVisibility(int iPixel, double dfZ, double dfZTarget,
    double* padfZVal, std::vector<GByte>& vResult,
    GByte byVisibleVal, GByte byInvisibleVal)
{
    if (padfZVal[iPixel] + dfZTarget < dfZ)
        vResult[iPixel] = byInvisibleVal;
    else
        vResult[iPixel] = byVisibleVal;

    if (padfZVal[iPixel] < dfZ)
        padfZVal[iPixel] = dfZ;
}

inline static bool AdjustHeightInRange(const double* adfGeoTransform,
    int iPixel, int iLine, double& dfHeight,
    double dfDistance2, double dfCurvCoeff,
    double dfSphereDiameter)
{
    if (dfDistance2 <= 0 && dfCurvCoeff == 0)
        return true;

    double dfX = adfGeoTransform[1] * iPixel + adfGeoTransform[2] * iLine;
    double dfY = adfGeoTransform[4] * iPixel + adfGeoTransform[5] * iLine;
    double dfR2 = dfX * dfX + dfY * dfY;

    /* calc adjustment */
    if (dfCurvCoeff != 0 &&
        dfSphereDiameter != std::numeric_limits<double>::infinity())
        dfHeight -= dfCurvCoeff * dfR2 / dfSphereDiameter;

    if (dfDistance2 > 0 && dfR2 > dfDistance2)
        return false;

    return true;
}

inline static double CalcHeightLine(int i, double Za, double Zo)
{
    if (i == 1)
        return Za;
    else
        return (Za - Zo) / (i - 1) + Za;
}

inline static double CalcHeightDiagonal(int i, int j, double Za, double Zb,
    double Zo)
{
    return ((Za - Zo) * i + (Zb - Zo) * j) / (i + j - 1) + Zo;
}

inline static double CalcHeightEdge(int i, int j, double Za, double Zb,
    double Zo)
{
    if (i == j)
        return CalcHeightLine(i, Za, Zo);
    else
        return ((Za - Zo) * i + (Zb - Zo) * (j - i)) / (j - 1) + Zo;
}

inline static double CalcHeight(double dfZ, double dfZ2, GDALViewshedMode eMode)
{
    if (eMode == GVM_Edge)
        return dfZ2;
    else if (eMode == GVM_Max)
        return (std::max)(dfZ, dfZ2);
    else if (eMode == GVM_Min)
        return (std::min)(dfZ, dfZ2);
    else
        return dfZ;
}



void ViewshedRenderer::DrawCircle(CDC* pDC)
{
    CBrush brush(RGB(255, 0, 0)); // 빨간색 브러시
    CBrush* pOldBrush = pDC->SelectObject(&brush);
    pDC->Ellipse(50, 50, 150, 150); // 좌표를 사용하여 원 그리기
    pDC->SelectObject(pOldBrush);
}



viewshedMemoryType* ViewshedRenderer::ViewshedGenerate(
    GDALRasterBandH hBand, const char* pszDriverName,
    const char* pszTargetRasterName, CSLConstList papszCreationOptions,
    double dfObserverX, double dfObserverY, double dfObserverHeight,
    double dfTargetHeight, double dfVisibleVal, double dfInvisibleVal,
    double dfOutOfRangeVal, double dfNoDataVal, double dfCurvCoeff,
    GDALViewshedMode eMode, double dfMaxDistance, GDALProgressFunc pfnProgress,
    void* pProgressArg, GDALViewshedOutputType heightMode,
    CSLConstList papszExtraOptions)

{
    VALIDATE_POINTER1(hBand, "GDALViewshedGenerate", nullptr);
    VALIDATE_POINTER1(pszTargetRasterName, "GDALViewshedGenerate", nullptr);

    CPL_IGNORE_RET_VAL(papszExtraOptions);

    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    if (!pfnProgress(0.0, "", pProgressArg))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return nullptr;
    }

    const GByte byNoDataVal = dfNoDataVal >= 0 && dfNoDataVal <= 255
        ? static_cast<GByte>(dfNoDataVal)
        : 0;
    const GByte byVisibleVal = dfVisibleVal >= 0 && dfVisibleVal <= 255
        ? static_cast<GByte>(dfVisibleVal)
        : 255;
    const GByte byInvisibleVal = dfInvisibleVal >= 0 && dfInvisibleVal <= 255
        ? static_cast<GByte>(dfInvisibleVal)
        : 0;
    const GByte byOutOfRangeVal = dfOutOfRangeVal >= 0 && dfOutOfRangeVal <= 255
        ? static_cast<GByte>(dfOutOfRangeVal)
        : 0;

    if (heightMode != GVOT_MIN_TARGET_HEIGHT_FROM_DEM &&
        heightMode != GVOT_MIN_TARGET_HEIGHT_FROM_GROUND)
        heightMode = GVOT_NORMAL;

    /* set up geotransformation */
    std::array<double, 6> adfGeoTransform{{0.0, 1.0, 0.0, 0.0, 0.0, 1.0}};
    GDALDatasetH hSrcDS = GDALGetBandDataset(hBand);
    if (hSrcDS != nullptr)
        GDALGetGeoTransform(hSrcDS, adfGeoTransform.data());

    double adfInvGeoTransform[6];
    if (!GDALInvGeoTransform(adfGeoTransform.data(), adfInvGeoTransform))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
        return nullptr;
    }

    /* calculate observer position */
    double dfX, dfY;
    GDALApplyGeoTransform(adfInvGeoTransform, dfObserverX, dfObserverY, &dfX,
        &dfY);
    int nX = static_cast<int>(dfX);
    int nY = static_cast<int>(dfY);

    int nXSize = GDALGetRasterBandXSize(hBand);
    int nYSize = GDALGetRasterBandYSize(hBand);

    if (nX < 0 || nX > nXSize || nY < 0 || nY > nYSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The observer location falls outside of the DEM area");
        return nullptr;
    }

    /* calculate the area of interest */
    int nXStart =
        dfMaxDistance > 0
        ? (std::max)(0, static_cast<int>(std::floor(
            nX - adfInvGeoTransform[1] * dfMaxDistance)))
        : 0;
    int nXStop =
        dfMaxDistance > 0
        ? (std::min)(nXSize,
            static_cast<int>(std::ceil(nX + adfInvGeoTransform[1] *
                dfMaxDistance) +
                1))
        : nXSize;
    int nYStart =
        dfMaxDistance > 0
        ? (std::max)(0, static_cast<int>(std::floor(
            nY + adfInvGeoTransform[5] * dfMaxDistance)))
        : 0;
    int nYStop =
        dfMaxDistance > 0
        ? (std::min)(nYSize,
            static_cast<int>(std::ceil(nY - adfInvGeoTransform[5] *
                dfMaxDistance) +
                1))
        : nYSize;

    /* normalize horizontal index (0 - nXSize) */
    nXSize = nXStop - nXStart;
    nX -= nXStart;

    nYSize = nYStop - nYStart;

    if (nXSize == 0 || nYSize == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid target raster size");
        return nullptr;
    }

    std::vector<double> vFirstLineVal;
    std::vector<double> vLastLineVal;
    std::vector<double> vThisLineVal;
    std::vector<GByte> vResult;
    std::vector<double> vHeightResult;

    try
    {
        vFirstLineVal.resize(nXSize);
        vLastLineVal.resize(nXSize);
        vThisLineVal.resize(nXSize);
        vResult.resize(nXSize);

        if (heightMode != GVOT_NORMAL)
            vHeightResult.resize(nXSize);
    }
    catch (...)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Cannot allocate vectors for viewshed");
        return nullptr;
    }

    double* padfFirstLineVal = vFirstLineVal.data();
    double* padfLastLineVal = vLastLineVal.data();
    double* padfThisLineVal = vThisLineVal.data();
    GByte* pabyResult = vResult.data();
    double* dfHeightResult = vHeightResult.data();

    GDALDriverManager* hMgr = GetGDALDriverManager();
    GDALDriver* hDriver =
        hMgr->GetDriverByName(pszDriverName ? pszDriverName : "GTiff");
    if (!hDriver)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get driver");
        return nullptr;
    }

    /* create output raster */
    auto poDstDS = std::unique_ptr<GDALDataset>(
        hDriver->Create("", nXSize, nYStop - nYStart, 1, GDT_Byte,
            const_cast<char**>(papszCreationOptions)));
    if (!poDstDS)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create dataset for %s",
            pszTargetRasterName);
        return nullptr;
    }
    /* copy srs */
    if (hSrcDS)
        poDstDS->SetSpatialRef(
            GDALDataset::FromHandle(hSrcDS)->GetSpatialRef());

    std::array<double, 6> adfDstGeoTransform;
    adfDstGeoTransform[0] = adfGeoTransform[0] + adfGeoTransform[1] * nXStart +
        adfGeoTransform[2] * nYStart;
    adfDstGeoTransform[1] = adfGeoTransform[1];
    adfDstGeoTransform[2] = adfGeoTransform[2];
    adfDstGeoTransform[3] = adfGeoTransform[3] + adfGeoTransform[4] * nXStart +
        adfGeoTransform[5] * nYStart;
    adfDstGeoTransform[4] = adfGeoTransform[4];
    adfDstGeoTransform[5] = adfGeoTransform[5];
    poDstDS->SetGeoTransform(adfDstGeoTransform.data());

    auto hTargetBand = poDstDS->GetRasterBand(1);
    if (hTargetBand == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get band for %s",
            pszTargetRasterName);
        return nullptr;
    }

    if (dfNoDataVal >= 0)
        GDALSetRasterNoDataValue(
            hTargetBand, heightMode != GVOT_NORMAL ? dfNoDataVal : byNoDataVal);

    /* process first line */
    if (GDALRasterIO(hBand, GF_Read, nXStart, nY, nXSize, 1, padfFirstLineVal,
        nXSize, 1, GDT_Float64, 0, 0))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "RasterIO error when reading DEM at position(%d, %d), size(%d, %d)",
            nXStart, nY, nXSize, 1);
        return nullptr;
    }

    const double dfZObserver = dfObserverHeight + padfFirstLineVal[nX];
    double dfZ = 0.0;
    const double dfDistance2 = dfMaxDistance * dfMaxDistance;

    /* If we can't get a SemiMajor axis from the SRS, it will be
     * SRS_WGS84_SEMIMAJOR
     */
    double dfSphereDiameter(std::numeric_limits<double>::infinity());
    const OGRSpatialReference* poDstSRS = poDstDS->GetSpatialRef();
    if (poDstSRS)
    {
        OGRErr eSRSerr;
        double dfSemiMajor = poDstSRS->GetSemiMajor(&eSRSerr);

        /* If we fetched the axis from the SRS, use it */
        if (eSRSerr != OGRERR_FAILURE)
            dfSphereDiameter = dfSemiMajor * 2.0;
        else
            CPLDebug("GDALViewshedGenerate",
                "Unable to fetch SemiMajor axis from spatial reference");
    }

    /* mark the observer point as visible */
    double dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
        ? padfFirstLineVal[nX]
        : 0.0;
    pabyResult[nX] = byVisibleVal;
    if (heightMode != GVOT_NORMAL)
        dfHeightResult[nX] = dfGroundLevel;

    if (nX > 0)
    {
        dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
            ? padfFirstLineVal[nX - 1]
            : 0.0;
        CPL_IGNORE_RET_VAL(AdjustHeightInRange(
            adfGeoTransform.data(), 1, 0, padfFirstLineVal[nX - 1], dfDistance2,
            dfCurvCoeff, dfSphereDiameter));
        pabyResult[nX - 1] = byVisibleVal;
        if (heightMode != GVOT_NORMAL)
            dfHeightResult[nX - 1] = dfGroundLevel;
    }
    if (nX < nXSize - 1)
    {
        dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
            ? padfFirstLineVal[nX + 1]
            : 0.0;
        CPL_IGNORE_RET_VAL(AdjustHeightInRange(
            adfGeoTransform.data(), 1, 0, padfFirstLineVal[nX + 1], dfDistance2,
            dfCurvCoeff, dfSphereDiameter));
        pabyResult[nX + 1] = byVisibleVal;
        if (heightMode != GVOT_NORMAL)
            dfHeightResult[nX + 1] = dfGroundLevel;
    }

    /* process left direction */
    for (int iPixel = nX - 2; iPixel >= 0; iPixel--)
    {
        dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
            ? padfFirstLineVal[iPixel]
            : 0.0;
        bool adjusted = AdjustHeightInRange(
            adfGeoTransform.data(), nX - iPixel, 0, padfFirstLineVal[iPixel],
            dfDistance2, dfCurvCoeff, dfSphereDiameter);
        if (adjusted)
        {
            dfZ = CalcHeightLine(nX - iPixel, padfFirstLineVal[iPixel + 1],
                dfZObserver);

            if (heightMode != GVOT_NORMAL)
                dfHeightResult[iPixel] = std::max(
                    0.0, (dfZ - padfFirstLineVal[iPixel] + dfGroundLevel));

            SetVisibility(iPixel, dfZ, dfTargetHeight, padfFirstLineVal,
                vResult, byVisibleVal, byInvisibleVal);
        }
        else
        {
            for (; iPixel >= 0; iPixel--)
            {
                pabyResult[iPixel] = byOutOfRangeVal;
                if (heightMode != GVOT_NORMAL)
                    dfHeightResult[iPixel] = dfOutOfRangeVal;
            }
        }
    }
    /* process right direction */
    for (int iPixel = nX + 2; iPixel < nXSize; iPixel++)
    {
        dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
            ? padfFirstLineVal[iPixel]
            : 0.0;
        bool adjusted = AdjustHeightInRange(
            adfGeoTransform.data(), iPixel - nX, 0, padfFirstLineVal[iPixel],
            dfDistance2, dfCurvCoeff, dfSphereDiameter);
        if (adjusted)
        {
            dfZ = CalcHeightLine(iPixel - nX, padfFirstLineVal[iPixel - 1],
                dfZObserver);

            if (heightMode != GVOT_NORMAL)
                dfHeightResult[iPixel] = std::max(
                    0.0, (dfZ - padfFirstLineVal[iPixel] + dfGroundLevel));

            SetVisibility(iPixel, dfZ, dfTargetHeight, padfFirstLineVal,
                vResult, byVisibleVal, byInvisibleVal);
        }
        else
        {
            for (; iPixel < nXSize; iPixel++)
            {
                pabyResult[iPixel] = byOutOfRangeVal;
                if (heightMode != GVOT_NORMAL)
                    dfHeightResult[iPixel] = dfOutOfRangeVal;
            }
        }
    }
    /* write result line */

    if (GDALRasterIO(hTargetBand, GF_Write, 0, nY - nYStart, nXSize, 1,
        heightMode != GVOT_NORMAL
        ? static_cast<void*>(dfHeightResult)
        : static_cast<void*>(pabyResult),
        nXSize, 1,
        heightMode != GVOT_NORMAL ? GDT_Float64 : GDT_Byte, 0, 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "RasterIO error when writing target raster at position "
            "(%d,%d), size (%d,%d)",
            0, nY - nYStart, nXSize, 1);
        return nullptr;
    }

    /* scan upwards */
    std::copy(vFirstLineVal.begin(), vFirstLineVal.end(), vLastLineVal.begin());
    for (int iLine = nY - 1; iLine >= nYStart; iLine--)
    {
        if (GDALRasterIO(hBand, GF_Read, nXStart, iLine, nXSize, 1,
            padfThisLineVal, nXSize, 1, GDT_Float64, 0, 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "RasterIO error when reading DEM at position (%d,%d), "
                "size (%d,%d)",
                nXStart, iLine, nXSize, 1);
            return nullptr;
        }

        /* set up initial point on the scanline */
        dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
            ? padfThisLineVal[nX]
            : 0.0;
        bool adjusted = AdjustHeightInRange(
            adfGeoTransform.data(), 0, nY - iLine, padfThisLineVal[nX],
            dfDistance2, dfCurvCoeff, dfSphereDiameter);
        if (adjusted)
        {
            dfZ = CalcHeightLine(nY - iLine, padfLastLineVal[nX], dfZObserver);

            if (heightMode != GVOT_NORMAL)
                dfHeightResult[nX] =
                std::max(0.0, (dfZ - padfThisLineVal[nX] + dfGroundLevel));

            SetVisibility(nX, dfZ, dfTargetHeight, padfThisLineVal, vResult,
                byVisibleVal, byInvisibleVal);
        }
        else
        {
            pabyResult[nX] = byOutOfRangeVal;
            if (heightMode != GVOT_NORMAL)
                dfHeightResult[nX] = dfOutOfRangeVal;
        }

        /* process left direction */
        for (int iPixel = nX - 1; iPixel >= 0; iPixel--)
        {
            dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                ? padfThisLineVal[iPixel]
                : 0.0;
            bool left_adjusted =
                AdjustHeightInRange(adfGeoTransform.data(), nX - iPixel,
                    nY - iLine, padfThisLineVal[iPixel],
                    dfDistance2, dfCurvCoeff, dfSphereDiameter);
            if (left_adjusted)
            {
                if (eMode != GVM_Edge)
                    dfZ = CalcHeightDiagonal(
                        nX - iPixel, nY - iLine, padfThisLineVal[iPixel + 1],
                        padfLastLineVal[iPixel], dfZObserver);

                if (eMode != GVM_Diagonal)
                {
                    double dfZ2 =
                        nX - iPixel >= nY - iLine
                        ? CalcHeightEdge(nY - iLine, nX - iPixel,
                            padfLastLineVal[iPixel + 1],
                            padfThisLineVal[iPixel + 1],
                            dfZObserver)
                        : CalcHeightEdge(nX - iPixel, nY - iLine,
                            padfLastLineVal[iPixel + 1],
                            padfLastLineVal[iPixel],
                            dfZObserver);
                    dfZ = CalcHeight(dfZ, dfZ2, eMode);
                }

                if (heightMode != GVOT_NORMAL)
                    dfHeightResult[iPixel] = std::max(
                        0.0, (dfZ - padfThisLineVal[iPixel] + dfGroundLevel));

                SetVisibility(iPixel, dfZ, dfTargetHeight, padfThisLineVal,
                    vResult, byVisibleVal, byInvisibleVal);
            }
            else
            {
                for (; iPixel >= 0; iPixel--)
                {
                    pabyResult[iPixel] = byOutOfRangeVal;
                    if (heightMode != GVOT_NORMAL)
                        dfHeightResult[iPixel] = dfOutOfRangeVal;
                }
            }
        }
        /* process right direction */
        for (int iPixel = nX + 1; iPixel < nXSize; iPixel++)
        {
            dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                ? padfThisLineVal[iPixel]
                : 0.0;
            bool right_adjusted =
                AdjustHeightInRange(adfGeoTransform.data(), iPixel - nX,
                    nY - iLine, padfThisLineVal[iPixel],
                    dfDistance2, dfCurvCoeff, dfSphereDiameter);
            if (right_adjusted)
            {
                if (eMode != GVM_Edge)
                    dfZ = CalcHeightDiagonal(
                        iPixel - nX, nY - iLine, padfThisLineVal[iPixel - 1],
                        padfLastLineVal[iPixel], dfZObserver);

                if (eMode != GVM_Diagonal)
                {
                    double dfZ2 =
                        iPixel - nX >= nY - iLine
                        ? CalcHeightEdge(nY - iLine, iPixel - nX,
                            padfLastLineVal[iPixel - 1],
                            padfThisLineVal[iPixel - 1],
                            dfZObserver)
                        : CalcHeightEdge(iPixel - nX, nY - iLine,
                            padfLastLineVal[iPixel - 1],
                            padfLastLineVal[iPixel],
                            dfZObserver);
                    dfZ = CalcHeight(dfZ, dfZ2, eMode);
                }

                if (heightMode != GVOT_NORMAL)
                    dfHeightResult[iPixel] = std::max(
                        0.0, (dfZ - padfThisLineVal[iPixel] + dfGroundLevel));

                SetVisibility(iPixel, dfZ, dfTargetHeight, padfThisLineVal,
                    vResult, byVisibleVal, byInvisibleVal);
            }
            else
            {
                for (; iPixel < nXSize; iPixel++)
                {
                    pabyResult[iPixel] = byOutOfRangeVal;
                    if (heightMode != GVOT_NORMAL)
                        dfHeightResult[iPixel] = dfOutOfRangeVal;
                }
            }
        }

        /* write result line */
        if (GDALRasterIO(
            hTargetBand, GF_Write, 0, iLine - nYStart, nXSize, 1,
            heightMode != GVOT_NORMAL ? static_cast<void*>(dfHeightResult)
            : static_cast<void*>(pabyResult),
            nXSize, 1, heightMode != GVOT_NORMAL ? GDT_Float64 : GDT_Byte,
            0, 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "RasterIO error when writing target raster at position "
                "(%d,%d), size (%d,%d)",
                0, iLine - nYStart, nXSize, 1);
            return nullptr;
        }

        std::swap(padfLastLineVal, padfThisLineVal);

        if (!pfnProgress((nY - iLine) / static_cast<double>(nYSize), "",
            pProgressArg))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            return nullptr;
        }
    }
    /* scan downwards */
    memcpy(padfLastLineVal, padfFirstLineVal, nXSize * sizeof(double));
    for (int iLine = nY + 1; iLine < nYStop; iLine++)
    {
        if (GDALRasterIO(hBand, GF_Read, nXStart, iLine, nXStop - nXStart, 1,
            padfThisLineVal, nXStop - nXStart, 1, GDT_Float64, 0,
            0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "RasterIO error when reading DEM at position (%d,%d), "
                "size (%d,%d)",
                nXStart, iLine, nXStop - nXStart, 1);
            return nullptr;
        }

        /* set up initial point on the scanline */
        dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
            ? padfThisLineVal[nX]
            : 0.0;
        bool adjusted = AdjustHeightInRange(
            adfGeoTransform.data(), 0, iLine - nY, padfThisLineVal[nX],
            dfDistance2, dfCurvCoeff, dfSphereDiameter);
        if (adjusted)
        {
            dfZ = CalcHeightLine(iLine - nY, padfLastLineVal[nX], dfZObserver);

            if (heightMode != GVOT_NORMAL)
                dfHeightResult[nX] =
                std::max(0.0, (dfZ - padfThisLineVal[nX] + dfGroundLevel));

            SetVisibility(nX, dfZ, dfTargetHeight, padfThisLineVal, vResult,
                byVisibleVal, byInvisibleVal);
        }
        else
        {
            pabyResult[nX] = byOutOfRangeVal;
            if (heightMode != GVOT_NORMAL)
                dfHeightResult[nX] = dfOutOfRangeVal;
        }

        /* process left direction */
        for (int iPixel = nX - 1; iPixel >= 0; iPixel--)
        {
            dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                ? padfThisLineVal[iPixel]
                : 0.0;
            bool left_adjusted =
                AdjustHeightInRange(adfGeoTransform.data(), nX - iPixel,
                    iLine - nY, padfThisLineVal[iPixel],
                    dfDistance2, dfCurvCoeff, dfSphereDiameter);
            if (left_adjusted)
            {
                if (eMode != GVM_Edge)
                    dfZ = CalcHeightDiagonal(
                        nX - iPixel, iLine - nY, padfThisLineVal[iPixel + 1],
                        padfLastLineVal[iPixel], dfZObserver);

                if (eMode != GVM_Diagonal)
                {
                    double dfZ2 =
                        nX - iPixel >= iLine - nY
                        ? CalcHeightEdge(iLine - nY, nX - iPixel,
                            padfLastLineVal[iPixel + 1],
                            padfThisLineVal[iPixel + 1],
                            dfZObserver)
                        : CalcHeightEdge(nX - iPixel, iLine - nY,
                            padfLastLineVal[iPixel + 1],
                            padfLastLineVal[iPixel],
                            dfZObserver);
                    dfZ = CalcHeight(dfZ, dfZ2, eMode);
                }

                if (heightMode != GVOT_NORMAL)
                    dfHeightResult[iPixel] = std::max(
                        0.0, (dfZ - padfThisLineVal[iPixel] + dfGroundLevel));

                SetVisibility(iPixel, dfZ, dfTargetHeight, padfThisLineVal,
                    vResult, byVisibleVal, byInvisibleVal);
            }
            else
            {
                for (; iPixel >= 0; iPixel--)
                {
                    pabyResult[iPixel] = byOutOfRangeVal;
                    if (heightMode != GVOT_NORMAL)
                        dfHeightResult[iPixel] = dfOutOfRangeVal;
                }
            }
        }
        /* process right direction */
        for (int iPixel = nX + 1; iPixel < nXSize; iPixel++)
        {
            dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                ? padfThisLineVal[iPixel]
                : 0.0;
            bool right_adjusted =
                AdjustHeightInRange(adfGeoTransform.data(), iPixel - nX,
                    iLine - nY, padfThisLineVal[iPixel],
                    dfDistance2, dfCurvCoeff, dfSphereDiameter);
            if (right_adjusted)
            {
                if (eMode != GVM_Edge)
                    dfZ = CalcHeightDiagonal(
                        iPixel - nX, iLine - nY, padfThisLineVal[iPixel - 1],
                        padfLastLineVal[iPixel], dfZObserver);

                if (eMode != GVM_Diagonal)
                {
                    double dfZ2 =
                        iPixel - nX >= iLine - nY
                        ? CalcHeightEdge(iLine - nY, iPixel - nX,
                            padfLastLineVal[iPixel - 1],
                            padfThisLineVal[iPixel - 1],
                            dfZObserver)
                        : CalcHeightEdge(iPixel - nX, iLine - nY,
                            padfLastLineVal[iPixel - 1],
                            padfLastLineVal[iPixel],
                            dfZObserver);
                    dfZ = CalcHeight(dfZ, dfZ2, eMode);
                }

                if (heightMode != GVOT_NORMAL)
                    dfHeightResult[iPixel] = std::max(
                        0.0, (dfZ - padfThisLineVal[iPixel] + dfGroundLevel));

                SetVisibility(iPixel, dfZ, dfTargetHeight, padfThisLineVal,
                    vResult, byVisibleVal, byInvisibleVal);
            }
            else
            {
                for (; iPixel < nXSize; iPixel++)
                {
                    pabyResult[iPixel] = byOutOfRangeVal;
                    if (heightMode != GVOT_NORMAL)
                        dfHeightResult[iPixel] = dfOutOfRangeVal;
                }
            }
        }

        /* write result line */
        if (GDALRasterIO(
            hTargetBand, GF_Write, 0, iLine - nYStart, nXSize, 1,
            heightMode != GVOT_NORMAL ? static_cast<void*>(dfHeightResult)
            : static_cast<void*>(pabyResult),
            nXSize, 1, heightMode != GVOT_NORMAL ? GDT_Float64 : GDT_Byte,
            0, 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "RasterIO error when writing target raster at position "
                "(%d,%d), size (%d,%d)",
                0, iLine - nYStart, nXSize, 1);
            return nullptr;
        }

        std::swap(padfLastLineVal, padfThisLineVal);

        if (!pfnProgress((iLine - nYStart) / static_cast<double>(nYSize), "",
            pProgressArg))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            return nullptr;
        }
    }

    if (!pfnProgress(1.0, "", pProgressArg))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return nullptr;
    }
    
    
    
    
    unsigned char* pafScanline = (unsigned char*)CPLMalloc(sizeof(unsigned char) * nXSize * nYSize);
    hTargetBand->RasterIO(GF_Read, 0, 0, nXSize, nYSize, pafScanline, nXSize, nYSize, GDT_Byte, 0, 0);

    double geoTransformResult[6];
    poDstDS->GetGeoTransform(geoTransformResult);
    const char* pszProjection = poDstDS->GetProjectionRef();

    OGRSpatialReference oSourceSRS;
    oSourceSRS.importFromWkt(pszProjection);

    OGRSpatialReference oTargetSRS;
    oTargetSRS.importFromEPSG(3857);

    OGRCoordinateTransformation* poCT = OGRCreateCoordinateTransformation(&oSourceSRS, &oTargetSRS);

    double xTopLeft = geoTransformResult[0];
    double yTopLeft = geoTransformResult[3];
    double xBottomRight = geoTransformResult[0] + (nXSize - 1) * geoTransformResult[1] + (nYSize - 1) * geoTransformResult[2];
    double yBottomRight = geoTransformResult[3] + (nXSize - 1) * geoTransformResult[4] + (nYSize - 1) * geoTransformResult[5];

    if (poCT == NULL)
    {
        // 에러 처리
        return nullptr;
    }

    // 좌상단 좌표 변환
    if (!poCT->Transform(1, &xTopLeft, &yTopLeft))
    {
        // 변환 실패 처리
        return nullptr;
    }

    // 우하단 좌표 변환
    if (!poCT->Transform(1, &xBottomRight, &yBottomRight))
    {
        // 변환 실패 처리
        return nullptr;
    }

    OGRCoordinateTransformation::DestroyCT(poCT);

    viewshedMemoryType* memoryType = (viewshedMemoryType*)malloc(sizeof(viewshedMemoryType));
    
    if (memoryType != NULL) {
        // pafScanline 사용 종료 후, free 필요
        memoryType->data = pafScanline;
        memoryType->size = nXSize * nYSize;
        memoryType->nXSize = nXSize;
        memoryType->nYSize = nYSize;
        memoryType->xTopLeft = xTopLeft;
    }
   
    GDALDataset::FromHandle(poDstDS.release());

    return memoryType;
}


viewshedMemoryType* ViewshedRenderer::GenerateViewshedRasterBuffer(CDC* pDC) {

    float* returnData = NULL;

    int nBandIn = 1;
    double dfObserverHeight = 2.0;
    double dfTargetHeight = 0.0;
    double dfMaxDistance = 1000.0;
    bool bObserverXSpecified = false;
    double dfObserverX = 961734.0;
    bool bObserverYSpecified = false;
    double dfObserverY = 1930776.0;
    double dfVisibleVal = 255.0;
    double dfInvisibleVal = 0.0;
    double dfOutOfRangeVal = 0.0;
    double dfNoDataVal = -1.0;
    // Value for standard atmospheric refraction. See
    // doc/source/programs/gdal_viewshed.rst
    bool bCurvCoeffSpecified = false;
    double dfCurvCoeff = 0.85714;
    const char* pszDriverName = "MEM";
    const char* pszSrcFilename = "37709.img";
    const char* pszDstFilename = "output.tif";
    bool bQuiet = false;
    GDALProgressFunc pfnProgress = nullptr;
    char** papszCreateOptions = nullptr;
    const char* pszOutputMode = nullptr;

    GDALAllRegister();

    GDALDatasetH hSrcDS = GDALOpen(pszSrcFilename, GA_ReadOnly);
    if (hSrcDS == nullptr)
    {
        // Handle error
        std::cout << "Error :: Load Source File !\n";
    }

    GDALRasterBandH hBand = GDALGetRasterBand(hSrcDS, nBandIn);
    GDALViewshedOutputType outputMode = GVOT_NORMAL; // or GVM_Edge

    /*GDALDatasetH hDstDS = ViewshedGenerate(
        hBand, pszDriverName, pszDstFilename,
        papszCreateOptions, dfObserverX, dfObserverY, dfObserverHeight,
        dfTargetHeight, dfVisibleVal, dfInvisibleVal, dfOutOfRangeVal,
        dfNoDataVal, dfCurvCoeff, GVM_Edge, dfMaxDistance, pfnProgress, nullptr,
        outputMode, nullptr);
    */
    viewshedMemoryType* viewshedResult = ViewshedGenerate(
        hBand, pszDriverName, pszDstFilename,
        papszCreateOptions, dfObserverX, dfObserverY, dfObserverHeight,
        dfTargetHeight, dfVisibleVal, dfInvisibleVal, dfOutOfRangeVal,
        dfNoDataVal, dfCurvCoeff, GVM_Edge, dfMaxDistance, pfnProgress, nullptr,
        outputMode, nullptr);

    //GDALDataset::FromHandle(poDstDS.release());


    CDC memDC;
    memDC.CreateCompatibleDC(pDC);
    CBitmap bitmap;
    bitmap.CreateCompatibleBitmap(pDC, viewshedResult->nXSize, viewshedResult->nYSize);
    CBitmap* pOldBitmap = memDC.SelectObject(&bitmap);

    // 메모리 DC에 그리기
    for (int y = 0; y < viewshedResult->nYSize; y++) {
        for (int x = 0; x < viewshedResult->nXSize; x++) {
            unsigned char pixelValue = viewshedResult->data[y * viewshedResult->nXSize + x];
            if (pixelValue != 0) { // 0이 아닌 경우에만 그리기
                COLORREF color = RGB(pixelValue, pixelValue, pixelValue);
                memDC.SetPixel(x, y, color);
            }
        }
    }

    // 메모리 DC의 내용을 실제 DC로 복사
    pDC->BitBlt(500, 500, viewshedResult->nXSize, viewshedResult->nYSize, &memDC, 0, 0, SRCCOPY);

    // 정리
    memDC.SelectObject(pOldBitmap);
    bitmap.DeleteObject();
    memDC.DeleteDC();


    CBrush brush(RGB(255, 0, 0)); // 빨간색 브러시
    CBrush* pOldBrush = pDC->SelectObject(&brush);
    pDC->Ellipse(50, 50, 150, 150); // 좌표를 사용하여 원 그리기
    pDC->SelectObject(pOldBrush);

    // 필요한 경우 메모리 정리
    // delete[] pPixels;

    return viewshedResult;
};



viewshedMemoryType* ViewshedRenderer::viewshed_generate_via_vector(
    GDALRasterBandH hBand, const char* pszDriverName,
    const char* pszTargetRasterName, CSLConstList papszCreationOptions,
    double dfObserverX, double dfObserverY, double dfObserverHeight,
    double dfTargetHeight, double dfVisibleVal, double dfInvisibleVal,
    double dfOutOfRangeVal, double dfNoDataVal, double dfCurvCoeff,
    GDALViewshedMode eMode, double dfMaxDistance, GDALProgressFunc pfnProgress,
    void* pProgressArg, GDALViewshedOutputType heightMode,
    CSLConstList papszExtraOptions)

{
    VALIDATE_POINTER1(hBand, "GDALViewshedGenerate", nullptr);
    VALIDATE_POINTER1(pszTargetRasterName, "GDALViewshedGenerate", nullptr);

    CPL_IGNORE_RET_VAL(papszExtraOptions);

    if (pfnProgress == nullptr)
        pfnProgress = GDALDummyProgress;

    if (!pfnProgress(0.0, "", pProgressArg))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return nullptr;
    }

    const GByte byNoDataVal = dfNoDataVal >= 0 && dfNoDataVal <= 255
        ? static_cast<GByte>(dfNoDataVal)
        : 0;
    const GByte byVisibleVal = dfVisibleVal >= 0 && dfVisibleVal <= 255
        ? static_cast<GByte>(dfVisibleVal)
        : 255;
    const GByte byInvisibleVal = dfInvisibleVal >= 0 && dfInvisibleVal <= 255
        ? static_cast<GByte>(dfInvisibleVal)
        : 0;
    const GByte byOutOfRangeVal = dfOutOfRangeVal >= 0 && dfOutOfRangeVal <= 255
        ? static_cast<GByte>(dfOutOfRangeVal)
        : 0;

    if (heightMode != GVOT_MIN_TARGET_HEIGHT_FROM_DEM &&
        heightMode != GVOT_MIN_TARGET_HEIGHT_FROM_GROUND)
        heightMode = GVOT_NORMAL;

    /* set up geotransformation */
    std::array<double, 6> adfGeoTransform{{0.0, 1.0, 0.0, 0.0, 0.0, 1.0}};
    GDALDatasetH hSrcDS = GDALGetBandDataset(hBand);
    if (hSrcDS != nullptr)
        GDALGetGeoTransform(hSrcDS, adfGeoTransform.data());

    double adfInvGeoTransform[6];
    if (!GDALInvGeoTransform(adfGeoTransform.data(), adfInvGeoTransform))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot invert geotransform");
        return nullptr;
    }

    /* calculate observer position */
    double dfX, dfY;
    GDALApplyGeoTransform(adfInvGeoTransform, dfObserverX, dfObserverY, &dfX,
        &dfY);
    int nX = static_cast<int>(dfX);
    int nY = static_cast<int>(dfY);

    int nXSize = GDALGetRasterBandXSize(hBand);
    int nYSize = GDALGetRasterBandYSize(hBand);

    if (nX < 0 || nX > nXSize || nY < 0 || nY > nYSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "The observer location falls outside of the DEM area");
        return nullptr;
    }

    /* calculate the area of interest */
    int nXStart =
        dfMaxDistance > 0
        ? (std::max)(0, static_cast<int>(std::floor(
            nX - adfInvGeoTransform[1] * dfMaxDistance)))
        : 0;
    int nXStop =
        dfMaxDistance > 0
        ? (std::min)(nXSize,
            static_cast<int>(std::ceil(nX + adfInvGeoTransform[1] *
                dfMaxDistance) +
                1))
        : nXSize;
    int nYStart =
        dfMaxDistance > 0
        ? (std::max)(0, static_cast<int>(std::floor(
            nY + adfInvGeoTransform[5] * dfMaxDistance)))
        : 0;
    int nYStop =
        dfMaxDistance > 0
        ? (std::min)(nYSize,
            static_cast<int>(std::ceil(nY - adfInvGeoTransform[5] *
                dfMaxDistance) +
                1))
        : nYSize;

    /* normalize horizontal index (0 - nXSize) */
    nXSize = nXStop - nXStart;
    nX -= nXStart;

    nYSize = nYStop - nYStart;

    if (nXSize == 0 || nYSize == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid target raster size");
        return nullptr;
    }

    std::vector<double> vFirstLineVal;
    std::vector<double> vLastLineVal;
    std::vector<double> vThisLineVal;
    std::vector<GByte> vResult;
    std::vector<double> vHeightResult;

    try
    {
        vFirstLineVal.resize(nXSize);
        vLastLineVal.resize(nXSize);
        vThisLineVal.resize(nXSize);
        vResult.resize(nXSize);

        if (heightMode != GVOT_NORMAL)
            vHeightResult.resize(nXSize);
    }
    catch (...)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "Cannot allocate vectors for viewshed");
        return nullptr;
    }

    double* padfFirstLineVal = vFirstLineVal.data();
    double* padfLastLineVal = vLastLineVal.data();
    double* padfThisLineVal = vThisLineVal.data();
    GByte* pabyResult = vResult.data();
    double* dfHeightResult = vHeightResult.data();

    GDALDriverManager* hMgr = GetGDALDriverManager();
    GDALDriver* hDriver =
        hMgr->GetDriverByName(pszDriverName ? pszDriverName : "GTiff");
    if (!hDriver)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get driver");
        return nullptr;
    }
    
    /* create output raster */
    /*auto poDstDS = std::unique_ptr<GDALDataset>(
        hDriver->Create("", nXSize, nYStop - nYStart, 1, GDT_Byte,
            const_cast<char**>(papszCreationOptions)));*/

    GDALDataset* poDstDS = hDriver->Create("", nXSize, nYStop - nYStart, 1, GDT_Byte,
        const_cast<char**>(papszCreationOptions));
    if (!poDstDS)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create dataset for %s",
            pszTargetRasterName);
        return nullptr;
    }
    /* copy srs */
    if (hSrcDS)
        poDstDS->SetSpatialRef(
            GDALDataset::FromHandle(hSrcDS)->GetSpatialRef());

    std::array<double, 6> adfDstGeoTransform;
    adfDstGeoTransform[0] = adfGeoTransform[0] + adfGeoTransform[1] * nXStart +
        adfGeoTransform[2] * nYStart;
    adfDstGeoTransform[1] = adfGeoTransform[1];
    adfDstGeoTransform[2] = adfGeoTransform[2];
    adfDstGeoTransform[3] = adfGeoTransform[3] + adfGeoTransform[4] * nXStart +
        adfGeoTransform[5] * nYStart;
    adfDstGeoTransform[4] = adfGeoTransform[4];
    adfDstGeoTransform[5] = adfGeoTransform[5];
    poDstDS->SetGeoTransform(adfDstGeoTransform.data());

    auto hTargetBand = poDstDS->GetRasterBand(1);
    if (hTargetBand == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot get band for %s",
            pszTargetRasterName);
        return nullptr;
    }

    if (dfNoDataVal >= 0)
        GDALSetRasterNoDataValue(
            hTargetBand, heightMode != GVOT_NORMAL ? dfNoDataVal : byNoDataVal);

    /* process first line */
    if (GDALRasterIO(hBand, GF_Read, nXStart, nY, nXSize, 1, padfFirstLineVal,
        nXSize, 1, GDT_Float64, 0, 0))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "RasterIO error when reading DEM at position(%d, %d), size(%d, %d)",
            nXStart, nY, nXSize, 1);
        return nullptr;
    }

    const double dfZObserver = dfObserverHeight + padfFirstLineVal[nX];
    double dfZ = 0.0;
    const double dfDistance2 = dfMaxDistance * dfMaxDistance;

    /* If we can't get a SemiMajor axis from the SRS, it will be
     * SRS_WGS84_SEMIMAJOR
     */
    double dfSphereDiameter(std::numeric_limits<double>::infinity());
    const OGRSpatialReference* poDstSRS = poDstDS->GetSpatialRef();
    if (poDstSRS)
    {
        OGRErr eSRSerr;
        double dfSemiMajor = poDstSRS->GetSemiMajor(&eSRSerr);

        /* If we fetched the axis from the SRS, use it */
        if (eSRSerr != OGRERR_FAILURE)
            dfSphereDiameter = dfSemiMajor * 2.0;
        else
            CPLDebug("GDALViewshedGenerate",
                "Unable to fetch SemiMajor axis from spatial reference");
    }

    /* mark the observer point as visible */
    double dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
        ? padfFirstLineVal[nX]
        : 0.0;
    pabyResult[nX] = byVisibleVal;
    if (heightMode != GVOT_NORMAL)
        dfHeightResult[nX] = dfGroundLevel;

    if (nX > 0)
    {
        dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
            ? padfFirstLineVal[nX - 1]
            : 0.0;
        CPL_IGNORE_RET_VAL(AdjustHeightInRange(
            adfGeoTransform.data(), 1, 0, padfFirstLineVal[nX - 1], dfDistance2,
            dfCurvCoeff, dfSphereDiameter));
        pabyResult[nX - 1] = byVisibleVal;
        if (heightMode != GVOT_NORMAL)
            dfHeightResult[nX - 1] = dfGroundLevel;
    }
    if (nX < nXSize - 1)
    {
        dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
            ? padfFirstLineVal[nX + 1]
            : 0.0;
        CPL_IGNORE_RET_VAL(AdjustHeightInRange(
            adfGeoTransform.data(), 1, 0, padfFirstLineVal[nX + 1], dfDistance2,
            dfCurvCoeff, dfSphereDiameter));
        pabyResult[nX + 1] = byVisibleVal;
        if (heightMode != GVOT_NORMAL)
            dfHeightResult[nX + 1] = dfGroundLevel;
    }

    /* process left direction */
    for (int iPixel = nX - 2; iPixel >= 0; iPixel--)
    {
        dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
            ? padfFirstLineVal[iPixel]
            : 0.0;
        bool adjusted = AdjustHeightInRange(
            adfGeoTransform.data(), nX - iPixel, 0, padfFirstLineVal[iPixel],
            dfDistance2, dfCurvCoeff, dfSphereDiameter);
        if (adjusted)
        {
            dfZ = CalcHeightLine(nX - iPixel, padfFirstLineVal[iPixel + 1],
                dfZObserver);

            if (heightMode != GVOT_NORMAL)
                dfHeightResult[iPixel] = std::max(
                    0.0, (dfZ - padfFirstLineVal[iPixel] + dfGroundLevel));

            SetVisibility(iPixel, dfZ, dfTargetHeight, padfFirstLineVal,
                vResult, byVisibleVal, byInvisibleVal);
        }
        else
        {
            for (; iPixel >= 0; iPixel--)
            {
                pabyResult[iPixel] = byOutOfRangeVal;
                if (heightMode != GVOT_NORMAL)
                    dfHeightResult[iPixel] = dfOutOfRangeVal;
            }
        }
    }
    /* process right direction */
    for (int iPixel = nX + 2; iPixel < nXSize; iPixel++)
    {
        dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
            ? padfFirstLineVal[iPixel]
            : 0.0;
        bool adjusted = AdjustHeightInRange(
            adfGeoTransform.data(), iPixel - nX, 0, padfFirstLineVal[iPixel],
            dfDistance2, dfCurvCoeff, dfSphereDiameter);
        if (adjusted)
        {
            dfZ = CalcHeightLine(iPixel - nX, padfFirstLineVal[iPixel - 1],
                dfZObserver);

            if (heightMode != GVOT_NORMAL)
                dfHeightResult[iPixel] = std::max(
                    0.0, (dfZ - padfFirstLineVal[iPixel] + dfGroundLevel));

            SetVisibility(iPixel, dfZ, dfTargetHeight, padfFirstLineVal,
                vResult, byVisibleVal, byInvisibleVal);
        }
        else
        {
            for (; iPixel < nXSize; iPixel++)
            {
                pabyResult[iPixel] = byOutOfRangeVal;
                if (heightMode != GVOT_NORMAL)
                    dfHeightResult[iPixel] = dfOutOfRangeVal;
            }
        }
    }
    /* write result line */

    if (GDALRasterIO(hTargetBand, GF_Write, 0, nY - nYStart, nXSize, 1,
        heightMode != GVOT_NORMAL
        ? static_cast<void*>(dfHeightResult)
        : static_cast<void*>(pabyResult),
        nXSize, 1,
        heightMode != GVOT_NORMAL ? GDT_Float64 : GDT_Byte, 0, 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
            "RasterIO error when writing target raster at position "
            "(%d,%d), size (%d,%d)",
            0, nY - nYStart, nXSize, 1);
        return nullptr;
    }

    /* scan upwards */
    std::copy(vFirstLineVal.begin(), vFirstLineVal.end(), vLastLineVal.begin());
    for (int iLine = nY - 1; iLine >= nYStart; iLine--)
    {
        if (GDALRasterIO(hBand, GF_Read, nXStart, iLine, nXSize, 1,
            padfThisLineVal, nXSize, 1, GDT_Float64, 0, 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "RasterIO error when reading DEM at position (%d,%d), "
                "size (%d,%d)",
                nXStart, iLine, nXSize, 1);
            return nullptr;
        }

        /* set up initial point on the scanline */
        dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
            ? padfThisLineVal[nX]
            : 0.0;
        bool adjusted = AdjustHeightInRange(
            adfGeoTransform.data(), 0, nY - iLine, padfThisLineVal[nX],
            dfDistance2, dfCurvCoeff, dfSphereDiameter);
        if (adjusted)
        {
            dfZ = CalcHeightLine(nY - iLine, padfLastLineVal[nX], dfZObserver);

            if (heightMode != GVOT_NORMAL)
                dfHeightResult[nX] =
                std::max(0.0, (dfZ - padfThisLineVal[nX] + dfGroundLevel));

            SetVisibility(nX, dfZ, dfTargetHeight, padfThisLineVal, vResult,
                byVisibleVal, byInvisibleVal);
        }
        else
        {
            pabyResult[nX] = byOutOfRangeVal;
            if (heightMode != GVOT_NORMAL)
                dfHeightResult[nX] = dfOutOfRangeVal;
        }

        /* process left direction */
        for (int iPixel = nX - 1; iPixel >= 0; iPixel--)
        {
            dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                ? padfThisLineVal[iPixel]
                : 0.0;
            bool left_adjusted =
                AdjustHeightInRange(adfGeoTransform.data(), nX - iPixel,
                    nY - iLine, padfThisLineVal[iPixel],
                    dfDistance2, dfCurvCoeff, dfSphereDiameter);
            if (left_adjusted)
            {
                if (eMode != GVM_Edge)
                    dfZ = CalcHeightDiagonal(
                        nX - iPixel, nY - iLine, padfThisLineVal[iPixel + 1],
                        padfLastLineVal[iPixel], dfZObserver);

                if (eMode != GVM_Diagonal)
                {
                    double dfZ2 =
                        nX - iPixel >= nY - iLine
                        ? CalcHeightEdge(nY - iLine, nX - iPixel,
                            padfLastLineVal[iPixel + 1],
                            padfThisLineVal[iPixel + 1],
                            dfZObserver)
                        : CalcHeightEdge(nX - iPixel, nY - iLine,
                            padfLastLineVal[iPixel + 1],
                            padfLastLineVal[iPixel],
                            dfZObserver);
                    dfZ = CalcHeight(dfZ, dfZ2, eMode);
                }

                if (heightMode != GVOT_NORMAL)
                    dfHeightResult[iPixel] = std::max(
                        0.0, (dfZ - padfThisLineVal[iPixel] + dfGroundLevel));

                SetVisibility(iPixel, dfZ, dfTargetHeight, padfThisLineVal,
                    vResult, byVisibleVal, byInvisibleVal);
            }
            else
            {
                for (; iPixel >= 0; iPixel--)
                {
                    pabyResult[iPixel] = byOutOfRangeVal;
                    if (heightMode != GVOT_NORMAL)
                        dfHeightResult[iPixel] = dfOutOfRangeVal;
                }
            }
        }
        /* process right direction */
        for (int iPixel = nX + 1; iPixel < nXSize; iPixel++)
        {
            dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                ? padfThisLineVal[iPixel]
                : 0.0;
            bool right_adjusted =
                AdjustHeightInRange(adfGeoTransform.data(), iPixel - nX,
                    nY - iLine, padfThisLineVal[iPixel],
                    dfDistance2, dfCurvCoeff, dfSphereDiameter);
            if (right_adjusted)
            {
                if (eMode != GVM_Edge)
                    dfZ = CalcHeightDiagonal(
                        iPixel - nX, nY - iLine, padfThisLineVal[iPixel - 1],
                        padfLastLineVal[iPixel], dfZObserver);

                if (eMode != GVM_Diagonal)
                {
                    double dfZ2 =
                        iPixel - nX >= nY - iLine
                        ? CalcHeightEdge(nY - iLine, iPixel - nX,
                            padfLastLineVal[iPixel - 1],
                            padfThisLineVal[iPixel - 1],
                            dfZObserver)
                        : CalcHeightEdge(iPixel - nX, nY - iLine,
                            padfLastLineVal[iPixel - 1],
                            padfLastLineVal[iPixel],
                            dfZObserver);
                    dfZ = CalcHeight(dfZ, dfZ2, eMode);
                }

                if (heightMode != GVOT_NORMAL)
                    dfHeightResult[iPixel] = std::max(
                        0.0, (dfZ - padfThisLineVal[iPixel] + dfGroundLevel));

                SetVisibility(iPixel, dfZ, dfTargetHeight, padfThisLineVal,
                    vResult, byVisibleVal, byInvisibleVal);
            }
            else
            {
                for (; iPixel < nXSize; iPixel++)
                {
                    pabyResult[iPixel] = byOutOfRangeVal;
                    if (heightMode != GVOT_NORMAL)
                        dfHeightResult[iPixel] = dfOutOfRangeVal;
                }
            }
        }

        /* write result line */
        if (GDALRasterIO(
            hTargetBand, GF_Write, 0, iLine - nYStart, nXSize, 1,
            heightMode != GVOT_NORMAL ? static_cast<void*>(dfHeightResult)
            : static_cast<void*>(pabyResult),
            nXSize, 1, heightMode != GVOT_NORMAL ? GDT_Float64 : GDT_Byte,
            0, 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "RasterIO error when writing target raster at position "
                "(%d,%d), size (%d,%d)",
                0, iLine - nYStart, nXSize, 1);
            return nullptr;
        }

        std::swap(padfLastLineVal, padfThisLineVal);

        if (!pfnProgress((nY - iLine) / static_cast<double>(nYSize), "",
            pProgressArg))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            return nullptr;
        }
    }
    /* scan downwards */
    memcpy(padfLastLineVal, padfFirstLineVal, nXSize * sizeof(double));
    for (int iLine = nY + 1; iLine < nYStop; iLine++)
    {
        if (GDALRasterIO(hBand, GF_Read, nXStart, iLine, nXStop - nXStart, 1,
            padfThisLineVal, nXStop - nXStart, 1, GDT_Float64, 0,
            0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "RasterIO error when reading DEM at position (%d,%d), "
                "size (%d,%d)",
                nXStart, iLine, nXStop - nXStart, 1);
            return nullptr;
        }

        /* set up initial point on the scanline */
        dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
            ? padfThisLineVal[nX]
            : 0.0;
        bool adjusted = AdjustHeightInRange(
            adfGeoTransform.data(), 0, iLine - nY, padfThisLineVal[nX],
            dfDistance2, dfCurvCoeff, dfSphereDiameter);
        if (adjusted)
        {
            dfZ = CalcHeightLine(iLine - nY, padfLastLineVal[nX], dfZObserver);

            if (heightMode != GVOT_NORMAL)
                dfHeightResult[nX] =
                std::max(0.0, (dfZ - padfThisLineVal[nX] + dfGroundLevel));

            SetVisibility(nX, dfZ, dfTargetHeight, padfThisLineVal, vResult,
                byVisibleVal, byInvisibleVal);
        }
        else
        {
            pabyResult[nX] = byOutOfRangeVal;
            if (heightMode != GVOT_NORMAL)
                dfHeightResult[nX] = dfOutOfRangeVal;
        }

        /* process left direction */
        for (int iPixel = nX - 1; iPixel >= 0; iPixel--)
        {
            dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                ? padfThisLineVal[iPixel]
                : 0.0;
            bool left_adjusted =
                AdjustHeightInRange(adfGeoTransform.data(), nX - iPixel,
                    iLine - nY, padfThisLineVal[iPixel],
                    dfDistance2, dfCurvCoeff, dfSphereDiameter);
            if (left_adjusted)
            {
                if (eMode != GVM_Edge)
                    dfZ = CalcHeightDiagonal(
                        nX - iPixel, iLine - nY, padfThisLineVal[iPixel + 1],
                        padfLastLineVal[iPixel], dfZObserver);

                if (eMode != GVM_Diagonal)
                {
                    double dfZ2 =
                        nX - iPixel >= iLine - nY
                        ? CalcHeightEdge(iLine - nY, nX - iPixel,
                            padfLastLineVal[iPixel + 1],
                            padfThisLineVal[iPixel + 1],
                            dfZObserver)
                        : CalcHeightEdge(nX - iPixel, iLine - nY,
                            padfLastLineVal[iPixel + 1],
                            padfLastLineVal[iPixel],
                            dfZObserver);
                    dfZ = CalcHeight(dfZ, dfZ2, eMode);
                }

                if (heightMode != GVOT_NORMAL)
                    dfHeightResult[iPixel] = std::max(
                        0.0, (dfZ - padfThisLineVal[iPixel] + dfGroundLevel));

                SetVisibility(iPixel, dfZ, dfTargetHeight, padfThisLineVal,
                    vResult, byVisibleVal, byInvisibleVal);
            }
            else
            {
                for (; iPixel >= 0; iPixel--)
                {
                    pabyResult[iPixel] = byOutOfRangeVal;
                    if (heightMode != GVOT_NORMAL)
                        dfHeightResult[iPixel] = dfOutOfRangeVal;
                }
            }
        }
        /* process right direction */
        for (int iPixel = nX + 1; iPixel < nXSize; iPixel++)
        {
            dfGroundLevel = heightMode == GVOT_MIN_TARGET_HEIGHT_FROM_DEM
                ? padfThisLineVal[iPixel]
                : 0.0;
            bool right_adjusted =
                AdjustHeightInRange(adfGeoTransform.data(), iPixel - nX,
                    iLine - nY, padfThisLineVal[iPixel],
                    dfDistance2, dfCurvCoeff, dfSphereDiameter);
            if (right_adjusted)
            {
                if (eMode != GVM_Edge)
                    dfZ = CalcHeightDiagonal(
                        iPixel - nX, iLine - nY, padfThisLineVal[iPixel - 1],
                        padfLastLineVal[iPixel], dfZObserver);

                if (eMode != GVM_Diagonal)
                {
                    double dfZ2 =
                        iPixel - nX >= iLine - nY
                        ? CalcHeightEdge(iLine - nY, iPixel - nX,
                            padfLastLineVal[iPixel - 1],
                            padfThisLineVal[iPixel - 1],
                            dfZObserver)
                        : CalcHeightEdge(iPixel - nX, iLine - nY,
                            padfLastLineVal[iPixel - 1],
                            padfLastLineVal[iPixel],
                            dfZObserver);
                    dfZ = CalcHeight(dfZ, dfZ2, eMode);
                }

                if (heightMode != GVOT_NORMAL)
                    dfHeightResult[iPixel] = std::max(
                        0.0, (dfZ - padfThisLineVal[iPixel] + dfGroundLevel));

                SetVisibility(iPixel, dfZ, dfTargetHeight, padfThisLineVal,
                    vResult, byVisibleVal, byInvisibleVal);
            }
            else
            {
                for (; iPixel < nXSize; iPixel++)
                {
                    pabyResult[iPixel] = byOutOfRangeVal;
                    if (heightMode != GVOT_NORMAL)
                        dfHeightResult[iPixel] = dfOutOfRangeVal;
                }
            }
        }

        /* write result line */
        if (GDALRasterIO(
            hTargetBand, GF_Write, 0, iLine - nYStart, nXSize, 1,
            heightMode != GVOT_NORMAL ? static_cast<void*>(dfHeightResult)
            : static_cast<void*>(pabyResult),
            nXSize, 1, heightMode != GVOT_NORMAL ? GDT_Float64 : GDT_Byte,
            0, 0))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                "RasterIO error when writing target raster at position "
                "(%d,%d), size (%d,%d)",
                0, iLine - nYStart, nXSize, 1);
            return nullptr;
        }

        std::swap(padfLastLineVal, padfThisLineVal);

        if (!pfnProgress((iLine - nYStart) / static_cast<double>(nYSize), "",
            pProgressArg))
        {
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
            return nullptr;
        }
    }

    if (!pfnProgress(1.0, "", pProgressArg))
    {
        CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated");
        return nullptr;
    }




    unsigned char* pafScanline = (unsigned char*)CPLMalloc(sizeof(unsigned char) * nXSize * nYSize);
    hTargetBand->RasterIO(GF_Read, 0, 0, nXSize, nYSize, pafScanline, nXSize, nYSize, GDT_Byte, 0, 0);

    double geoTransformResult[6];
    poDstDS->GetGeoTransform(geoTransformResult);
    const char* pszProjection = poDstDS->GetProjectionRef();

    OGRSpatialReference oSourceSRS;
    oSourceSRS.importFromWkt(pszProjection);

    OGRSpatialReference oTargetSRS;
    oTargetSRS.importFromEPSG(3857);

    OGRCoordinateTransformation* poCT = OGRCreateCoordinateTransformation(&oSourceSRS, &oTargetSRS);

    double xTopLeft = geoTransformResult[0];
    double yTopLeft = geoTransformResult[3];
    double xBottomRight = geoTransformResult[0] + (nXSize - 1) * geoTransformResult[1] + (nYSize - 1) * geoTransformResult[2];
    double yBottomRight = geoTransformResult[3] + (nXSize - 1) * geoTransformResult[4] + (nYSize - 1) * geoTransformResult[5];

    if (poCT == NULL)
    {
        // 에러 처리
        return nullptr;
    }

    // 좌상단 좌표 변환
    if (!poCT->Transform(1, &xTopLeft, &yTopLeft))
    {
        // 변환 실패 처리
        return nullptr;
    }

    // 우하단 좌표 변환
    if (!poCT->Transform(1, &xBottomRight, &yBottomRight))
    {
        // 변환 실패 처리
        return nullptr;
    }

    OGRCoordinateTransformation::DestroyCT(poCT);

    viewshedMemoryType* memoryType = (viewshedMemoryType*)malloc(sizeof(viewshedMemoryType));

    if (memoryType != NULL) {
        // pafScanline 사용 종료 후, free 필요
        memoryType->data = pafScanline;
        memoryType->size = nXSize * nYSize;
        memoryType->nXSize = nXSize;
        memoryType->nYSize = nYSize;
        memoryType->xTopLeft = xTopLeft;
    }

    /*
    // additional code for building vector binary data
    GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GeoJSON");

    char** papszOptions = nullptr;
    papszOptions = CSLSetNameValue(papszOptions, "FORMAT", "MEMORY");
    GDALDataset* poVectorDS = poDriver->Create("", 0, 0, 0, GDT_Unknown, papszOptions);
    CSLDestroy(papszOptions);
    */

    // GDALDriver를 사용하여 메모리 내에 GeoJSON 데이터셋을 생성
    GDALDriver* poDriver = GetGDALDriverManager()->GetDriverByName("GeoJSON");

    // Create a new dataset in memory
    GDALDataset* poVectorDS = poDriver->Create("/vsimem/in_memory_output.geojson", 0, 0, 0, GDT_Unknown, nullptr);
    OGRLayer* poLayer = poVectorDS->CreateLayer("contour", nullptr, wkbLineString, nullptr);


    CPLErrorReset();
    CPLPushErrorHandler(GDALErrorHandler);

    GDALContourGenerate(
        poDstDS->GetRasterBand(1),  // 입력 래스터 밴드
        10.0,                       // 등고선 간격
        0.0,                        // 등고선 기본 높이
        0,                          // 고정된 등고선 레벨 수 (0이면 사용하지 않음)
        nullptr,                    // 고정된 등고선 레벨 배열
        FALSE,                      // NoData 값을 사용할지 여부
        0.0,                        // NoData 값 (사용하지 않으면 무시됨)
        poVectorDS->GetLayer(0),       // 출력 벡터 레이어
        -1,                         // ID 필드 인덱스 (생성하지 않으려면 -1)
        -1,                         // 고도 필드 인덱스 (생성하지 않으려면 -1)
        nullptr,                    // 진행률 콜백 함수 (사용하지 않으면 nullptr)
        nullptr                     // 진행률 콜백 함수의 사용자 데이터 (사용하지 않으면 nullptr)
    );

    if (CPLGetLastErrorType() != CE_None) // 오류가 발생한 경우
    {
        const char* pszErrorMsg = CPLGetLastErrorMsg();
        printf("GDAL Error: %s\n", pszErrorMsg); // 오류 메시지를 출력합니다.
    }
    else
    {
        printf("No GDAL errors detected.\n");
    }




    printf("aaa\n");


    char* pMemoryAddress = GetDatasetMemoryAddress(poDstDS);

    VSILFILE* fp = VSIFOpenL("/vsimem/in_memory_output.geojson", "rb");
    char* pszGeoJSON = nullptr;
    if (fp)
    {
        VSIFSeekL(fp, 0, SEEK_END);
        vsi_l_offset nLength = VSIFTellL(fp);
        pszGeoJSON = (char*)CPLMalloc(nLength + 1);
        VSIFSeekL(fp, 0, SEEK_SET);
        VSIFReadL(pszGeoJSON, 1, nLength, fp);
        pszGeoJSON[nLength] = '\0';
    }

    // 이제 pszGeoJSON에 GeoJSON 바이트 스트림이 있습니다.
    // 필요에 따라 CEF와 OpenLayers에 전달할 수 있습니다.

    // 정리
    CPLFree(pszGeoJSON);
    GDALClose(poVectorDS);
    VSIUnlink("/vsimem/output.geojson");

    GDALClose(poDstDS);
    //GDALDataset::FromHandle(poDstDS.release());

    return memoryType;
}


char* ViewshedRenderer::generate_viewshed_vector_buffer(CDC* pDC) {

    float* returnData = NULL;

    int nBandIn = 1;
    double dfObserverHeight = 2.0;
    double dfTargetHeight = 0.0;
    double dfMaxDistance = 1000.0;
    bool bObserverXSpecified = false;
    double dfObserverX = 961734.0;
    bool bObserverYSpecified = false;
    double dfObserverY = 1930776.0;
    double dfVisibleVal = 255.0;
    double dfInvisibleVal = 0.0;
    double dfOutOfRangeVal = 0.0;
    double dfNoDataVal = -1.0;
    // Value for standard atmospheric refraction. See
    // doc/source/programs/gdal_viewshed.rst
    bool bCurvCoeffSpecified = false;
    double dfCurvCoeff = 0.85714;
    const char* pszDriverName = "MEM";
    const char* pszSrcFilename = "37709.img";
    const char* pszDstFilename = "output.tif";
    bool bQuiet = false;
    GDALProgressFunc pfnProgress = nullptr;
    char** papszCreateOptions = nullptr;
    const char* pszOutputMode = nullptr;

    GDALAllRegister();

    GDALDatasetH hSrcDS = GDALOpen(pszSrcFilename, GA_ReadOnly);
    if (hSrcDS == nullptr)
    {
        // Handle error
        std::cout << "Error :: Load Source File !\n";
    }

    GDALRasterBandH hBand = GDALGetRasterBand(hSrcDS, nBandIn);
    GDALViewshedOutputType outputMode = GVOT_NORMAL; // or GVM_Edge

    /*GDALDatasetH hDstDS = ViewshedGenerate(
        hBand, pszDriverName, pszDstFilename,
        papszCreateOptions, dfObserverX, dfObserverY, dfObserverHeight,
        dfTargetHeight, dfVisibleVal, dfInvisibleVal, dfOutOfRangeVal,
        dfNoDataVal, dfCurvCoeff, GVM_Edge, dfMaxDistance, pfnProgress, nullptr,
        outputMode, nullptr);
    */
    viewshedMemoryType* viewshedResult = viewshed_generate_via_vector(
        hBand, pszDriverName, pszDstFilename,
        papszCreateOptions, dfObserverX, dfObserverY, dfObserverHeight,
        dfTargetHeight, dfVisibleVal, dfInvisibleVal, dfOutOfRangeVal,
        dfNoDataVal, dfCurvCoeff, GVM_Edge, dfMaxDistance, pfnProgress, nullptr,
        outputMode, nullptr);

    //GDALDataset::FromHandle(poDstDS.release());
    char* data = nullptr;

    return data;
};