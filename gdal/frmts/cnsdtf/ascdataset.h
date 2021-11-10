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

#include "gdal_pam.h"

CPLErr WriteImageData(GDALRasterBand* poBand, int nXSize, int nYSize,
    double dfScale, VSILFILE* fpImage, char** papszOptions,
    GDALProgressFunc pfnProgress, void* pProgressData);

/************************************************************************/
/* ==================================================================== */
/*                              AscDataset                              */
/* ==================================================================== */
/************************************************************************/

class AscRasterBand;

class CPL_DLL AscDataset : public GDALPamDataset
{
    friend class AscRasterBand;

protected:
    VSILFILE* fp;

    char** papszPrj;
    CPLString   osPrjFilename;
    char* pszProjection;

    double dfScale;

    unsigned char achReadBuf[256];
    GUIntBig    nBufferOffset;
    int         nOffsetInBuffer;

    char        Getc();
    GUIntBig    Tell();
    int         Seek(GUIntBig nOffset);

    double       adfGeoTransform[6];
    GDALDataType eDataType;
    int          bHaveNoDataValue;
    double       dfNoDataValue;

    virtual int ParseHeader(const char* pszHeader, int& nStartOfData) = 0;

public:
    AscDataset();
    ~AscDataset();

    virtual char** GetFileList(void);

    virtual CPLErr GetGeoTransform(double* padfTransform);
    virtual const char* GetProjectionRef(void);
};

class AscRasterBand : public GDALPamRasterBand
{
    friend class AscDataset;

public:
    GUIntBig* panLineOffset;
    double dfScale;

public:

    AscRasterBand(AscDataset* poDS, int nDataStart);
    virtual ~AscRasterBand();

    virtual double GetNoDataValue(int* pbSuccess);
    virtual CPLErr SetNoDataValue(double dfNoData);
    virtual double GetScale(int* pbSuccess = nullptr);
    CPLErr SetScale(double dfNewScale);

    virtual CPLErr IReadBlock(int nBlockXOff, int nBlockYOff, void* pImage);
};
