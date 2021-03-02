
#include <stdio.h>
#include <stdlib.h>
#include "jpeglib.h"

typedef struct
{
    FILE *fp;
    int rw;       // 读写标志: 0/读 1/写
    int rowCount; // 当前已处理行计数
    struct jpeg_error_mgr jerr;
    struct jpeg_compress_struct cinfo;   // 压缩信息
    struct jpeg_decompress_struct dinfo; // 解压信息
} Jpeg_Private;

/*
 *  生成 bmp 图片
 *  参数:
 *      fileOutput: 路径
 *      rgb: 原始数据
 *      width: 宽(像素)
 *      height: 高(像素)
 *      pixelBytes: 每像素字节数
 *      quality: 压缩质量,1~100,越大越好,文件越大
 *  返回: 0成功 -1失败
 */
int jpeg_create(char *fileOutput, unsigned char *rgb, int width, int height, int pixelBytes, int quality)
{
    FILE *fp;
    int rowSize;
    JSAMPROW jsampRow[1];
    struct jpeg_error_mgr jerr;
    struct jpeg_compress_struct cinfo;

    // 数据流IO准备
    if ((fp = fopen(fileOutput, "wb")) == NULL)
    {
        printf("can't open %s\n", fileOutput);
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
Jpeg_Private *jpeg_createLine(char *fileOutput, int width, int height, int pixelBytes, int quality)
{
    Jpeg_Private *jp = (Jpeg_Private *)calloc(1, sizeof(Jpeg_Private));

    // 数据流IO准备
    if ((jp->fp = fopen(fileOutput, "wb")) == NULL)
    {
        printf("can't open %s\n", fileOutput);
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
    jpeg_set_quality(&jp->cinfo, 100, TRUE);

    // 开始压缩
    jpeg_start_compress(&jp->cinfo, TRUE);

    // 写标志
    jp->rw =1;
    return jp;
}

/*
 *  bmp 图片数据获取
 *  参数:
 *      fileInput: 路径
 *      width: 返回图片宽(像素), 不接收置NULL
 *      height: 返回图片高(像素), 不接收置NULL
 *      pixelBytes: 返回图片每像素的字节数, 不接收置NULL
 * 
 *  返回: 图片数据指针, 已分配内存, 用完记得释放
 */
unsigned char *jpeg_get(char *fileInput, int *width, int *height, int *pixelBytes)
{
    FILE *fp;
    int offset, rowSize;
    unsigned char *retBuff;
    // JSAMPARRAY jsampArray;
    JSAMPROW jsampRow[1];
    struct jpeg_error_mgr jerr;
    struct jpeg_decompress_struct dinfo;

    // 数据流IO准备
    if ((fp = fopen(fileInput, "rb")) == NULL)
    {
        fprintf(stderr, "can't open %s\n", fileInput);
        return NULL;
    }

    // Initialize the JPEG decompression object with default error handling.
    dinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&dinfo);

    // 传递文件流
    jpeg_stdio_src(&dinfo, fp);
    // 读取文件头
    jpeg_read_header(&dinfo, TRUE);
    // 开始解压
    jpeg_start_decompress(&dinfo);

    // 得到图片基本参数
    if (width)
        *width = dinfo.output_width;
    if (height)
        *height = dinfo.output_height;
    if (pixelBytes)
        *pixelBytes = dinfo.output_components;

    // 计算图片RGB数据大小,并分配内存
    retBuff = (unsigned char *)calloc(
        dinfo.output_width * dinfo.output_height * dinfo.output_components + 1, 1);

    // Process data
    offset = 0;
    rowSize = dinfo.output_width * dinfo.output_components;
    // jsampArray = (*dinfo.mem->alloc_sarray)((j_common_ptr)&dinfo, JPOOL_IMAGE, rowSize, 1);

    // 按行读取解压数据
    while (dinfo.output_scanline < dinfo.output_height)
    {
        // printf("output_scanline = %d\r\n", dinfo.output_scanline);
        jsampRow[0] = (JSAMPROW)&retBuff[offset];
        jpeg_read_scanlines(&dinfo, jsampRow, 1); //读取一行数据
        offset += rowSize;
    }

    jpeg_finish_decompress(&dinfo);
    jpeg_destroy_decompress(&dinfo);
    fclose(fp);
    return retBuff;
}

/*
 *  行处理模式
 *  参数: 同上
 *  返回: 行处理指针,NULL失败
 */
Jpeg_Private *jpeg_getLine(char *fileInput, int *width, int *height, int *pixelBytes)
{
    Jpeg_Private *jp = (Jpeg_Private *)calloc(1, sizeof(Jpeg_Private));

    // 数据流IO准备
    if ((jp->fp = fopen(fileInput, "rb")) == NULL)
    {
        printf("can't open %s\n", fileInput);
        free(jp);
        return NULL;
    }

    // Initialize the JPEG decompression object with default error handling.
    jp->dinfo.err = jpeg_std_error(&jp->jerr);
    jpeg_create_decompress(&jp->dinfo);

    // 传递文件流
    jpeg_stdio_src(&jp->dinfo, jp->fp);
    // 读取文件头
    jpeg_read_header(&jp->dinfo, TRUE);
    // 开始解压
    jpeg_start_decompress(&jp->dinfo);

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
    int ret;
    JSAMPROW jsampRow[line];
    // 行计数
    jp->rowCount += line;
    if (jp->rowCount > jp->cinfo.image_height)
    {
        line -= jp->rowCount - jp->cinfo.image_height;
        jp->rowCount = jp->cinfo.image_height;
    }
    // 行数据扫描
    jsampRow[0] = (JSAMPROW)rgbLine;
    jpeg_write_scanlines(&jp->cinfo, jsampRow, line);
    // 剩余行
    ret = jp->cinfo.image_height - jp->rowCount;
    // 完毕内存回收
    if (ret == 0)
    {
        jpeg_finish_compress(&jp->cinfo);
        jpeg_destroy_compress(&jp->cinfo);
        fclose(jp->fp);
        jp->fp = NULL;
        // printf("end of _jpeg_createLine \r\n");
    }
    return ret;
}

int _jpeg_getLine(Jpeg_Private *jp, unsigned char *rgbLine, int line)
{
    JSAMPROW jsampRow[1];
    // 行计数
    jp->rowCount += line;
    if (jp->rowCount > jp->dinfo.image_height)
    {
        line -= jp->rowCount - jp->dinfo.image_height;
        jp->rowCount = jp->dinfo.image_height;
    }
    // 行数据扫描
    jsampRow[0] = (JSAMPROW)rgbLine;
    jpeg_read_scanlines(&jp->dinfo, jsampRow, 1);
    // 完毕内存回收
    if (jp->dinfo.image_height == jp->rowCount)
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
void jpeg_line_close(Jpeg_Private *jp)
{
    if (jp)
    {
        //用文件指针判断流是否关闭
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
        jp->fp = NULL;
        free(jp);
    }
}
