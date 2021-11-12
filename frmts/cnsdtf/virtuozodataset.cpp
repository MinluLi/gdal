/******************************************************************************
*
* Project:  GDAL
* Purpose:  Implements VirtuoZo Grid Format.
* Author:   Minlu Li, liminlu0314@163.com
* Describe: VirtuoZo Grid format.
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
/*                            VirtuoZoDataset                           */
/* ==================================================================== */
/************************************************************************/

class CPL_DLL VirtuoZoDataset : public AscDataset
{
    friend class AscRasterBand;

protected:
    virtual int ParseHeader(const char* pszHeader, int& nStartOfData);

public:
    VirtuoZoDataset();
    ~VirtuoZoDataset();

    static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
    static int          Identify(GDALOpenInfo* poOpenInfo);
    static GDALDataset* CreateCopy(const char* pszFilename,
        GDALDataset* poSrcDS, int bStrict, char** papszOptions,
        GDALProgressFunc pfnProgress, void* pProgressData);
};

/************************************************************************/
/*                           VirtuoZoDataset()                          */
/************************************************************************/

VirtuoZoDataset::VirtuoZoDataset()
    :AscDataset()
{
}

/************************************************************************/
/*                          ~VirtuoZoDataset()                          */
/************************************************************************/

VirtuoZoDataset::~VirtuoZoDataset()
{
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int VirtuoZoDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    /* -------------------------------------------------------------------- */
    /*      Does this look like an VirtuoZo grid file?                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 80)
        return FALSE;

    char** papszTokens = CSLTokenizeString2((const char*)poOpenInfo->pabyHeader, "\n\r", 0);
    int nTokens = CSLCount(papszTokens);
    if (nTokens < 2)	//the header at (the) least 2 lines
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    char** papszFirstLine = CSLTokenizeString2(papszTokens[0], " \t", 0);
    if (CSLCount(papszFirstLine) != 7)	//the first line must be 7 elements
    {
        CSLDestroy(papszTokens);
        CSLDestroy(papszFirstLine);
        return FALSE;
    }

    CSLDestroy(papszTokens);
    CSLDestroy(papszFirstLine);
    return TRUE;
}

/************************************************************************/
/*                          ParseHeader()                               */
/************************************************************************/

int VirtuoZoDataset::ParseHeader(const char* pszHeader, int& nStartOfData)
{
    char** papszTokens = CSLTokenizeString2(pszHeader, "\n\r", 0);
    int nTokens = CSLCount(papszTokens);
    if (nTokens < 2)
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    char** papszFirstLine = CSLTokenizeString2(papszTokens[0], " \t", 0);
    if (CSLCount(papszFirstLine) != 7)	//the first line must be 7 elements
    {
        CSLDestroy(papszTokens);
        CSLDestroy(papszFirstLine);
        return FALSE;
    }

    nRasterXSize = atoi(papszFirstLine[5]);
    nRasterYSize = atoi(papszFirstLine[6]);

    if (!GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize))
    {
        CSLDestroy(papszTokens);
        CSLDestroy(papszFirstLine);
        return FALSE;
    }

    // llcenter
    double dfX0 = CPLAtofM(papszFirstLine[0]);
    double dfY0 = CPLAtofM(papszFirstLine[1]);

    double dfCellDX = CPLAtofM(papszFirstLine[3]);
    double dfCellDY = CPLAtofM(papszFirstLine[4]);

    adfGeoTransform[0] = dfX0 - dfCellDX * 0.5; //xllconner to xllcenter
    adfGeoTransform[1] = dfCellDX;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = dfY0 - dfCellDY * 0.5 + nRasterYSize * dfCellDY;  //yllconner to yulcenter
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = -dfCellDY;

    dfScale = atoi(papszFirstLine[2]);
    if (dfScale == 0)
        dfScale = 1.0;

    CSLDestroy(papszFirstLine);

    eDataType = GDT_Float32;
    bHaveNoDataValue = TRUE;
    dfNoDataValue = -9999.0;

    int nPos = FindString(pszHeader, papszTokens[0]);
    nStartOfData = nPos + static_cast<int>(strlen(papszTokens[0]));

    while (pszHeader[nStartOfData] == ' ' || pszHeader[nStartOfData] == '\n' || pszHeader[nStartOfData] == '\r')
    {
        nStartOfData++;
    }

    size_t nLine1Len = strlen(papszTokens[1]);
    if (!EQUALN(pszHeader + nStartOfData, papszTokens[1], nLine1Len))
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

GDALDataset* VirtuoZoDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    VirtuoZoDataset* poDS;
    poDS = new VirtuoZoDataset();

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
    AscRasterBand* band = new AscRasterBand(poDS, nStartOfData, true);
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

GDALDataset* VirtuoZoDataset::CreateCopy(
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
            "VirtuoZo Grid driver doesn't support %d bands. Must be 1 band.\n",
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
    /*      Write VirtuoZo Grid file header                                 */
    /* -------------------------------------------------------------------- */

    double adfGeoTransform[6];
    poSrcDS->GetGeoTransform(adfGeoTransform);

    // Write `HZoom' value to header if it is exists in source dataset
    GDALRasterBand* poBand = poSrcDS->GetRasterBand(1);
    int bSuccess;

    double dfScale = poBand->GetScale(&bSuccess);
    if (!bSuccess)
        dfScale = 0.0;

    char szHeader[2000];
    sprintf(szHeader,
        "%.6f %.6f %.3f %.3f %.3f %d %d\n",
        adfGeoTransform[0] + adfGeoTransform[1] * 0.5,  //xulconner to xllcenter
        adfGeoTransform[3] + adfGeoTransform[5] * (nYSize - 0.5),  //yulconner to yllcenter
        dfScale,
        ABS(adfGeoTransform[1]),
        ABS(adfGeoTransform[5]),
        nXSize,
        nYSize);

    VSIFWriteL(szHeader, 1, strlen(szHeader), fpImage);

    /* -------------------------------------------------------------------- */
    /*      writeing image data.                                            */
    /* -------------------------------------------------------------------- */
    char** papszNewOptions = CSLDuplicate(papszOptions);
    papszNewOptions = CSLAddString(papszNewOptions, "DECIMAL_PRECISION=2");
    papszNewOptions = CSLAddString(papszNewOptions, "INVERTLINE=TRUE");
    CPLErr eErr = WriteImageData(poBand, nXSize, nYSize, dfScale, fpImage,
        papszNewOptions, pfnProgress, pProgressData);

    CSLDestroy(papszNewOptions);
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

    VirtuoZoDataset* poTmpDS = new VirtuoZoDataset();
    poTmpDS->nRasterXSize = nXSize;
    poTmpDS->nRasterYSize = nYSize;
    poTmpDS->nBands = 1;
    poTmpDS->SetBand(1, new AscRasterBand(poTmpDS, 1, true));
    return poTmpDS;
}

/************************************************************************/
/*                      GDALRegister_VirtuoZo()                         */
/************************************************************************/

void GDALRegister_VirtuoZo()
{
    if (GDALGetDriverByName("VirtuoZo") != nullptr)
        return;

    GDALDriver* poDriver = new GDALDriver();

    poDriver->SetDescription("VirtuoZo");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "VirtuoZo DEM Format");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_nsdtf.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "dem");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Float32");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>\n"
        "   <Option name='DECIMAL_PRECISION' type='int' description='Number of decimal when writing floating-point numbers.'/>\n"
        "</CreationOptionList>\n");

    poDriver->pfnIdentify = VirtuoZoDataset::Identify;
    poDriver->pfnOpen = VirtuoZoDataset::Open;
    poDriver->pfnCreateCopy = VirtuoZoDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
