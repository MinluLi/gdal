/******************************************************************************
*
* Project:  GDAL
* Purpose:  Implements ImageInfo Grid Format.
* Author:   Minlu Li, liminlu0314@gmail.com
* Describe: Chinese Spatial Data Transfer format. Reference:GB/T 17798-2007
*
******************************************************************************
* Copyright (c) 2013, Minlu Li (liminlu0314@gmail.com)
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
#include "gdal_pam.h"

/************************************************************************/
/* ==================================================================== */
/*                              ImageInfoDataset                           */
/* ==================================================================== */
/************************************************************************/

class ImageInfoRasterBand;

class CPL_DLL ImageInfoDataset : public GDALPamDataset
{
    friend class ImageInfoRasterBand;

    VSILFILE* fp;

    char** papszPrj;
    CPLString   osPrjFilename;
    char* pszProjection;

    // DataMark Version Compress Alpha
    CPLString	osDataMark;
    CPLString	osDataVersion;
    GBool       bIsCompress;
    double      adfAlphaValue;
    int			nHZoom;
    CPLString   osUnitType;

    unsigned char achReadBuf[256];
    GUIntBig    nBufferOffset;
    int         nOffsetInBuffer;

    char        Getc();
    GUIntBig    Tell();
    int         Seek(GUIntBig nOffset);

protected:
    GDALDataType eDataType;
    double      adfGeoTransform[6];
    int         bNoDataSet;
    double      dfNoDataValue;

    int ParseHeader(const char* pszHeader, int& nStartOfData);

public:
    ImageInfoDataset();
    ~ImageInfoDataset();

    virtual char** GetFileList(void);

    static GDALDataset* Open(GDALOpenInfo* poOpenInfo);
    static int          Identify(GDALOpenInfo* poOpenInfo);
    static GDALDataset* CreateCopy(const char* pszFilename,
        GDALDataset* poSrcDS, int bStrict, char** papszOptions,
        GDALProgressFunc pfnProgress, void* pProgressData);

    virtual CPLErr GetGeoTransform(double* padfTransform);
    virtual const char* GetProjectionRef(void);
};

/************************************************************************/
/* ==================================================================== */
/*                            ImageInfoRasterBand                          */
/* ==================================================================== */
/************************************************************************/

class ImageInfoRasterBand : public GDALPamRasterBand
{
    friend class ImageInfoDataset;

    GUIntBig* panLineOffset;
    char* pszUnitType;
    double        dfScale;

public:

    ImageInfoRasterBand(ImageInfoDataset* poDS, int nDataStart);
    virtual       ~ImageInfoRasterBand();

    virtual double GetNoDataValue(int* pbSuccess);
    virtual CPLErr SetNoDataValue(double dfNoData);
    virtual const char* GetUnitType();
    CPLErr SetUnitType(const char* pszNewValue);
    virtual double GetScale(int* pbSuccess = nullptr);
    CPLErr SetScale(double dfNewScale);

    virtual CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void* pImage);
};

/************************************************************************/
/*                           ImageInfoRasterBand()                         */
/************************************************************************/

ImageInfoRasterBand::ImageInfoRasterBand(ImageInfoDataset* poDS, int nDataStart)

{
    this->poDS = poDS;

    nBand = 1;
    eDataType = poDS->eDataType;

    nBlockXSize = poDS->nRasterXSize;
    nBlockYSize = 1;

    panLineOffset =
        (GUIntBig*)VSICalloc(poDS->nRasterYSize, sizeof(GUIntBig));
    if (panLineOffset == nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
            "ImageInfoRasterBand::ImageInfoRasterBand : Out of memory (nRasterYSize = %d)",
            poDS->nRasterYSize);
        return;
    }

    panLineOffset[0] = nDataStart;
    dfScale = poDS->nHZoom;
    pszUnitType = CPLStrdup(poDS->osUnitType.c_str());
}

/************************************************************************/
/*                          ~ImageInfoRasterBand()                         */
/************************************************************************/

ImageInfoRasterBand::~ImageInfoRasterBand()

{
    CPLFree(panLineOffset);
    CPLFree(pszUnitType);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ImageInfoRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
    void* pImage)

{
    ImageInfoDataset* poODS = (ImageInfoDataset*)poDS;
    int         iPixel;

    if (nBlockYOff < 0 || nBlockYOff > poODS->nRasterYSize - 1
        || nBlockXOff != 0 || panLineOffset == nullptr || poODS->fp == nullptr)
        return CE_Failure;

    if (panLineOffset[nBlockYOff] == 0)
    {
        int iPrevLine;

        for (iPrevLine = 1; iPrevLine <= nBlockYOff; iPrevLine++)
            if (panLineOffset[iPrevLine] == 0)
                IReadBlock(nBlockXOff, iPrevLine - 1, nullptr);
    }

    if (panLineOffset[nBlockYOff] == 0)
        return CE_Failure;


    if (poODS->Seek(panLineOffset[nBlockYOff]) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO,
            "Can't seek to offset %lu in input file to read data.",
            (long unsigned int)panLineOffset[nBlockYOff]);
        return CE_Failure;
    }

    for (iPixel = 0; iPixel < poODS->nRasterXSize; )
    {
        char szToken[500];
        char chNext;
        int  iTokenChar = 0;

        /* suck up any pre-white space. */
        do {
            chNext = poODS->Getc();
        } while (isspace((unsigned char)chNext));

        while (chNext != '\0' && !isspace((unsigned char)chNext))
        {
            if (iTokenChar == sizeof(szToken) - 2)
            {
                CPLError(CE_Failure, CPLE_FileIO,
                    "Token too long at scanline %d.",
                    nBlockYOff);
                return CE_Failure;
            }

            szToken[iTokenChar++] = chNext;
            chNext = poODS->Getc();
        }

        if (chNext == '\0' &&
            (iPixel != poODS->nRasterXSize - 1 ||
                nBlockYOff != poODS->nRasterYSize - 1))
        {
            CPLError(CE_Failure, CPLE_FileIO,
                "File short, can't read line %d.",
                nBlockYOff);
            return CE_Failure;
        }

        szToken[iTokenChar] = '\0';

        if (pImage != nullptr)
        {
            if (eDataType == GDT_Float64)
                ((double*)pImage)[iPixel] = CPLAtofM(szToken);
            else if (eDataType == GDT_Float32)
                ((float*)pImage)[iPixel] = (float)CPLAtofM(szToken);
            else
                ((GInt32*)pImage)[iPixel] = (GInt32)atoi(szToken);
        }

        iPixel++;
    }

    if (nBlockYOff < poODS->nRasterYSize - 1)
        panLineOffset[nBlockYOff + 1] = poODS->Tell();

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double ImageInfoRasterBand::GetNoDataValue(int* pbSuccess)

{
    ImageInfoDataset* poODS = (ImageInfoDataset*)poDS;

    if (pbSuccess)
        *pbSuccess = poODS->bNoDataSet;

    return poODS->dfNoDataValue;
}


/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr ImageInfoRasterBand::SetNoDataValue(double dfNoData)

{
    ImageInfoDataset* poODS = (ImageInfoDataset*)poDS;

    poODS->bNoDataSet = TRUE;
    poODS->dfNoDataValue = dfNoData;

    return CE_None;
}

/************************************************************************/
/*                            GetUnitType()                             */
/************************************************************************/

const char* ImageInfoRasterBand::GetUnitType()

{
    if (pszUnitType == nullptr)
        return "";
    else
        return pszUnitType;
}

/************************************************************************/
/*                            SetUnitType()                             */
/************************************************************************/

CPLErr ImageInfoRasterBand::SetUnitType(const char* pszNewValue)

{
    CPLFree(pszUnitType);

    if (pszNewValue == nullptr)
        pszUnitType = nullptr;
    else
        pszUnitType = CPLStrdup(pszNewValue);

    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double ImageInfoRasterBand::GetScale(int* pbSuccess)

{
    if (pbSuccess != nullptr)
        *pbSuccess = TRUE;

    return dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr ImageInfoRasterBand::SetScale(double dfNewScale)

{
    dfScale = dfNewScale;
    return CE_None;
}

/************************************************************************/
/*                            ImageInfoDataset()                           */
/************************************************************************/

ImageInfoDataset::ImageInfoDataset()
{
    papszPrj = nullptr;
    pszProjection = CPLStrdup("");
    fp = nullptr;
    eDataType = GDT_Int32;
    bNoDataSet = TRUE;
    dfNoDataValue = -99999.0;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    nOffsetInBuffer = 256;
    nBufferOffset = 0;

    osDataMark = "NSDTF-DEM";
    osDataVersion = "1.0";
    bIsCompress = FALSE;
    adfAlphaValue = 0.0;
    nHZoom = 1;
}

/************************************************************************/
/*                           ~ImageInfoDataset()                           */
/************************************************************************/

ImageInfoDataset::~ImageInfoDataset()

{
    FlushCache();

    if (fp != nullptr)
        VSIFCloseL(fp);

    CPLFree(pszProjection);
    CSLDestroy(papszPrj);
}

/************************************************************************/
/*                                Tell()                                */
/************************************************************************/

GUIntBig ImageInfoDataset::Tell()

{
    return nBufferOffset + nOffsetInBuffer;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int ImageInfoDataset::Seek(GUIntBig nNewOffset)

{
    nOffsetInBuffer = sizeof(achReadBuf);
    return VSIFSeekL(fp, nNewOffset, SEEK_SET);
}

/************************************************************************/
/*                                Getc()                                */
/*                                                                      */
/*      Read a single character from the input file (efficiently we     */
/*      hope).                                                          */
/************************************************************************/

char ImageInfoDataset::Getc()

{
    if (nOffsetInBuffer < (int)sizeof(achReadBuf))
        return achReadBuf[nOffsetInBuffer++];

    nBufferOffset = VSIFTellL(fp);
    auto nRead = VSIFReadL(achReadBuf, 1, sizeof(achReadBuf), fp);

    for (size_t i = nRead; i < sizeof(achReadBuf); i++)
        achReadBuf[i] = '\0';

    nOffsetInBuffer = 0;

    return achReadBuf[nOffsetInBuffer++];
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** ImageInfoDataset::GetFileList()

{
    char** papszFileList = GDALPamDataset::GetFileList();

    if (papszPrj != nullptr)
        papszFileList = CSLAddString(papszFileList, osPrjFilename);

    return papszFileList;
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
    ImageInfoRasterBand* band = new ImageInfoRasterBand(poDS, nStartOfData);
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
/*                          ParseHeader()                               */
/************************************************************************/

int ImageInfoDataset::ParseHeader(const char* pszHeader, int& nStartOfData)
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
    adfAlphaValue = CPLAtofM(papszTokens[3]);
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

    nHZoom = atoi(papszTokens[11]);
    eDataType = GDT_Int32;
    bNoDataSet = TRUE;
    dfNoDataValue = CPLAtofM(papszTokens[12]);

    int nPos = FindString(pszHeader, papszTokens[12]);
    nStartOfData = nPos + static_cast<int>(strlen(papszTokens[12]));

    while (pszHeader[nStartOfData] == ' ' || pszHeader[nStartOfData] == '\n' || pszHeader[nStartOfData] == '\r')
    {
        nStartOfData++;
    }

    size_t nLine13Len = strlen(papszTokens[13]);
    if (!EQUALN(pszHeader + nStartOfData, papszTokens[13], nLine13Len))
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    char** papsz13Tokens = CSLTokenizeString2(papszTokens[13], " ", 0);
    int n13Tokens = CSLCount(papsz13Tokens);
    CSLDestroy(papsz13Tokens);
    if (n13Tokens != 10)
    {
        CSLDestroy(papszTokens);
        return FALSE;
    }

    CSLDestroy(papszTokens);
    return TRUE;
}


/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ImageInfoDataset::GetGeoTransform(double* padfTransform)

{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
    return(CE_None);
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char* ImageInfoDataset::GetProjectionRef()

{
    return pszProjection;
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

    // Write `nodata' value to header if it is exists in source dataset
    double dfNoData = poBand->GetNoDataValue(&bSuccess);
    if (bSuccess)
        sprintf(szHeader + strlen(szHeader), "%6.20g\n", dfNoData);
    else
        sprintf(szHeader + strlen(szHeader), "-9999000\n");

    VSIFWriteL(szHeader, 1, strlen(szHeader), fpImage);

    /* -------------------------------------------------------------------- */
    /*     Builds the format string used for printing float values.         */
    /* -------------------------------------------------------------------- */
    char szFormatFloat[32];
    strcpy(szFormatFloat, " %.20g");
    const char* pszDecimalPrecision = CSLFetchNameValue(papszOptions, "DECIMAL_PRECISION");
    if (pszDecimalPrecision)
    {
        int nDecimal = atoi(pszDecimalPrecision);
        if (nDecimal >= 0)
            sprintf(szFormatFloat, " %%.%dg", nDecimal);
    }

    /* -------------------------------------------------------------------- */
    /*      Loop over image, copying image data.                            */
    /* -------------------------------------------------------------------- */
    int* panScanline = nullptr;
    double* padfScanline = nullptr;
    int         bReadAsInt;
    int         iLine, iPixel;
    CPLErr      eErr = CE_None;

    bReadAsInt = (poBand->GetRasterDataType() == GDT_Byte
        || poBand->GetRasterDataType() == GDT_Int16
        || poBand->GetRasterDataType() == GDT_UInt16
        || poBand->GetRasterDataType() == GDT_Int32);

    // Write scanlines to output file
    if (bReadAsInt)
        panScanline = (int*)CPLMalloc(nXSize * GDALGetDataTypeSize(GDT_Int32) / 8);
    else
        padfScanline = (double*)CPLMalloc(nXSize * GDALGetDataTypeSize(GDT_Float64) / 8);

    for (iLine = 0; eErr == CE_None && iLine < nYSize; iLine++)
    {
        CPLString osBuf;
        eErr = poBand->RasterIO(GF_Read, 0, iLine, nXSize, 1,
            (bReadAsInt) ? (void*)panScanline : (void*)padfScanline,
            nXSize, 1, (bReadAsInt) ? GDT_Int32 : GDT_Float64, 0, 0, nullptr);

        if (bReadAsInt)
        {
            for (iPixel = 0; iPixel < nXSize; iPixel++)
            {
                sprintf(szHeader, "%d ", panScanline[iPixel]);
                if (iPixel % 10 == 9)
                    sprintf(szHeader + strlen(szHeader), "\n");

                osBuf += szHeader;
                if ((iPixel & 1023) == 0 || iPixel == nXSize - 1)
                {
                    if (VSIFWriteL(osBuf, (int)osBuf.size(), 1, fpImage) != 1)
                    {
                        eErr = CE_Failure;
                        CPLError(CE_Failure, CPLE_AppDefined, "Write failed, disk full?\n");
                        break;
                    }
                    osBuf = "";
                }
            }
        }
        else
        {
            for (iPixel = 0; iPixel < nXSize; iPixel++)
            {
                sprintf(szHeader, "%d ", static_cast<int>(padfScanline[iPixel] * dfScale));
                if (iPixel % 10 == 9)
                    sprintf(szHeader + strlen(szHeader), "\n");

                osBuf += szHeader;
                if ((iPixel & 1023) == 0 || iPixel == nXSize - 1)
                {
                    if (VSIFWriteL(osBuf, (int)osBuf.size(), 1, fpImage) != 1)
                    {
                        eErr = CE_Failure;
                        CPLError(CE_Failure, CPLE_AppDefined, "Write failed, disk full?\n");
                        break;
                    }
                    osBuf = "";
                }
            }
        }
        VSIFWriteL((void*)"\n", 1, 1, fpImage);

        if (eErr == CE_None &&
            !pfnProgress((iLine + 1) / ((double)nYSize), nullptr, pProgressData))
        {
            eErr = CE_Failure;
            CPLError(CE_Failure, CPLE_UserInterrupt, "User terminated CreateCopy()");
        }
    }

    CPLFree(panScanline);
    CPLFree(padfScanline);
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

    ImageInfoDataset* poNsdtfDS = new ImageInfoDataset();
    poNsdtfDS->nRasterXSize = nXSize;
    poNsdtfDS->nRasterYSize = nYSize;
    poNsdtfDS->nBands = 1;
    poNsdtfDS->SetBand(1, new ImageInfoRasterBand(poNsdtfDS, 1));
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

    poDriver->pfnIdentify = ImageInfoDataset::Identify;
    poDriver->pfnOpen = ImageInfoDataset::Open;
    poDriver->pfnCreateCopy = ImageInfoDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
