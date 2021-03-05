
#include <stdio.h>
#include <stdlib.h>

#include "jpeglib.h"

typedef struct
{
    unsigned char r, g, b;
} Jpeg_Rgb;

typedef struct
{
    FILE *fp;
    int rw;       // 读写标志: 0/读 1/写
    int rowCount; // 当前已处理行计数
    int rowMax;   // rowCount计数目标
    int rowSize;
    struct jpeg_error_mgr jerr;
    struct jpeg_compress_struct cinfo;   // 压缩信息
    struct jpeg_decompress_struct dinfo; // 解压信息
} Jpeg_Private;

/*
 *  生成 bmp 图片
 *  参数:
 *      outFile: 路径
 *      rgb: 原始数据
 *      width: 宽(像素)
 *      height: 高(像素)
 *      pixelBytes: 每像素字节数
 *      quality: 压缩质量,1~100,越大越好,文件越大
 *  返回: 0成功 -1失败
 */
int jpeg_create(char *outFile, unsigned char *rgb, int width, int height, int pixelBytes, int quality)
{
    FILE *fp;
    int rowSize;
    JSAMPROW jsampRow[1];
    struct jpeg_error_mgr jerr;
    struct jpeg_compress_struct cinfo;

    // 数据流IO准备
    if ((fp = fopen(outFile, "wb")) == NULL)
    {
        fprintf(stderr, "jpeg_create: can't open %s\n", outFile);
        return -1;
    }

    // Initialize the JPEG decompression object with default error handling.
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    // 传递文件流
    jpeg_stdio_dest(&cinfo, fp);

    // 压缩参数设置
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = pixelBytes;
    cinfo.in_color_space = JCS_RGB; //压缩格式
    jpeg_set_defaults(&cinfo);

    // 设置压缩质量0~100,越大、文件越大、处理越久
    jpeg_set_quality(&cinfo, quality, TRUE);

    // 开始压缩
    jpeg_start_compress(&cinfo, TRUE);

    // 逐行扫描数据
    rowSize = width * pixelBytes;
    while (cinfo.next_scanline < cinfo.image_height)
    {
        jsampRow[0] = (JSAMPROW)&rgb[cinfo.next_scanline * rowSize];
        jpeg_write_scanlines(&cinfo, jsampRow, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(fp);
    return 0;
}

/*
 *  行处理模式
 *  参数: 同上
 *  返回: 行处理指针,NULL失败
 */
Jpeg_Private *jpeg_createLine(char *outFile, int width, int height, int pixelBytes, int quality)
{
    Jpeg_Private *jp = (Jpeg_Private *)calloc(1, sizeof(Jpeg_Private));

    // 数据流IO准备
    if ((jp->fp = fopen(outFile, "wb")) == NULL)
    {
        fprintf(stderr, "jpeg_createLine: can't open %s\n", outFile);
        free(jp);
        return NULL;
    }

    // Initialize the JPEG decompression object with default error handling.
    jp->cinfo.err = jpeg_std_error(&jp->jerr);
    jpeg_create_compress(&jp->cinfo);

    // 传递文件流
    jpeg_stdio_dest(&jp->cinfo, jp->fp);

    // 压缩参数设置
    jp->cinfo.image_width = width;
    jp->cinfo.image_height = height;
    jp->cinfo.input_components = pixelBytes;
    jp->cinfo.in_color_space = JCS_RGB; //压缩格式
    jpeg_set_defaults(&jp->cinfo);

    // 设置压缩质量0~100,越大、文件越大、处理越久
    jpeg_set_quality(&jp->cinfo, quality, TRUE);

    // 开始压缩
    jpeg_start_compress(&jp->cinfo, TRUE);

    jp->rowMax = height;
    jp->rowSize = width * pixelBytes;

    // 写标志
    jp->rw = 1;
    return jp;
}

/*
 *  bmp 图片数据获取
 *  参数:
 *      inFile: 路径
 *      width: 返回图片宽(像素), 不接收置NULL
 *      height: 返回图片高(像素), 不接收置NULL
 *      pixelBytes: 返回图片每像素的字节数, 不接收置NULL
 * 
 *  返回: 图片数据指针, 已分配内存, 用完记得释放
 */
unsigned char *jpeg_get(char *inFile, int *width, int *height, int *pixelBytes)
{
    FILE *fp;
    int offset, rowSize;
    unsigned char *retRgb;
    JSAMPROW jsampRow[1];
    struct jpeg_error_mgr jerr;
    struct jpeg_decompress_struct dinfo;

    // 数据流IO准备
    if ((fp = fopen(inFile, "rb")) == NULL)
    {
        fprintf(stderr, "jpeg_get: can't open %s\n", inFile);
        return NULL;
    }

    // Initialize the JPEG decompression object with default error handling.
    dinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&dinfo);

    // 传递文件流
    jpeg_stdio_src(&dinfo, fp);
    // 解析文件头
    if (jpeg_read_header(&dinfo, FALSE) != JPEG_HEADER_OK)
    {
        //失败
        fprintf(stderr, "jpeg_get: jpeg_read_header failed \r\n");
        jpeg_destroy_decompress(&dinfo);
        fclose(fp);
        return NULL;
    }
    // 开始解压
    if (jpeg_start_decompress(&dinfo) == FALSE)
    {
        //失败
        fprintf(stderr, "jpeg_get: jpeg_start_decompress failed \r\n");
        jpeg_finish_decompress(&dinfo);
        jpeg_destroy_decompress(&dinfo);
        fclose(fp);
        return NULL;
    }

    // 得到图片基本参数
    if (width)
        *width = dinfo.output_width;
    if (height)
        *height = dinfo.output_height;
    if (pixelBytes)
        *pixelBytes = dinfo.output_components;

    // 计算图片RGB数据大小,并分配内存
    retRgb = (unsigned char *)calloc(
        dinfo.output_width * dinfo.output_height * dinfo.output_components + 1, 1);

    // Process data
    offset = 0;
    rowSize = dinfo.output_width * dinfo.output_components;

    // 按行读取解压数据
    while (dinfo.output_scanline < dinfo.output_height)
    {
        jsampRow[0] = (JSAMPROW)&retRgb[offset];
        jpeg_read_scanlines(&dinfo, jsampRow, 1); //读取一行数据
        offset += rowSize;
    }

    jpeg_finish_decompress(&dinfo);
    jpeg_destroy_decompress(&dinfo);
    fclose(fp);
    return retRgb;
}

/*
 *  行处理模式
 *  参数: 同上
 *  返回: 行处理指针,NULL失败
 */
Jpeg_Private *jpeg_getLine(char *inFile, int *width, int *height, int *pixelBytes)
{
    Jpeg_Private *jp = (Jpeg_Private *)calloc(1, sizeof(Jpeg_Private));

    // 数据流IO准备
    if ((jp->fp = fopen(inFile, "rb")) == NULL)
    {
        fprintf(stderr, "jpeg_getLine: can't open %s\n", inFile);
        free(jp);
        return NULL;
    }

    // Initialize the JPEG decompression object with default error handling.
    jp->dinfo.err = jpeg_std_error(&jp->jerr);
    jpeg_create_decompress(&jp->dinfo);

    // 传递文件流
    jpeg_stdio_src(&jp->dinfo, jp->fp);
    // 解析文件头
    if (jpeg_read_header(&jp->dinfo, TRUE) != JPEG_HEADER_OK)
    {
        //失败
        fprintf(stderr, "jpeg_getLine: jpeg_read_header failed \r\n");
        jpeg_destroy_decompress(&jp->dinfo);
        fclose(jp->fp);
        free(jp);
        return NULL;
    }
    // 开始解压
    if (jpeg_start_decompress(&jp->dinfo) == FALSE)
    {
        //失败
        fprintf(stderr, "jpeg_getLine: jpeg_start_decompress failed \r\n");
        jpeg_finish_decompress(&jp->dinfo);
        jpeg_destroy_decompress(&jp->dinfo);
        fclose(jp->fp);
        free(jp);
        return NULL;
    }

    jp->rowMax = jp->dinfo.output_height;
    jp->rowSize = jp->dinfo.output_width * jp->dinfo.output_components;

    if (width)
        *width = jp->dinfo.output_width;
    if (height)
        *height = jp->dinfo.output_height;
    if (pixelBytes)
        *pixelBytes = jp->dinfo.output_components;

    return jp;
}

int _jpeg_createLine(Jpeg_Private *jp, unsigned char *rgbLine, int line)
{
    JSAMPROW jsampRow[line];
    // 行计数
    jp->rowCount += line;
    if (jp->rowCount > jp->rowMax)
    {
        line -= jp->rowCount - jp->rowMax;
        jp->rowCount = jp->rowMax;
    }
    // 行数据扫描
    jsampRow[0] = (JSAMPROW)rgbLine;
    jpeg_write_scanlines(&jp->cinfo, jsampRow, line);
    // 完毕内存回收
    if (jp->rowCount == jp->rowMax)
    {
        jpeg_finish_compress(&jp->cinfo);
        jpeg_destroy_compress(&jp->cinfo);
        fclose(jp->fp);
        jp->fp = NULL;
        // printf("end of _jpeg_createLine \r\n");
    }
    return line;
}

int _jpeg_getLine(Jpeg_Private *jp, unsigned char *rgbLine, int line)
{
    JSAMPROW jsampRow[1];
    // 行计数
    jp->rowCount += line;
    if (jp->rowCount > jp->rowMax)
    {
        line -= jp->rowCount - jp->rowMax;
        jp->rowCount = jp->rowMax;
    }
    // 行数据扫描
    jsampRow[0] = (JSAMPROW)rgbLine;
    jpeg_read_scanlines(&jp->dinfo, jsampRow, 1);
    // 完毕内存回收
    if (jp->rowCount == jp->rowMax)
    {
        jpeg_finish_decompress(&jp->dinfo);
        jpeg_destroy_decompress(&jp->dinfo);
        fclose(jp->fp);
        jp->fp = NULL;
        // printf("end of _jpeg_getLine \r\n");
    }
    return line;
}

/*
 *  按行rgb数据读、写
 *  参数:
 *      jp: 行处理指针
 *      rgbLine: 一行数据量,长度为 width * height * pixelBytes
 *      line: 要处理的行数
 *  返回:
 *      写图片时返回剩余行数,
 *      读图片时返回实际读取行数,
 *      返回0时结束(此时系统自动回收内存)
 */
int jpeg_line(Jpeg_Private *jp, unsigned char *rgbLine, int line)
{
    // 参数检查
    if (jp && jp->fp && rgbLine && line > 0)
    {
        if (jp->rw)
            return _jpeg_createLine(jp, rgbLine, line);
        else
            return _jpeg_getLine(jp, rgbLine, line);
    }
    return 0;
}

/*
 *  完毕释放指针
 */
void jpeg_closeLine(Jpeg_Private *jp)
{
    unsigned char *rgbLine;
    if (jp)
    {
        //用文件指针判断流是否关闭
        if (jp->fp)
        {
            //必须把行数据填充足够,否则关闭失败
            if (jp->rowCount != jp->rowMax)
            {
                rgbLine = (unsigned char *)calloc(jp->rowSize, 1);
                while (jpeg_line(jp, rgbLine, 1) == 1)
                    ;
                free(rgbLine);
            }
            //主动关闭
            if (jp->fp)
            {
                if (jp->rw)
                {
                    jpeg_finish_compress(&jp->cinfo);
                    jpeg_destroy_compress(&jp->cinfo);
                }
                else
                {
                    jpeg_finish_decompress(&jp->dinfo);
                    jpeg_destroy_decompress(&jp->dinfo);
                }
                fclose(jp->fp);
            }
        }
        jp->fp = NULL;
        free(jp);
    }
}

/*
 *  文件缩放
 *  参数:
 *      inFile, outFile: 输入输出文件,类型.jpg.jpeg.JPG.JPEG
 *      zoom: 缩放倍数,0.1到1为缩放,1.0以上放大
 *      quality: 输出图片质量,1~100,越大越好,文件越大
 */
void jpeg_zoom(char *inFile, char *outFile, float zoom, int quality)
{
    Jpeg_Private jpIn;
    Jpeg_Private jpOut;

    //输入图片一次加载完
    Jpeg_Rgb *rgbIn;
    //输入图片每次写入一行
    Jpeg_Rgb *rgbOutLine;
    //公用指针
    Jpeg_Rgb *pRgb;

    // 缩放分度格div及其增量计数
    float xStep, yStep, xDiv, yDiv;
    // 根据 xStep yStep 近似后定位到源图的位置
    int ySrc, ySrcLast;
    // 二维for循环计数
    int x, y;

    JSAMPROW jsampRow[1];

    // 参数检查
    if (!inFile || !outFile || zoom < 0.1 || quality < 1 || quality > 100)
    {
        fprintf(stderr, "jpeg_zoom: param error !!\n");
        return;
    }

    // 数据流IO准备
    if ((jpIn.fp = fopen(inFile, "rb")) == NULL)
    {
        fprintf(stderr, "jpeg_zoom: can't open %s\n", inFile);
        return;
    }
    if ((jpOut.fp = fopen(outFile, "wb")) == NULL)
    {
        fprintf(stderr, "jpeg_zoom: can't open %s\n", outFile);
        fclose(jpIn.fp);
        return;
    }

    // 编解码器初始化
    jpIn.dinfo.err = jpeg_std_error(&jpIn.jerr);
    jpOut.cinfo.err = jpeg_std_error(&jpOut.jerr);
    jpeg_create_decompress(&jpIn.dinfo);
    jpeg_create_compress(&jpOut.cinfo);

    // 解析输入图片参数
    jpeg_stdio_src(&jpIn.dinfo, jpIn.fp);
    if (jpeg_read_header(&jpIn.dinfo, FALSE) != JPEG_HEADER_OK)
    {
        fprintf(stderr, "jpeg_zoom: jpeg_read_header failed \r\n");
        goto end;
    }

    // 开始解码
    if (jpeg_start_decompress(&jpIn.dinfo) == FALSE)
    {
        fprintf(stderr, "jpeg_zoom: jpeg_start_decompress failed \r\n");
        goto end;
    }

    // 决定输出图片参数(一定要 jpeg_start_decompress 之后再查看dinfo参数)
    jpeg_stdio_dest(&jpOut.cinfo, jpOut.fp);
    jpOut.cinfo.image_width = (int)(jpIn.dinfo.output_width * zoom);
    if (jpOut.cinfo.image_width < 1)
        jpOut.cinfo.image_width = 1;
    jpOut.cinfo.image_height = (int)(jpIn.dinfo.output_height * zoom);
    if (jpOut.cinfo.image_height < 1)
        jpOut.cinfo.image_height = 1;
    jpOut.cinfo.input_components = jpIn.dinfo.output_components;
    jpOut.cinfo.in_color_space = JCS_RGB; //压缩格式
    jpeg_set_defaults(&jpOut.cinfo);
    jpeg_set_quality(&jpOut.cinfo, quality, TRUE); //压缩质量

    // 开始编码
    jpeg_start_compress(&jpOut.cinfo, TRUE);

    // 内存准备
    rgbIn = (Jpeg_Rgb *)calloc(jpIn.dinfo.output_width * jpIn.dinfo.output_height, sizeof(Jpeg_Rgb));
    rgbOutLine = (Jpeg_Rgb *)calloc(jpOut.cinfo.image_width, sizeof(Jpeg_Rgb));

    // 读取输入整图
    pRgb = rgbIn;
    while (jpIn.dinfo.output_scanline < jpIn.dinfo.output_height)
    {
        jsampRow[0] = (JSAMPROW)pRgb;
        jpeg_read_scanlines(&jpIn.dinfo, jsampRow, 1);
        pRgb += jpIn.dinfo.output_width;
    }

    // 缩放准备
    xDiv = (float)jpIn.dinfo.output_width / jpOut.cinfo.image_width;
    yDiv = (float)jpIn.dinfo.output_height / jpOut.cinfo.image_height;

    // 开始缩放
    jsampRow[0] = (JSAMPROW)rgbOutLine; // 用于写jpeg行数据
    for (y = 0, yStep = 0, ySrcLast = -1; y < jpOut.cinfo.image_height; y += 1, yStep += yDiv)
    {
        //最近y值
        ySrc = (int)(yStep);
        //同行比对,避免重复的行赋值
        if (ySrc != ySrcLast)
        {
            //更新比对值
            ySrcLast = ySrc;
            //避免下面for循环中重复该乘法
            ySrc *= jpIn.dinfo.output_width;
            //行像素遍历
            for (x = 0, xStep = 0; x < jpOut.cinfo.image_width; x += 1, xStep += xDiv)
            {
                rgbOutLine[x] = rgbIn[ySrc + (int)(xStep)];
            }
        }
        //写入一行数据
        jpeg_write_scanlines(&jpOut.cinfo, jsampRow, 1);
    }

    // 内存
    free(rgbIn);
    free(rgbOutLine);

    // 结束编解码
    jpeg_finish_decompress(&jpIn.dinfo);
    jpeg_finish_compress(&jpOut.cinfo);

end:

    // 销毁编解码器
    jpeg_destroy_decompress(&jpIn.dinfo);
    jpeg_destroy_compress(&jpOut.cinfo);

    fclose(jpOut.fp);
    fclose(jpIn.fp);
}

//固定放大2.5倍,且要求输入图像宽高为5的整数倍
void jpeg_zoom2(char *inFile, char *outFile, int quality)
{
    Jpeg_Private jpIn;
    Jpeg_Private jpOut;

    //输入图片一次加载完
    Jpeg_Rgb *rgbIn;
    //输入图片每次写入一行
    Jpeg_Rgb *rgbOutLine;
    //公用指针
    Jpeg_Rgb *pRgb, *pRgbTar;

    // 二维for循环计数
    int xDist;

    JSAMPROW jsampRow[1];

    // 参数检查
    if (!inFile || !outFile || quality < 1 || quality > 100)
    {
        fprintf(stderr, "jpeg_zoom: param error !!\n");
        return;
    }

    // 数据流IO准备
    if ((jpIn.fp = fopen(inFile, "rb")) == NULL)
    {
        fprintf(stderr, "jpeg_zoom: can't open %s\n", inFile);
        return;
    }
    if ((jpOut.fp = fopen(outFile, "wb")) == NULL)
    {
        fprintf(stderr, "jpeg_zoom: can't open %s\n", outFile);
        fclose(jpIn.fp);
        return;
    }

    // 编解码器初始化
    jpIn.dinfo.err = jpeg_std_error(&jpIn.jerr);
    jpOut.cinfo.err = jpeg_std_error(&jpOut.jerr);
    jpeg_create_decompress(&jpIn.dinfo);
    jpeg_create_compress(&jpOut.cinfo);

    // 解析输入图片参数
    jpeg_stdio_src(&jpIn.dinfo, jpIn.fp);
    if (jpeg_read_header(&jpIn.dinfo, FALSE) != JPEG_HEADER_OK)
    {
        fprintf(stderr, "jpeg_zoom: jpeg_read_header failed \r\n");
        goto end;
    }

    // 开始解码
    if (jpeg_start_decompress(&jpIn.dinfo) == FALSE)
    {
        fprintf(stderr, "jpeg_zoom: jpeg_start_decompress failed \r\n");
        goto end;
    }

    // 决定输出图片参数(一定要 jpeg_start_decompress 之后再查看dinfo参数)
    jpeg_stdio_dest(&jpOut.cinfo, jpOut.fp);
    jpOut.cinfo.image_width = (int)(jpIn.dinfo.output_width * 2.5);
    jpOut.cinfo.image_height = (int)(jpIn.dinfo.output_height * 2.5);
    jpOut.cinfo.input_components = jpIn.dinfo.output_components;
    jpOut.cinfo.in_color_space = JCS_RGB; //压缩格式
    jpeg_set_defaults(&jpOut.cinfo);
    jpeg_set_quality(&jpOut.cinfo, quality, TRUE); //压缩质量

    // 开始编码
    jpeg_start_compress(&jpOut.cinfo, TRUE);

    // 内存准备
    rgbIn = (Jpeg_Rgb *)calloc(jpIn.dinfo.output_width * jpIn.dinfo.output_height, sizeof(Jpeg_Rgb));
    rgbOutLine = (Jpeg_Rgb *)calloc(jpOut.cinfo.image_width, sizeof(Jpeg_Rgb));

    // 读取输入整图
    pRgb = rgbIn;
    while (jpIn.dinfo.output_scanline < jpIn.dinfo.output_height)
    {
        jsampRow[0] = (JSAMPROW)pRgb;
        jpeg_read_scanlines(&jpIn.dinfo, jsampRow, 1);
        pRgb += jpIn.dinfo.output_width;
    }

    // 开始缩放
    jsampRow[0] = (JSAMPROW)rgbOutLine; // 用于写jpeg行数据
    pRgb = rgbIn;
    pRgbTar = rgbIn + (jpIn.dinfo.output_width * jpIn.dinfo.output_height);
    do
    {
        //拷贝一行数据
        xDist = 0;
        do
        {
            // step += div, div = 0.4, step = 0.0/0.4/0.8, 即原图复用3次这个点
            rgbOutLine[xDist++] = *pRgb;
            rgbOutLine[xDist++] = *pRgb;
            rgbOutLine[xDist++] = *pRgb++;
            // step += div, div = 0.4, step = 1.2/1.6, 即原图复用2次这个点
            rgbOutLine[xDist++] = *pRgb;
            rgbOutLine[xDist++] = *pRgb++;
        }
        while (xDist < jpOut.cinfo.image_width);

        //写3行数据 step += div, div = 0.4, step = 0.0/0.4/0.8, 即原图复用3次该行
        jpeg_write_scanlines(&jpOut.cinfo, jsampRow, 1);
        jpeg_write_scanlines(&jpOut.cinfo, jsampRow, 1);
        jpeg_write_scanlines(&jpOut.cinfo, jsampRow, 1);

        //拷贝一行数据
        xDist = 0;
        do
        {
            // step += div, div = 0.4, step = 0.0/0.4/0.8, 即原图复用3次这个点
            rgbOutLine[xDist++] = *pRgb;
            rgbOutLine[xDist++] = *pRgb;
            rgbOutLine[xDist++] = *pRgb++;
            // step += div, div = 0.4, step = 1.2/1.6, 即原图复用2次这个点
            rgbOutLine[xDist++] = *pRgb;
            rgbOutLine[xDist++] = *pRgb++;
        }
        while (xDist < jpOut.cinfo.image_width);

        //写2行数据 step += div, div = 0.4, step = 1.2/1.6, 即原图复用2次该行
        jpeg_write_scanlines(&jpOut.cinfo, jsampRow, 1);
        jpeg_write_scanlines(&jpOut.cinfo, jsampRow, 1);
    }
    while (pRgb < pRgbTar);

    // 内存
    free(rgbIn);
    free(rgbOutLine);

    // 结束编解码
    jpeg_finish_decompress(&jpIn.dinfo);
    jpeg_finish_compress(&jpOut.cinfo);

end:

    // 销毁编解码器
    jpeg_destroy_decompress(&jpIn.dinfo);
    jpeg_destroy_compress(&jpOut.cinfo);

    fclose(jpOut.fp);
    fclose(jpIn.fp);
}