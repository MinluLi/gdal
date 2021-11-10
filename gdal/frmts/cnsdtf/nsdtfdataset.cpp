/******************************************************************************
*
* Project:  GDAL
* Purpose:  Implements NSDTF Grid Format.
* Author:   Minlu Li, liminlu0314@163.com
* Describe: NSDTF Grid format. Reference:GB/T 17798-2007
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
/*                              NSDTFDataset                           */
/* ==================================================================== */
/************************************************************************/

class CPL_DLL NSDTFDataset : public AscDataset
{
    friend class AscRasterBand;

    // DataMark Version Compress Alpha
    CPLString	osDataMark;
    CPLString	osDataVersion;
    GBool       bIsCompress;
    double      dfAlphaValue;
    CPLString   osUnitType;

protected:
    virtual int ParseHeader(const char* pszHeader, int& nStartOfData);

public:
    NSDTFDataset();
    ~NSDTFDataset();

    static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
    static int          Identify(GDALOpenInfo* poOpenInfo);
    static GDALDataset* CreateCopy(const char* pszFilename,
        GDALDataset* poSrcDS, int bStrict, char** papszOptions,
        GDALProgressFunc pfnProgress, void* pProgressData);
};

/************************************************************************/
/*                            NSDTFDataset()                           */
/************************************************************************/

NSDTFDataset::NSDTFDataset()
    :AscDataset()
{
    osDataMark = "NSDTF-DEM";
    osDataVersion = "1.0";
    bIsCompress = FALSE;
    dfAlphaValue = 0.0;
}

/************************************************************************/
/*                           ~NSDTFDataset()                           */
/************************************************************************/

NSDTFDataset::~NSDTFDataset()
{
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int NSDTFDataset::Identify(GDALOpenInfo* poOpenInfo)

{
    /* -------------------------------------------------------------------- */
    /*      Does this look like an NSDTF grid file?                         */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 40
        || !(EQUALN((const char*)poOpenInfo->pabyHeader, "NSDTF-DEM", 9)))
        return FALSE;

    char** papszTokens = CSLTokenizeString2((const char*)poOpenInfo->pabyHeader, " \n\r\t", 0);
    int nTokens = CSLCount(papszTokens);
    if (nTokens < 13)	//the header at (the) least 13 lines
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    if (poOpenInfo->nHeaderBytes < 40 ||
        !(EQUALN(papszTokens[0], "NSDTF-DEM", 9))
        )
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    CSLDestroy(papszTokens);
    return TRUE;
}

/************************************************************************/
/*                          ParseHeader()                               */
/************************************************************************/

int NSDTFDataset::ParseHeader(const char* pszHeader, int& nStartOfData)
{
    char** papszTokens = CSLTokenizeString2(pszHeader, "\n\r", 0);
    int nTokens = CSLCount(papszTokens);
    if (nTokens < 14)
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    if (!EQUAL(papszTokens[0], "NSDTF-DEM"))
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    if (!EQUAL(papszTokens[2], "M") && !EQUAL(papszTokens[2], "D") && !EQUAL(papszTokens[2], "K"))
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    osDataMark = "NSDTF-DEM";
    osDataVersion = papszTokens[1];
    osUnitType = papszTokens[2];
    dfAlphaValue = CPLAtofM(papszTokens[3]);
    bIsCompress = atoi(papszTokens[4]);

    adfGeoTransform[0] = CPLAtofM(papszTokens[5]);
    adfGeoTransform[1] = CPLAtofM(papszTokens[7]);
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = CPLAtofM(papszTokens[6]);
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = -CPLAtofM(papszTokens[8]);

    nRasterYSize = atoi(papszTokens[9]);
    nRasterXSize = atoi(papszTokens[10]);

    if (!GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize))
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    dfScale = atoi(papszTokens[11]);
    eDataType = GDT_Int32;
    bHaveNoDataValue = TRUE;
    dfNoDataValue = -9999.0;

    int nPos = FindString(pszHeader, papszTokens[11]);
    nStartOfData = nPos + static_cast<int>(strlen(papszTokens[11]));

    while (pszHeader[nStartOfData] == ' ' || pszHeader[nStartOfData] == '\n' || pszHeader[nStartOfData] == '\r')
    {
        nStartOfData++;
    }

    size_t nLine13Len = strlen(papszTokens[12]);
    if (!EQUALN(pszHeader + nStartOfData, papszTokens[12], nLine13Len))
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    CSLDestroy(papszTokens);
    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset* NSDTFDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    NSDTFDataset* poDS;
    poDS = new NSDTFDataset();

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

GDALDataset* NSDTFDataset::CreateCopy(
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
            "NSDTF Grid driver doesn't support %d bands. Must be 1 band.\n",
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
    /*      Write NSDTF Grid file header                                    */
    /* -------------------------------------------------------------------- */

    double adfGeoTransform[6];
    poSrcDS->GetGeoTransform(adfGeoTransform);

    // Write `HZoom' value to header if it is exists in source dataset
    GDALRasterBand* poBand = poSrcDS->GetRasterBand(1);
    int bSuccess;

    double dfScale = poBand->GetScale(&bSuccess);
    if (!bSuccess)
        dfScale = 1000.0;

    char szHeader[2000];
    sprintf(szHeader,
        "NSDTF-DEM\n"
        "1.0\n"
        "%s\n"
        "%.6f\n"
        "0\n"
        "%.6f\n"
        "%.6f\n"
        "%.6f\n"
        "%.6f\n"
        "%d\n"
        "%d\n"
        "%d\n",	//Scale
        pszMeterUnit,
        0.0, //alpha
        adfGeoTransform[0],
        adfGeoTransform[3],
        ABS(adfGeoTransform[1]),
        ABS(adfGeoTransform[5]),
        nYSize, nXSize, static_cast<int>(dfScale));

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

    NSDTFDataset* poNsdtfDS = new NSDTFDataset();
    poNsdtfDS->nRasterXSize = nXSize;
    poNsdtfDS->nRasterYSize = nYSize;
    poNsdtfDS->nBands = 1;
    poNsdtfDS->SetBand(1, new AscRasterBand(poNsdtfDS, 1));
    return poNsdtfDS;
}

/************************************************************************/
/*                       GDALRegister_NSDTF()                           */
/************************************************************************/

void GDALRegister_NSDTF()
{
    if (GDALGetDriverByName("NSDTF") != nullptr)
        return;

    GDALDriver* poDriver = new GDALDriver();

    poDriver->SetDescription("NSDTF");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "China Geospatial Data Transfer Format");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_nsdtf.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "dem");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte UInt16 Int16 Int32");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>\n"
        "   <Option name='FORCE_RASTER' type='boolean' description='Force use of RASTER, default is FALSE(DEM).'/>\n"
        "   <Option name='DECIMAL_PRECISION' type='int' description='Number of decimal when writing floating-point numbers.'/>\n"
        "</CreationOptionList>\n");

    poDriver->pfnIdentify = NSDTFDataset::Identify;
    poDriver->pfnOpen = NSDTFDataset::Open;
    poDriver->pfnCreateCopy = NSDTFDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
