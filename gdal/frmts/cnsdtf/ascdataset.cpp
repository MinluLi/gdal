/******************************************************************************
*
* Project:  GDAL
* Purpose:  Implements CNSDTF\NSDTF\ImageInfo Grid Format.
* Author:   Minlu Li, liminlu0314@163.com
* Describe: CNSDTF\NSDTF\ImageInfo Grid format. Reference:GB/T 17798-2007
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

#include "ascdataset.h"

CPLErr WriteImageData(GDALRasterBand* poBand, int nXSize, int nYSize,
    double dfScale, VSILFILE* fpImage, char** papszOptions,
    GDALProgressFunc pfnProgress, void* pProgressData)
{
    /* -------------------------------------------------------------------- */
    /*     Builds the format string used for printing float values.         */
    /* -------------------------------------------------------------------- */
    char szFormatFloat[32];
    strcpy(szFormatFloat, "%.6f ");
    const char* pszDecimalPrecision = CSLFetchNameValue(papszOptions, "DECIMAL_PRECISION");
    if (pszDecimalPrecision)
    {
        int nDecimal = atoi(pszDecimalPrecision);
        if (nDecimal >= 0)
            sprintf(szFormatFloat, "%%.%df ", nDecimal);
    }

    // invert line
    bool bInvertLine = CPLTestBool(CSLFetchNameValueDef(papszOptions, "INVERTLINE", "FALSE"));

    char szHeader[2000];

    /* -------------------------------------------------------------------- */
    /*      Loop over image, copying image data.                            */
    /* -------------------------------------------------------------------- */
    int* panScanline = nullptr;
    double* padfScanline = nullptr;

    CPLErr      eErr = CE_None;

    int bReadAsInt = (poBand->GetRasterDataType() == GDT_Byte
        || poBand->GetRasterDataType() == GDT_Int16
        || poBand->GetRasterDataType() == GDT_UInt16
        || poBand->GetRasterDataType() == GDT_Int32);

    // Write scanlines to output file
    if (bReadAsInt)
        panScanline = (int*)CPLMalloc(nXSize * GDALGetDataTypeSize(GDT_Int32) / 8);
    else
        padfScanline = (double*)CPLMalloc(nXSize * GDALGetDataTypeSize(GDT_Float64) / 8);

    if (bInvertLine)// from down to up
    {
        for (int iLine = nYSize - 1; eErr == CE_None && iLine >= 0; iLine--)
        {
            CPLString osBuf;
            eErr = poBand->RasterIO(GF_Read, 0, iLine, nXSize, 1,
                (bReadAsInt) ? (void*)panScanline : (void*)padfScanline,
                nXSize, 1, (bReadAsInt) ? GDT_Int32 : GDT_Float64, 0, 0, nullptr);

            if (bReadAsInt)
            {
                for (int iPixel = 0; iPixel < nXSize; iPixel++)
                {
                    sprintf(szHeader, "%d ", static_cast<int>(panScanline[iPixel] * dfScale));
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
                for (int iPixel = 0; iPixel < nXSize; iPixel++)
                {
                    sprintf(szHeader, szFormatFloat, padfScanline[iPixel] * dfScale);
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
    }
    else // from up to down
    {
        for (int iLine = 0; eErr == CE_None && iLine < nYSize; iLine++)
        {
            CPLString osBuf;
            eErr = poBand->RasterIO(GF_Read, 0, iLine, nXSize, 1,
                (bReadAsInt) ? (void*)panScanline : (void*)padfScanline,
                nXSize, 1, (bReadAsInt) ? GDT_Int32 : GDT_Float64, 0, 0, nullptr);

            if (bReadAsInt)
            {
                for (int iPixel = 0; iPixel < nXSize; iPixel++)
                {
                    sprintf(szHeader, "%d ", static_cast<int>(panScanline[iPixel] * dfScale));
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
                for (int iPixel = 0; iPixel < nXSize; iPixel++)
                {
                    sprintf(szHeader, szFormatFloat, padfScanline[iPixel] * dfScale);
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
    }

    CPLFree(panScanline);
    CPLFree(padfScanline);

    return eErr;
}

/************************************************************************/
/*                             AscRasterBand()                          */
/************************************************************************/

AscRasterBand::AscRasterBand(AscDataset* poDS, int nDataStart, bool bInvertLine)
{
    this->poDS = poDS;
    this->bInvertLine = bInvertLine;

    nBand = 1;
    eDataType = poDS->eDataType;

    nBlockXSize = poDS->nRasterXSize;
    nBlockYSize = 1;

    panLineOffset =
        (GUIntBig*)VSICalloc(poDS->nRasterYSize, sizeof(GUIntBig));
    if (panLineOffset == nullptr)
    {
        CPLError(CE_Failure, CPLE_OutOfMemory,
            "AscRasterBand::AscRasterBand : Out of memory (nRasterYSize = %d)",
            poDS->nRasterYSize);
        return;
    }

    panLineOffset[0] = nDataStart;

    dfScale = poDS->dfScale;
}

/************************************************************************/
/*                            ~AscRasterBand()                          */
/************************************************************************/

AscRasterBand::~AscRasterBand()
{
    CPLFree(panLineOffset);
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr AscRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff,
    void* pImage)
{
    AscDataset* poODS = (AscDataset*)poDS;
    int         iPixel;

    if (nBlockYOff < 0 || nBlockYOff > poODS->nRasterYSize - 1
        || nBlockXOff != 0 || panLineOffset == nullptr || poODS->fp == nullptr)
        return CE_Failure;

    // TODO 需要根据bInvertLine来改变读取数据内容
    if (bInvertLine)
        nBlockYOff = poODS->nRasterYSize - nBlockYOff;

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

double AscRasterBand::GetNoDataValue(int* pbSuccess)
{
    AscDataset* poODS = (AscDataset*)poDS;

    if (pbSuccess)
        *pbSuccess = poODS->bHaveNoDataValue;

    return poODS->dfNoDataValue;
}


/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr AscRasterBand::SetNoDataValue(double dfNoData)
{
    AscDataset* poODS = (AscDataset*)poDS;

    poODS->bHaveNoDataValue = TRUE;
    poODS->dfNoDataValue = dfNoData;

    return CE_None;
}

/************************************************************************/
/*                              GetScale()                              */
/************************************************************************/

double AscRasterBand::GetScale(int* pbSuccess)
{
    if (pbSuccess != nullptr)
        *pbSuccess = TRUE;

    return dfScale;
}

/************************************************************************/
/*                              SetScale()                              */
/************************************************************************/

CPLErr AscRasterBand::SetScale(double dfNewScale)
{
    dfScale = dfNewScale;
    return CE_None;
}

/************************************************************************/
/*                              AscDataset()                            */
/************************************************************************/

AscDataset::AscDataset()
{
    fp = nullptr;
    papszPrj = nullptr;
    osPrjFilename = "";
    pszProjection = CPLStrdup("");

    dfScale = 1.0;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    eDataType = GDT_Int32;
    bHaveNoDataValue = TRUE;
    dfNoDataValue = -9999.0;

    nOffsetInBuffer = 256;
    nBufferOffset = 0;
}

/************************************************************************/
/*                             ~AscDataset()                            */
/************************************************************************/

AscDataset::~AscDataset()
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
GUIntBig AscDataset::Tell()
{
    return nBufferOffset + nOffsetInBuffer;
}

/************************************************************************/
/*                                Seek()                                */
/************************************************************************/

int AscDataset::Seek(GUIntBig nNewOffset)
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

char AscDataset::Getc()
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
char** AscDataset::GetFileList()
{
    char** papszFileList = GDALPamDataset::GetFileList();

    if (papszPrj != nullptr)
        papszFileList = CSLAddString(papszFileList, osPrjFilename);

    return papszFileList;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr AscDataset::GetGeoTransform(double* padfTransform)
{
    memcpy(padfTransform, adfGeoTransform, sizeof(double) * 6);
    return(CE_None);
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char* AscDataset::GetProjectionRef()
{
    return pszProjection;
}
