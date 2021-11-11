/******************************************************************************
*
* Project:  GDAL
* Purpose:  Implements ImageInfo Grid Format.
* Author:   Minlu Li, liminlu0314@163.com
* Describe: ImageInfo Grid format.
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
/*                            ImageInfoDataset                          */
/* ==================================================================== */
/************************************************************************/

class CPL_DLL ImageInfoDataset : public AscDataset
{
    friend class AscRasterBand;

protected:
    virtual int ParseHeader(const char* pszHeader, int& nStartOfData);

public:
    ImageInfoDataset();
    ~ImageInfoDataset();

    static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
    static int          Identify(GDALOpenInfo* poOpenInfo);
    static GDALDataset* CreateCopy(const char* pszFilename,
        GDALDataset* poSrcDS, int bStrict, char** papszOptions,
        GDALProgressFunc pfnProgress, void* pProgressData);
};

/************************************************************************/
/*                           ImageInfoDataset()                         */
/************************************************************************/

ImageInfoDataset::ImageInfoDataset()
    :AscDataset()
{
}

/************************************************************************/
/*                           ~ImageInfoDataset()                           */
/************************************************************************/

ImageInfoDataset::~ImageInfoDataset()
{
}

/************************************************************************/
/*                            Identify()                                */
/************************************************************************/

int ImageInfoDataset::Identify(GDALOpenInfo* poOpenInfo)
{
    /* -------------------------------------------------------------------- */
    /*      Does this look like an NSDTF grid file?                         */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 40
        || !(EQUALN((const char*)poOpenInfo->pabyHeader, "UTM_X0", 6)))
        return FALSE;

    char** papszTokens = CSLTokenizeString2((const char*)poOpenInfo->pabyHeader, " \n\r", 0);
    int nTokens = CSLCount(papszTokens);
    if (nTokens < 14)	//the header at (the) least 14 lines
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    if (!(EQUALN(papszTokens[0], "UTM_X0", 6)))
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

int ImageInfoDataset::ParseHeader(const char* pszHeader, int& nStartOfData)
{
    char** papszTokens = CSLTokenizeString2(pszHeader, "\n\r", 0);

    nRasterYSize = atoi(ParseKeyValue(papszTokens, "DEM_ROW").c_str());
    nRasterXSize = atoi(ParseKeyValue(papszTokens, "DEM_COL").c_str());

    if (!GDALCheckDatasetDimensions(nRasterXSize, nRasterYSize))
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    double dfX0 = CPLAtofM(ParseKeyValue(papszTokens, "UTM_X0").c_str());
    double dfY0 = CPLAtofM(ParseKeyValue(papszTokens, "UTM_Y0").c_str());

    double dfCellDX = CPLAtofM(ParseKeyValue(papszTokens, "DEM_DX").c_str());
    double dfCellDY = CPLAtofM(ParseKeyValue(papszTokens, "DEM_DY").c_str());

    adfGeoTransform[0] = dfX0;
    adfGeoTransform[1] = dfCellDX;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = dfY0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = -dfCellDY;

    eDataType = GDT_Float32;
    bHaveNoDataValue = TRUE;
    dfNoDataValue = -9999.0;

    int nPos = FindString(pszHeader, papszTokens[5]);
    nStartOfData = nPos + static_cast<int>(strlen(papszTokens[5]));

    while (pszHeader[nStartOfData] == ' ' || pszHeader[nStartOfData] == '\n' || pszHeader[nStartOfData] == '\r')
    {
        nStartOfData++;
    }

    size_t nLine13Len = strlen(papszTokens[6]);
    if (!EQUALN(pszHeader + nStartOfData, papszTokens[6], nLine13Len))
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
GDALDataset* ImageInfoDataset::Open(GDALOpenInfo* poOpenInfo)
{
    if (!Identify(poOpenInfo))
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    ImageInfoDataset* poDS;
    poDS = new ImageInfoDataset();

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
GDALDataset* ImageInfoDataset::CreateCopy(
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
    /*      Write ImageInfo Grid file header                                    */
    /* -------------------------------------------------------------------- */

    double adfGeoTransform[6];
    poSrcDS->GetGeoTransform(adfGeoTransform);

    GDALRasterBand* poBand = poSrcDS->GetRasterBand(1);

    char szHeader[400];
    sprintf(szHeader,
        "UTM_X0: %.15f\n"
        "UTM_Y0: %.15f\n"
        "DEM_DX: %.15f\n"
        "DEM_DY: %.15f\n"
        "DEM_COL: %d\n"
        "DEM_ROW: %d\n",
        adfGeoTransform[0],
        adfGeoTransform[3],
        ABS(adfGeoTransform[1]),
        ABS(adfGeoTransform[5]),
        nXSize, nYSize);

    VSIFWriteL(szHeader, 1, strlen(szHeader), fpImage);

    /* -------------------------------------------------------------------- */
    /*      writeing image data.                                            */
    /* -------------------------------------------------------------------- */
    char** papszNewOptions = CSLDuplicate(papszOptions);
    papszNewOptions = CSLAddString(papszNewOptions, "DECIMAL_PRECISION=2");
    CPLErr eErr = WriteImageData(poBand, nXSize, nYSize, 1.0, fpImage,
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

    ImageInfoDataset* poTmpDS = new ImageInfoDataset();
    poTmpDS->nRasterXSize = nXSize;
    poTmpDS->nRasterYSize = nYSize;
    poTmpDS->nBands = 1;
    poTmpDS->SetBand(1, new AscRasterBand(poTmpDS, 1));
    return poTmpDS;
}

/************************************************************************/
/*                    GDALRegister_ImageInfo()                          */
/************************************************************************/

void GDALRegister_ImageInfo()
{
    if (GDALGetDriverByName("ImageInfo") != nullptr)
        return;

    GDALDriver* poDriver = new GDALDriver();

    poDriver->SetDescription("ImageInfo");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "ImageInfo Data Transfer Format");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "frmt_imageinfo.html");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "dtm");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Float32");

    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>\n"
        "   <Option name='DECIMAL_PRECISION' type='int' description='Number of decimal when writing floating-point numbers.'/>\n"
        "</CreationOptionList>\n");

    poDriver->pfnIdentify = ImageInfoDataset::Identify;
    poDriver->pfnOpen = ImageInfoDataset::Open;
    poDriver->pfnCreateCopy = ImageInfoDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
