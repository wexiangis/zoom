
#include "jpeglib.h"
#include <stdio.h>
#include <stdlib.h>

/*
 *  当目标图片达到1080p,其rgb数据内存高达6M,
 *  为避免对整张图片进行内存分配,提供按行写入rgb数据方法
 *  参数:
 *      fileOutput : 路径
 *      width : 宽(像素)
 *      height : 高(像素)
 *      pixelBytes : 每像素字节数
 *  返回: 大于0的fd值,小于等于0失败
 */
int jpeg_open(char *fileOutput, int width, int height, int pixelBytes)
{
    return 0;
}

/*
 *  按行写入rgb数据
 *  参数:
 *      fd: 上面 jpeg_open 返回值,取地址
 *      rgbLine: 一行数据量,字节数为 width * height * pixelBytes
 *  返回: 剩余需要写入行数, 为0时结束(fd会被自动close并置0)
 */
int jpeg_line(int *fd, unsigned char *rgbLine)
{
    return 0;
}

/*
 *  生成 bmp 图片
 *  参数:
 *      fileOutput : 路径
 *      rgb : 原始数据
 *      width : 宽(像素)
 *      height : 高(像素)
 *      pixelBytes : 每像素字节数
 *  返回 : 0成功 -1失败
 */
int jpeg_create(char *fileOutput, unsigned char *rgb, int width, int height, int pixelBytes)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *outfile;
    JSAMPROW row_pointer[1]; // pointer to a single row
    int row_stride;          // physical row width in buffer

    // Initialize the JPEG decompression object with default error handling.
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    // 数据流IO准备
    if ((outfile = fopen(fileOutput, "wb")) == NULL)
    {
        printf("can't open %s\n", fileOutput);
        return -1;
    }
    jpeg_stdio_dest(&cinfo, outfile);

    // 压缩参数设置
    cinfo.image_width = width; // image width and height, in pixels
    cinfo.image_height = height;
    cinfo.input_components = pixelBytes; // # of color components pixelBytes pixel
    cinfo.in_color_space = JCS_RGB;      // colorspace of input image
    jpeg_set_defaults(&cinfo);

    // 设置压缩质量0~100,越大、文件越大、处理越久
    jpeg_set_quality(&cinfo, 100, TRUE);

    // 开始压缩
    jpeg_start_compress(&cinfo, TRUE);

    // 逐行扫描数据
    row_stride = width * pixelBytes;
    while (cinfo.next_scanline < cinfo.image_height)
    {
        // printf("next_scanline = %d\r\n", cinfo.next_scanline);
        row_pointer[0] = (JSAMPROW)&rgb[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    fclose(outfile);

    return 0;
}

/*
 *  bmp 图片数据获取
 *  参数:
 *      fileInput : 路径
 *      width : 返回图片宽(像素), 不接收置NULL
 *      height : 返回图片高(像素), 不接收置NULL
 *      pixelBytes : 返回图片每像素的字节数, 不接收置NULL
 * 
 *  返回 : 图片数据指针, 已分配内存, 用完记得释放
 */
unsigned char *jpeg_get(char *fileInput, int *width, int *height, int *pixelBytes)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *infile;
    JSAMPARRAY buffer;

    int count = 0, lineLen;
    unsigned char *retBuff;

    // Initialize the JPEG decompression object with default error handling.
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);

    // 数据流IO准备
    if ((infile = fopen(fileInput, "rb")) == NULL)
    {
        fprintf(stderr, "can't open %s\n", fileInput);
        return NULL;
    }
    jpeg_stdio_src(&cinfo, infile);

    // 读取文件头
    jpeg_read_header(&cinfo, TRUE);

    // 开始解压
    jpeg_start_decompress(&cinfo);

    // 得到图片基本参数
    if (width)
        *width = cinfo.output_width;
    if (height)
        *height = cinfo.output_height;
    if (pixelBytes)
        *pixelBytes = cinfo.output_components;

    // 计算图片RGB数据大小,并分配内存
    retBuff = (unsigned char *)calloc(
        cinfo.output_width * cinfo.output_height * cinfo.output_components + 1, 1);

    // Process data
    count = 0;
    lineLen = cinfo.output_width * cinfo.output_components;
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, lineLen, 1);

    // 按行读取解压数据
    while (cinfo.output_scanline < cinfo.output_height)
    {
        // printf("output_scanline = %d\r\n", cinfo.output_scanline);
        buffer[0] = (JSAMPROW)&retBuff[count];
        jpeg_read_scanlines(&cinfo, buffer, 1); //读取一行数据
        count += lineLen;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    fclose(infile);

    return retBuff;
}
