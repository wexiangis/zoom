/*
 *  bmp文件读写
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

//文件第1~14字节定义(注意结构体内存对齐,使之 sizeof(Bmp_FileHeader) = 14)
#define Bmp_FileHeader_Size 14
typedef struct
{
    uint8_t type[2];     //文件类型: "BM"/bmp, "BA"/.. , ...
    uint16_t size[2];    //整个文件的大小(u32,为内存对齐拆成数组)
    uint16_t reserved1;  //保留: 0
    uint16_t reserved2;  //保留: 0
    uint16_t offbits[2]; //文件数据从第几个字节开始(u32,为内存对齐拆成数组)
} Bmp_FileHeader;

//文件第15~54字节定义(紧接着就是BGR数据,注意每行必须补足为4的倍数)
#define Bmp_Info_Size 40
typedef struct
{
    uint32_t size;         //该段占用字节数
    uint32_t width;        //图像宽度, 单位像素
    int32_t height;        //图像高度, 单位像素(数据为正时为倒向)
    uint16_t planes;       //平面数, 总是为1
    uint16_t bitCount;     //单位像素占用比特数: 1, 4, 8, 16, 24, 42
    uint32_t compression;  //图像压缩方式: 0/BI_BGB 不压缩,
                           //  1/BI_RLE8 8比特游程压缩, 只用于8位位图
                           //  2/BI_RLE4 4比特游程压缩, 只用于4位位图
                           //  3/BI_BITFIELDS 比特域, 用于16/32位位图
                           //  4/BI_JPEG 位图含有jpeg图像, 用于打印机
                           //  5/BI_PWG 位图含有pwg图像, 用于打印机
    uint32_t sizeImage;    //说明图像大小, 为BI_BGB时可能为0
    int32_t xPelsPerMeter; //水平分辨率, 像素/米, 有符号整数
    int32_t yPelsPerMeter; //垂直分辨率, 像素/米, 有符号整数
    uint32_t clrUsed;      //位图实际使用彩色表中的颜色索引数(为0表示使用所有)
    uint32_t clrImportant; //图像显示有重要影响的颜色索引数
} Bmp_Info;

/*
 *  功能: 读取bmp格式图片
 *  参数:
 *      filePath: 文件地址
 *      width: 用以返回图片横向的像素个数
 *      height: 用以返回图片纵向的像素个数
 *      pixelBytes: 用以返回图片每像素占用字节数
 *
 *  返回: NULL失败,否则为图片RGB排列的数据指针 !! 用完需释放 !!
 */
unsigned char *bmp_get(char *filePath, int *width, int *height, int *pixelBytes)
{
    int fd, x, y, l, offset;

    Bmp_FileHeader head;
    Bmp_Info info;

    int pb;       //每像素字节数
    char dir = 0; //图像内存排列方式: 0/上下颠倒BGR排列 1/正序BGR排列

    unsigned char *line; //每次读取一行数据
    int lineSize;

    unsigned char *rgb; //最终整理返回的rgb数据
    int rgbSize;

    if (!filePath)
        return NULL;

    if ((fd = open(filePath, O_RDONLY)) < 0)
    {
        fprintf(stderr, "bmp_get: open file %s failed !!\r\n", filePath);
        return NULL;
    }

    //读取文件头和文件信息结构体
    if (read(fd, &head, sizeof(head)) != Bmp_FileHeader_Size ||
        read(fd, &info, sizeof(info)) != Bmp_Info_Size)
    {
        fprintf(stderr, "bmp_get: read head & info failed !!\r\n");
        close(fd);
        return NULL;
    }

    //检查文件类型(必须"BM"开头)
    if (head.type[0] != 'B' || head.type[1] != 'M')
    {
        fprintf(stderr, "bmp_get: unknow type \"%s\", must be \"BM\"\r\n", head.type);
        close(fd);
        return NULL;
    }

    //每像素字节数获得
    pb = info.bitCount / 8;
    //这里只作24位色模式支持
    if (pb != 3)
    {
        fprintf(stderr, "bmp_get: only 24-bit color mode is supported !! \r\n");
        close(fd);
        return NULL;
    }

    //图像内存排列方式(height为正值时使用颠倒排列,大多数为颠倒排列)
    if (info.height < 0)
    {
        info.height = -info.height;
        dir = 1; //正序排列
    }

    //分配一行数据的内存,每次读出一行
    lineSize = info.sizeImage / info.height;
    line = (unsigned char *)calloc(lineSize, 1);

    //分配最终返回的rgb内存
    rgbSize = info.width * info.height * 3;
    rgb = (unsigned char *)calloc(rgbSize, 1);

    //正序,BGR排列
    if (dir)
    {
        for (y = 0; y < info.height; y++)
        {
            //读一行数据
            if (read(fd, line, lineSize) != lineSize)
                break;
            //写入到rgb数组的实际起始位置
            offset = y * info.width * 3;
            //整理一行数据
            for (x = 0, l = 0; x < info.width; x++)
            {
                //把BGR顺序改为RGB
                rgb[offset + 2] = line[l++];
                rgb[offset + 1] = line[l++];
                rgb[offset] = line[l++];
                offset += 3;
            }
        }
    }
    //上下颠倒,BGR排列
    else
    {
        for (y = 0; y < info.height; y++)
        {
            //读一行数据
            if (read(fd, line, lineSize) != lineSize)
                break;
            //写入到rgb数组的实际起始位置
            offset = (info.height - y - 1) * info.width * 3;
            //整理一行数据
            for (x = 0, l = 0; x < info.width; x++)
            {
                //把BGR顺序改为RGB
                rgb[offset + 2] = line[l++];
                rgb[offset + 1] = line[l++];
                rgb[offset] = line[l++];
                offset += 3;
            }
        }
    }

    //内存回收
    close(fd);
    free(line);

    //返回 宽, 高, 像素字节
    if (width)
        *width = info.width;
    if (height)
        *height = info.height;
    if (pixelBytes)
        *pixelBytes = pb;

    return rgb;
}

/*
 *  创建图片,返回文件大小
 *  参数:
 *      filePath: 文件地址
 *      rgb: 图片矩阵数据的指针,rgb格式
 *      width: 图片横向的像素个数
 *      height: 图片纵向的像素个数
 *      pixelBytes: 图片每像素占用字节数
 *
 *  返回: 成功返回0 其它失败
 */
int bmp_create(char *filePath, unsigned char *rgb, int width, int height, int pixelBytes)
{
    int fd, x, y, l, offset;
    uint32_t fileSize;

    Bmp_FileHeader head = {
        .type = "BM",
        .offbits = {
            Bmp_FileHeader_Size + Bmp_Info_Size,
            0},
    };
    Bmp_Info info = {
        .size = Bmp_Info_Size,
        .width = width,
        .height = height,
        .planes = 1,
        .bitCount = pixelBytes * 8,
        .compression = 0,
        .xPelsPerMeter = 0,
        .yPelsPerMeter = 0,
        .clrUsed = 0,
        .clrImportant = 0,
    };

    char dir = 0; //图像内存排列方式: 0/上下颠倒BGR排列 1/正序BGR排列

    unsigned char *line; //每次读取一行数据
    int lineSize;

    int extraBytes; //每行结尾补0字节数,每行字节数规定为4的倍数,不足将补0,所以读行的时候注意跳过

    if (!filePath || width < 1 || height == 0 || pixelBytes != 3)
    {
        fprintf(stderr, "bmp_create: param error %s %dx%dx%d !!\r\n",
                filePath, width, height, pixelBytes);
        return -1;
    }

    if ((fd = open(filePath, O_WRONLY | O_CREAT, 0666)) < 0)
    {
        fprintf(stderr, "bmp_create: create file failed %s !!\r\n", filePath);
        return -1;
    }

    //图像内存排列方式(height为正值时使用颠倒排列,大多数为颠倒排列)
    if (height < 0)
    {
        height = -height;
        dir = 1; //正序排列
    }

    //图像每行多余字节数(要求每行字节需补足为4的倍数,所以有些分辨率下会多补字节)
    extraBytes = 4 - width * pixelBytes % 4;
    if (extraBytes == 4)
        extraBytes = 0;

    //每行数据量
    lineSize = width * pixelBytes + extraBytes;
    line = (unsigned char *)calloc(lineSize, 1);

    //图像数据大小
    info.sizeImage = lineSize * height;
    //文件大小
    fileSize = info.sizeImage + Bmp_FileHeader_Size + Bmp_Info_Size;
    head.size[0] = (uint16_t)(fileSize & 0xFFFF);
    head.size[1] = (uint16_t)((fileSize >> 16) & 0xFFFF);

    //写文件头和文件信息
    write(fd, &head, sizeof(head));
    write(fd, &info, sizeof(info));

    //正序,BGR排列
    if (dir)
    {
        for (y = 0; y < height; y++)
        {
            //写入到rgb数组的实际起始位置
            offset = y * info.width * 3;
            //整理一行数据
            for (x = 0, l = 0; x < width; x++)
            {
                //把RGB顺序改为BGR
                line[l++] = rgb[offset + 2];
                line[l++] = rgb[offset + 1];
                line[l++] = rgb[offset];
                offset += 3;
            }
            //写一行数据
            write(fd, line, lineSize);
        }
    }
    else
    {
        for (y = 0; y < height; y++)
        {
            //写入到rgb数组的实际起始位置
            offset = (height - y - 1) * info.width * 3;
            //整理一行数据
            for (x = 0, l = 0; x < width; x++)
            {
                //把RGB顺序改为BGR
                line[l++] = rgb[offset + 2];
                line[l++] = rgb[offset + 1];
                line[l++] = rgb[offset];
                offset += 3;
            }
            //写一行数据
            write(fd, line, lineSize);
        }
    }

    //内存回收
    close(fd);
    free(line);
    return 0;
}

/*
 *  连续输出帧图片
 *  参数:
 *      order: 帧序号,用来生成图片名称效果如: 0001.bmp
 *      folder: 帧图片保存路径,格式如: /tmp
 *      data: 图片矩阵数据的指针,rgb格式
 *      width: 图片横向的像素个数
 *      height: 图片纵向的像素个数
 *      pixelBytes: 图片每像素占用字节数
 */
void bmp_create2(int order, char *folder, unsigned char *data, int width, int height, int pixelBytes)
{
    char file[1024] = {0};
    //参数检查
    if (!folder || !data || strlen(folder) < 1 || width < 1 || height < 1 || pixelBytes < 3)
        return;
    //路径名称要不要补'/'
    if (folder[strlen(folder) - 1] == '/')
        snprintf(file, sizeof(file), "%s%04d.bmp", folder, order);
    else
        snprintf(file, sizeof(file), "%s/%04d.bmp", folder, order);
    //生成文件
    bmp_create(file, data, width, height, pixelBytes);
}
