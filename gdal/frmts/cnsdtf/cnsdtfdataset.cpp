/******************************************************************************
*
* Project:  GDAL
* Purpose:  Implements CNSDTF Grid Format.
* Author:   Minlu Li, liminlu0314@163.com
* Describe: CNSDTF Grid format. Reference:GB/T 17798-2007
*
******************************************************************************
* Copyright (c) 2013, Minlu Li (liminlu0314@163.com)
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
****************************************************************************/

#include "cnsdtfsrs.h"
#include "ascdataset.h"

/************************************************************************/
/* ==================================================================== */
/*                              CNSDTFDataset                           */
/* ==================================================================== */
/************************************************************************/

class CPL_DLL CNSDTFDataset : public AscDataset
{
    friend class AscRasterBand;

    // DataMark Version Compress Alpha
    CPLString	osDataMark;
    CPLString	osDataVersion;
    GBool       bIsCompress;
    double      dfAlphaValue;
    CPLString   osValueType;
    CPLString   osUnitType;
    double      dfMin;
    double      dfMax;

protected:
    virtual int ParseHeader(const char* pszHeader, int& nStartOfData);

public:
    CNSDTFDataset();
    ~CNSDTFDataset();

    static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
    static int          Identify(GDALOpenInfo* poOpenInfo);
    static GDALDataset* CreateCopy(const char* pszFilename,
        GDALDataset* poSrcDS, int bStrict, char** papszOptions,
        GDALProgressFunc pfnProgress, void* pProgressData);
};

/************************************************************************/
/*                            CNSDTFDataset()                           */
/************************************************************************/

CNSDTFDataset::CNSDTFDataset()
    :AscDataset()
{
    osDataMark = "CNSDTF-RAS";
    osDataVersion = "GB/T 17798-2007";
    bIsCompress = FALSE;
    dfAlphaValue = 0.0;
    osValueType = "Integer";
    dfMin = 0.0;
    dfMax = 0.0;
}

/************************************************************************/
/*                           ~CNSDTFDataset()                           */
/************************************************************************/

CNSDTFDataset::~CNSDTFDataset()
{
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int CNSDTFDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    /* -------------------------------------------------------------------- */
    /*      Does this look like an CNSDTF grid file?                        */
    /* -------------------------------------------------------------------- */
    char** papszTokens = CSLTokenizeString2((const char*)poOpenInfo->pabyHeader, "\n\r", 0);
    int nTokens = CSLCount(papszTokens);
    if (nTokens < 12)	//the header at (the) least 12 lines
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    if (!(EQUALN(papszTokens[0], "DataMark", 8) && EQUALN(papszTokens[1], "Version", 7)))
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    char** papszDataMark = CSLTokenizeString2(papszTokens[0], " :：", 0);
    if (CSLCount(papszDataMark) != 2)
    {
        CSLDestroy(papszDataMark);
        CSLDestroy(papszTokens);
        return FALSE;
    }

    CSLDestroy(papszTokens);

    if (!(EQUALN(papszDataMark[0], "DataMark", 8) &&
        (EQUALN(papszDataMark[1], "CNSDTF-DEM", 10) ||
            EQUALN(papszDataMark[1], "CNSDTF-RAS", 10)))
        )
    {
        CSLDestroy(papszDataMark);
        return FALSE;
    }

    CSLDestroy(papszDataMark);
    return TRUE;
}

/************************************************************************/
/*                          ParseHeader()                               */
/************************************************************************/

int CNSDTFDataset::ParseHeader(const char* pszHeader, int& nStartOfData)
{
    char** papszTokens = CSLTokenizeString2(pszHeader, "\n\r", 0);

    osDataMark = ParseKeyValue(papszTokens, "DataMark");
    osDataVersion = ParseKeyValue(papszTokens, "Version");
    osUnitType = ParseKeyValue(papszTokens, "Unit");

    dfAlphaValue = CPLAtofM(ParseKeyValue(papszTokens, "Alpha").c_str());
    bIsCompress = atoi(ParseKeyValue(papszTokens, "Compress").c_str());
    dfScale = CPLAtofM(ParseKeyValue(papszTokens, "HZoom").c_str());

    nRasterYSize = atoi(ParseKeyValue(papszTokens, "Row").c_str());
    nRasterXSize = atoi(ParseKeyValue(papszTokens, "Col").c_str());
    if (nRasterXSize == 0)
        nRasterXSize = atoi(ParseKeyValue(papszTokens, "Column").c_str());

    if (!GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize))
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    double dfX0 = CPLAtofM(ParseKeyValue(papszTokens, "X0").c_str());
    double dfY0 = CPLAtofM(ParseKeyValue(papszTokens, "Y0").c_str());

    double dfCellDX = CPLAtofM(ParseKeyValue(papszTokens, "DX").c_str());
    double dfCellDY = CPLAtofM(ParseKeyValue(papszTokens, "DY").c_str());

    adfGeoTransform[0] = dfX0;
    adfGeoTransform[1] = dfCellDX;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = dfY0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = -dfCellDY;

    osValueType = ParseKeyValue(papszTokens, "ValueType");
    if (EQUAL(osValueType, "Integer"))
    {
        eDataType = GDT_Int32;
        bHaveNoDataValue = TRUE;
        dfNoDataValue = -9999.0;
    }
    else if (EQUAL(osValueType, "Char"))
    {
        eDataType = GDT_Byte;
    }
    else
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    dfMin = CPLAtofM(ParseKeyValue(papszTokens, "MinV").c_str());
    dfMax = CPLAtofM(ParseKeyValue(papszTokens, "MaxV").c_str());

    int nPos = FindString(pszHeader, "MaxV:");
    nStartOfData = nPos + 5;

    while (pszHeader[nStartOfData] == ' ' || pszHeader[nStartOfData] == '\n' || pszHeader[nStartOfData] == '\r')
    {
        nStartOfData++;
    }

    CSLDestroy(papszTokens);
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* CNSDTFDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    CNSDTFDataset* poDS;
    poDS = new CNSDTFDataset();

    /* -------------------------------------------------------------------- */
    /*      Parse the header.                                               */
    /* -------------------------------------------------------------------- */
    int nStartOfData = -1;
    if (!poDS->ParseHeader((const char*)poOpenInfo->pabyHeader, nStartOfData))
    {
        delete poDS;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Open file with large file API.                                  */
    /* -------------------------------------------------------------------- */

    poDS->fp = VSIFOpenL(poOpenInfo->pszFilename, "r");
    if (poDS->fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
            "VSIFOpenL(%s) failed unexpectedly.",
            poOpenInfo->pszFilename);
        delete poDS;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Recognize the type of data.										*/
    /* -------------------------------------------------------------------- */
    CPLAssert(nullptr != poDS->fp);

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    AscRasterBand* band = new AscRasterBand(poDS, nStartOfData);
    poDS->SetBand(1, band);
    if (band->panLineOffset == nullptr)
    {
        delete poDS;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Parse the SRS.                                                  */
    /* -------------------------------------------------------------------- */
    CPLString strProjection = ParseSpatialReference((const char*)poOpenInfo->pabyHeader);
    if (strProjection != "")
    {
        CPLFree(poDS->pszProjection);
        poDS->pszProjection = CPLStrdup(strProjection.c_str());
    }

    if (EQUAL(poDS->pszProjection, ""))
    {
        /* -------------------------------------------------------------------- */
        /*      Try to read projection file.                                    */
        /* -------------------------------------------------------------------- */
        char* pszDirname, * pszBasename;
        VSIStatBufL   sStatBuf;

        pszDirname = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
        pszBasename = CPLStrdup(CPLGetBasename(poOpenInfo->pszFilename));

        poDS->osPrjFilename = CPLFormFilename(pszDirname, pszBasename, "prj");
        int nRet = VSIStatL(poDS->osPrjFilename, &sStatBuf);

        if (nRet != 0 && VSIIsCaseSensitiveFS(poDS->osPrjFilename))
        {
            poDS->osPrjFilename = CPLFormFilename(pszDirname, pszBasename, "PRJ");
            nRet = VSIStatL(poDS->osPrjFilename, &sStatBuf);
        }

        if (nRet == 0)
        {
            OGRSpatialReference     oSRS;

            poDS->papszPrj = CSLLoad(poDS->osPrjFilename);

            CPLDebug("CNSDTF Grid", "Loaded SRS from %s",
                poDS->osPrjFilename.c_str());

            if (oSRS.importFromESRI(poDS->papszPrj) == OGRERR_NONE)
            {
                // If geographic values are in seconds, we must transform. 
                // Is there a code for minutes too? 
                if (oSRS.IsGeographic()
                    && EQUAL(OSR_GDS(poDS->papszPrj, "Units", ""), "DS"))
                {
                    poDS->adfGeoTransform[0] /= 3600.0;
                    poDS->adfGeoTransform[1] /= 3600.0;
                    poDS->adfGeoTransform[2] /= 3600.0;
                    poDS->adfGeoTransform[3] /= 3600.0;
                    poDS->adfGeoTransform[4] /= 3600.0;
                    poDS->adfGeoTransform[5] /= 3600.0;
                }

                CPLFree(poDS->pszProjection);
                oSRS.exportToWkt(&(poDS->pszProjection));
            }
        }

        CPLFree(pszDirname);
        CPLFree(pszBasename);
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Check for external overviews.                                   */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);

    return(poDS);
}

/************************************************************************/
/*                          CreateCopy()                                */
/************************************************************************/

GDALDataset* CNSDTFDataset::CreateCopy(
    const char* pszFilename, GDALDataset* poSrcDS,
    int bStrict, char** papszOptions,
    GDALProgressFunc pfnProgress, void* pProgressData)

{
    int  nBands = poSrcDS->GetRasterCount();
    int  nXSize = poSrcDS->GetRasterXSize();
    int  nYSize = poSrcDS->GetRasterYSize();

    /* -------------------------------------------------------------------- */
    /*      Some rudimentary checks                                         */
    /* -------------------------------------------------------------------- */
    if (nBands != 1)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
            "CNSDTF Grid driver doesn't support %d bands.  Must be 1 band.\n",
            nBands);

        return nullptr;
    }

    if (!pfnProgress(0.0, nullptr, pProgressData))
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create the dataset.                                             */
    /* -------------------------------------------------------------------- */
    VSILFILE* fpImage;

    fpImage = VSIFOpenL(pszFilename, "wt");
    if (fpImage == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
            "Unable to create file %s.\n", pszFilename);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Try to write projection file.                                   */
    /* -------------------------------------------------------------------- */
    const char* pszOriginalProjection = poSrcDS->GetProjectionRef();
    const char* pszMeterUnit = "M";
    if (!EQUAL(pszOriginalProjection, ""))
    {
        char* pszDirname, * pszBasename;
        char* pszPrjFilename;
        char* pszESRIProjection = nullptr;
        VSILFILE* fp;
        OGRSpatialReference     oSRS;

        pszDirname = CPLStrdup(CPLGetPath(pszFilename));
        pszBasename = CPLStrdup(CPLGetBasename(pszFilename));

        pszPrjFilename = CPLStrdup(CPLFormFilename(pszDirname, pszBasename, "prj"));
        fp = VSIFOpenL(pszPrjFilename, "wt");
        if (fp != nullptr)
        {
            oSRS.importFromWkt((char**)&pszOriginalProjection);
            oSRS.morphToESRI();
            oSRS.exportToWkt(&pszESRIProjection);
            VSIFWriteL(pszESRIProjection, 1, strlen(pszESRIProjection), fp);

            VSIFCloseL(fp);
            CPLFree(pszESRIProjection);
        }
        else
        {
            CPLError(CE_Failure, CPLE_FileIO,
                "Unable to create file %s.\n", pszPrjFilename);
        }
        CPLFree(pszDirname);
        CPLFree(pszBasename);
        CPLFree(pszPrjFilename);

        if (oSRS.IsGeographic())
            pszMeterUnit = "D";
    }

    /* -------------------------------------------------------------------- */
    /*      Write CNSDTF Grid file header                                   */
    /* -------------------------------------------------------------------- */
    char szHeader[2000];
    const char* pszForceRaster = CSLFetchNameValue(papszOptions, "FORCE_RASTER");
    char* pszHeaderMark = "DataMark:CNSDTF-DEM";
    if (pszForceRaster != nullptr && CSLTestBoolean(pszForceRaster))
        pszHeaderMark = "DataMark:CNSDTF-RAS";

    double adfGeoTransform[6];
    poSrcDS->GetGeoTransform(adfGeoTransform);

    sprintf(szHeader,
        "%s\n"
        "Version:1.0\n"
        "Unit:%s\n"
        "Alpha:%.6f\n"
        "Compress:0\n"
        "X0:%.6f\n"
        "Y0:%.6f\n"
        "DX:%.6f\n"
        "DY:%.6f\n"
        "Row:%d\n"
        "Col:%d\n"
        "ValueType:Integer\n",	//Integer
        pszHeaderMark,
        pszMeterUnit,
        0.0,
        adfGeoTransform[0],
        adfGeoTransform[3],
        ABS(adfGeoTransform[1]),
        ABS(adfGeoTransform[5]),
        nYSize, nXSize);

    GDALRasterBand* poBand = poSrcDS->GetRasterBand(1);
    double dfScale, dfMin, dfMax;
    int bSuccess;

    const char* pszUnitType = poBand->GetUnitType();
    if (pszUnitType != nullptr)
        sprintf(szHeader + strlen(szHeader), "ZUnit:%s\n", pszUnitType);

    // Write `HZoom' value to header if it is exists in source dataset
    dfScale = poBand->GetScale(&bSuccess);
    if (!bSuccess)
        dfScale = 1000.0;
    sprintf(szHeader + strlen(szHeader), "HZoom:%d\n", static_cast<int>(dfScale));

    // Write `minmax' value to header if it is exists in source dataset
    dfMin = poBand->GetMinimum(&bSuccess);
    if (bSuccess)
        sprintf(szHeader + strlen(szHeader), "MinV:%.20g\n", dfMin);
    else
        sprintf(szHeader + strlen(szHeader), "MinV:  \n");

    dfMax = poBand->GetMaximum(&bSuccess);
    if (bSuccess)
        sprintf(szHeader + strlen(szHeader), "MaxV:%.20g\n", dfMax);
    else
        sprintf(szHeader + strlen(szHeader), "MaxV:  \n");

    VSIFWriteL(szHeader, 1, strlen(szHeader), fpImage);

    /* -------------------------------------------------------------------- */
    /*      writeing image data.                                            */
    /* -------------------------------------------------------------------- */
    CPLErr eErr = WriteImageData(poBand, nXSize, nYSize, dfScale, fpImage,
        papszOptions, pfnProgress, pProgressData);

    VSIFCloseL(fpImage);

    if (eErr != CE_None)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Re-open dataset, and copy any auxilary pam information.         */
    /* -------------------------------------------------------------------- */

    /* If outputing to stdout, we can't reopen it, so we'll return */
    /* a fake dataset to make the caller happy */
    CPLPushErrorHandler(CPLQuietErrorHandler);
    GDALPamDataset* poDS = (GDALPamDataset*)GDALOpen(pszFilename, GA_ReadOnly);
    CPLPopErrorHandler();
    if (poDS)
    {
        poDS->CloneInfo(poSrcDS, GCIF_PAM_DEFAULT);
        return poDS;
    }

    CPLErrorReset();

    CNSDTFDataset* poTmpDS = new CNSDTFDataset();
    poTmpDS->nRasterXSize = nXSize;
    poTmpDS->nRasterYSize = nYSize;
    poTmpDS->nBands = 1;
    poTmpDS->SetBand(1, new AscRasterBand(poTmpDS, 1));
    return poTmpDS;
}

/************************************************************************/
/*                       GDALRegister_CNSDTF()                          */
/************************************************************************/

void GDALRegister_CNSDTF()
{
    if (GDALGetDriverByName("CNSDTF") != nullptr)
        return;

    GDALDriver* poDriver = new GDALDriver();

    poDriver->SetDescription("CNSDTF");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "China Geospatial Data Transfer Forma");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_cnsdtf.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "grd,vct");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte UInt16 Int16 Int32");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>\n"
        "   <Option name='FORCE_RASTER' type='boolean' description='Force use of RASTER, default is FALSE(DEM).'/>\n"
        "   <Option name='DECIMAL_PRECISION' type='int' description='Number of decimal when writing floating-point numbers.'/>\n"
        "</CreationOptionList>\n");

    poDriver->pfnIdentify = CNSDTFDataset::Identify;
    poDriver->pfnOpen = CNSDTFDataset::Open;
    poDriver->pfnCreateCopy = CNSDTFDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
