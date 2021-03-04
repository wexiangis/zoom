#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <sys/sysinfo.h> // get_nprocs() 获取有效cpu 核心数

#include "zoom.h"

// 原版计算方式
#define LINEAR1(p11, p12, p21, p22, le, re, ue, de) \
(unsigned char)((p11 * re + p12 * le) * de + (p21 * re + p22 * le) * ue)

// 简化版,用re替换le后
#define LINEAR2(p11, p12, p21, p22, re, ue, de) \
(unsigned char)((p12 + re * (p11 - p12)) * de + (p22 + re * (p21 - p22)) * ue)

// 选择一种计算方式
// #define LINEAR(p11, p12, p21, p22, le, re, ue, de) LINEAR1(p11, p12, p21, p22, le, re, ue, de)
#define LINEAR(p11, p12, p21, p22, le, re, ue, de) LINEAR2(p11, p12, p21, p22, re, ue, de)

typedef struct
{
    unsigned char r, g, b;
} Zoom_Rgb;

typedef struct
{
    //输入输出图像信息
    Zoom_Rgb *rgb;
    int width, height;
    Zoom_Rgb *rgbOut;
    int widthOut, heightOut;
    //多线程
    int lineDiv;
    int threadCount;
    int threadFinsh;
} Zoom_Info;

//抛线程工具
static void new_thread(void *obj, void *callback)
{
    pthread_t th;
    pthread_attr_t attr;
    int ret;
    //禁用线程同步,线程运行结束后自动释放
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    //抛出线程
    ret = pthread_create(&th, &attr, callback, (void *)obj);
    if (ret != 0)
        printf("new_thread failed !! %s\r\n", strerror(ret));
    //attr destroy
    pthread_attr_destroy(&attr);
}

void _zoom_linear(Zoom_Info *info)
{
    float floorX, floorY, ceilX, ceilY;
    float errUp, errDown, errLeft, errRight;
    int x1, x2, y1, y2;
    float xStep, yStep, xDiv, yDiv;
    int x, y;
    int offsetOut, offset11, offset12, offset21, offset22;

    //多线程
    int startLine, endLine;
    //多线程,获得自己处理行信息
    startLine = info->lineDiv * (info->threadCount++);
    endLine = startLine + info->lineDiv;
    if (endLine > info->heightOut)
        endLine = info->heightOut;

    //步宽计算(注意谁除以谁,这里表示的是在输出图像上每跳动一行、列等价于源图像跳过的行、列量)
    xDiv = (float)info->width / info->widthOut;
    yDiv = (float)info->height / info->heightOut;

    //列像素遍历
    for (y = startLine, yStep = startLine * yDiv,
        offsetOut = startLine * info->widthOut;
         y < endLine; y += 1, yStep += yDiv)
    {
        //上下2个相邻点: 距离计算
        floorY = floor(yStep);
        ceilY = ceil(yStep);
        errUp = yStep - floorY;
        errDown = 1 - errUp;

        //上下2个相邻点: 序号
        y1 = (int)floorY;
        y2 = (int)ceilY;
        if (y2 == info->height)
            y2 -= 1;

        //避免下面for循环中重复该乘法
        y1 *= info->width;
        y2 *= info->width;

        //行像素遍历
        for (x = 0, xStep = 0; x < info->widthOut; x += 1, xStep += xDiv, offsetOut += 1)
        {
            //左右2个相邻点: 距离计算
            floorX = floor(xStep);
            ceilX = ceil(xStep);
            errLeft = xStep - floorX;
            errRight = 1 - errLeft;

            //左右2个相邻点: 序号
            x1 = (int)floorX;
            x2 = (int)ceilX;
            if (x2 == info->width)
                x2 -= 1;

            //双线性插值
            offset11 = x1 + y1;
            offset12 = x2 + y1;
            offset21 = x1 + y2;
            offset22 = x2 + y2;
            info->rgbOut[offsetOut].r = LINEAR(
                info->rgb[offset11].r, info->rgb[offset12].r,
                info->rgb[offset21].r, info->rgb[offset22].r,
                errLeft, errRight, errUp, errDown);
            info->rgbOut[offsetOut].g = LINEAR(
                info->rgb[offset11].g, info->rgb[offset12].g,
                info->rgb[offset21].g, info->rgb[offset22].g,
                errLeft, errRight, errUp, errDown);
            info->rgbOut[offsetOut].b = LINEAR(
                info->rgb[offset11].b, info->rgb[offset12].b,
                info->rgb[offset21].b, info->rgb[offset22].b,
                errLeft, errRight, errUp, errDown);
        }
    }

    //多线程,处理完成行数
    info->threadFinsh++;
}

void _zoom_linear_stream(
    Zoom_Info *info,
    void *objSrc, void *objDist,
    int (*srcRead)(void *, unsigned char *, int),
    int (*distWrite)(void *, unsigned char *, int))
{
    float floorX, floorY, ceilX, ceilY;
    float errUp, errDown, errLeft, errRight;
    int x1, x2, y2;
    float xStep, yStep, xDiv, yDiv;
    int x, y;

    //当前读取行数
    int readLine = 0;
    //两行数据的指针,对应y1,y2来使用
    Zoom_Rgb *line1 = &info->rgb[0];
    Zoom_Rgb *line2 = &info->rgb[info->width];
    Zoom_Rgb *lineX;

    //步宽计算(注意谁除以谁,这里表示的是在输出图像上每跳动一行、列等价于源图像跳过的行、列量)
    xDiv = (float)info->width / info->widthOut;
    yDiv = (float)info->height / info->heightOut;

    //读取新1行数据
    srcRead(objSrc, (unsigned char *)line2, 1);
    //填充满2行
    memcpy(line1, line2, info->width * 3);

    //列像素遍历
    for (y = 0, yStep = 0 * yDiv; y < info->heightOut; y += 1, yStep += yDiv)
    {
        //上下2个相邻点: 距离计算
        floorY = floor(yStep);
        ceilY = ceil(yStep);
        errUp = yStep - floorY;
        errDown = 1 - errUp;

        //上下2个相邻点: 序号
        // y1 = (int)floorY;
        y2 = (int)ceilY;
        if (y2 == info->height)
            y2 -= 1;

        //读取足够的行数据(移动info->rgb中的行数据到能覆盖y1,y2所在行)
        while (readLine < y2)
        {
            //后面数据往前挪
            lineX = line1;
            line1 = line2;
            line2 = lineX;
            //读取新一行数据
            if (srcRead(objSrc, (unsigned char *)line2, 1) == 1)
                readLine += 1;
            else
                break;
        }

        // printf("y1 %d y2 %d - readLine %d \r\n", y1, y2, readLine);

        //行像素遍历
        for (x = 0, xStep = 0; x < info->widthOut; x += 1, xStep += xDiv)
        {
            //左右2个相邻点: 距离计算
            floorX = floor(xStep);
            ceilX = ceil(xStep);
            errLeft = xStep - floorX;
            errRight = 1 - errLeft;

            //左右2个相邻点: 序号
            x1 = (int)floorX;
            x2 = (int)ceilX;
            if (x2 == info->width)
                x2 -= 1;

            //双线性插值
            info->rgbOut[x].r = LINEAR(
                    line1[x1].r, line1[x2].r,
                    line2[x1].r, line2[x2].r,
                    errLeft, errRight, errUp, errDown);
            info->rgbOut[x].g = LINEAR(
                    line1[x1].g, line1[x2].g,
                    line2[x1].g, line2[x2].g,
                    errLeft, errRight, errUp, errDown);
            info->rgbOut[x].b = LINEAR(
                    line1[x1].b, line1[x2].b,
                    line2[x1].b, line2[x2].b,
                    errLeft, errRight, errUp, errDown);
        }

        //输出一行数据
        distWrite(objDist, (unsigned char *)info->rgbOut, 1);
    }
}

void _zoom_near(Zoom_Info *info)
{
    int xSrc, ySrc;
    float xStep, yStep, xDiv, yDiv;
    int x, y, offset;

    //多线程
    int startLine, endLine;
    //多线程,获得自己处理行信息
    startLine = info->lineDiv * (info->threadCount++);
    endLine = startLine + info->lineDiv;
    if (endLine > info->heightOut)
        endLine = info->heightOut;

    //步宽计算(注意谁除以谁,这里表示的是在输出图像上每跳动一行、列等价于源图像跳过的行、列量)
    xDiv = (float)info->width / info->widthOut;
    yDiv = (float)info->height / info->heightOut;

    //列像素遍历
    for (y = startLine, yStep = startLine * yDiv,
        offset = startLine * info->widthOut;
         y < endLine; y += 1, yStep += yDiv)
    {
        //最近y值
#if 0
        ySrc = (int)round(yStep);
        if (ySrc == info->height)
            ySrc -= 1;
#else
        //直接类型转换可以提升速度,效果相当于floor
        ySrc = (int)(yStep);
#endif

        //避免下面for循环中重复该乘法
        ySrc *= info->width;

        //行像素遍历
        for (x = 0, xStep = 0; x < info->widthOut; x += 1, xStep += xDiv)
        {
            //最近x值
#if 0
            xSrc = (int)round(xStep);
            if (xSrc == info->width)
                xSrc -= 1;
#else
            //直接类型转换可以提升速度,效果相当于floor
            xSrc = (int)(xStep);
#endif
            //拷贝最近点
            info->rgbOut[offset++] = info->rgb[ySrc + xSrc];
        }
    }

    //多线程,处理完成行数
    info->threadFinsh++;
}

void _zoom_near_stream(
    Zoom_Info *info,
    void *objSrc, void *objDist,
    int (*srcRead)(void *, unsigned char *, int),
    int (*distWrite)(void *, unsigned char *, int))
{
    int xSrc, ySrc;
    float xStep, yStep, xDiv, yDiv;
    int x, y;

    //当前已读取行数
    int readLine = 0;

    //步宽计算(注意谁除以谁,这里表示的是在输出图像上每跳动一行、列等价于源图像跳过的行、列量)
    xDiv = (float)info->width / info->widthOut;
    yDiv = (float)info->height / info->heightOut;

    //读取新一行数据
    srcRead(objSrc, (unsigned char *)info->rgb, 1);

    //列像素遍历
    for (y = 0, yStep = 0 * yDiv; y < info->heightOut; y += 1, yStep += yDiv)
    {
        //最近y值
#if 0
        ySrc = (int)round(yStep);
#else
        //直接类型转换可以提升速度,效果相当于floor
        ySrc = (int)(yStep);
#endif

        //读取足够的行数据(移动info->rgb中的行数据到能覆盖ySrc所在行)
        while (readLine < ySrc)
        {
            //读取新一行数据
            if (srcRead(objSrc, (unsigned char *)info->rgb, 1) == 1)
                readLine += 1;
            else
                break;
        }

        // printf("ySrc %d - readLine %d \r\n", ySrc, readLine);

        //行像素遍历
        for (x = 0, xStep = 0; x < info->widthOut; x += 1, xStep += xDiv)
        {
            //最近x值
#if 0
            xSrc = (int)round(xStep);
            if (xSrc == info->width)
                xSrc -= 1;
#else
            //直接类型转换可以提升速度,效果相当于floor
            xSrc = (int)(xStep);
#endif
            //拷贝最近点
            info->rgbOut[x] = info->rgb[xSrc];
        }

        //输出一行数据
        distWrite(objDist, (unsigned char *)info->rgbOut, 1);
    }
}

/*
 *  缩放rgb图像(双线性插值算法)
 *  参数:
 *      rgb: 源图像数据指针,rgb排列,3字节一像素
 *      width, height: 源图像宽、高
 *      retWidth, retHeigt: 输出图像宽、高
 *      zm: 缩放倍数,(0,1)小于1缩小倍数,(1,~]大于1放大倍数
 *      zt: 缩放方式
 *
 *  返回: 输出rgb图像数据指针 !! 用完记得free() !!
 */
unsigned char *zoom(
    unsigned char *rgb,
    int width, int height,
    int *retWidth, int *retHeight,
    float zm,
    Zoom_Type zt)
{
    int i;
    int outSize;
    int threadCount;
    int processor = 0;
    void (*callback)(Zoom_Info *);

    Zoom_Info info = {
        .rgb = (Zoom_Rgb *)rgb,
        .width = width,
        .height = height,
        .widthOut = (int)(width * zm),
        .heightOut = (int)(height * zm),
        .threadCount = 0,
        .threadFinsh = 0,
    };

    //参数检查
    if (zm <= 0 || width < 1 || height < 1)
        return NULL;

    //输出图像内存准备
    outSize = info.widthOut * info.heightOut;
    info.rgbOut = (Zoom_Rgb *)calloc(outSize, sizeof(Zoom_Rgb));

    //缩放方式
    if (zt == ZT_LINEAR)
        callback = &_zoom_linear;
    else
        callback = &_zoom_near;

    //多线程处理(输出图像大于320x240时)
    if (outSize > 76800)
    {
        //获取cpu可用核心数
        processor = get_nprocs();
    }

    //普通处理
    if (processor < 2)
    {
        info.lineDiv = info.heightOut;
        callback(&info);
    }
    //多线程处理
    else
    {
        //每核心处理行数
        info.lineDiv = info.heightOut / processor;
        if (info.lineDiv < 1)
            info.lineDiv = 1;
        //多线程
        for (i = threadCount = 0; i < info.heightOut; i += info.lineDiv)
        {
            new_thread(&info, callback);
            threadCount += 1;
        }
        //等待各线程处理完毕
        while (info.threadFinsh != threadCount)
            usleep(1000);
    }

    //返回
    if (retWidth)
        *retWidth = info.widthOut;
    if (retHeight)
        *retHeight = info.heightOut;
    return (unsigned char *)info.rgbOut;
}

/*
 *  数据流处理(为避免大张图片占用巨大内存空间)
 *  参数:
 *      obj: 用户私有参数,在调用下面回调函数时传回给用户
 *      srcRead: 源图片行数据读取回调函数
 *             : 函数原型 int srcRead(void *obj, unsigned char *rgbLine, int line)
 *      distWrite: 输出图片行数据回调函数
 *             : 函数原型 int distWrite(void *obj, unsigned char *rgbLine, int line)
 *  说明: 关于回调函数的返回,返回成功读写行数,返回0结束
 */
void zoom_stream(
    void *objSrc, void *objDist,
    int (*srcRead)(void *, unsigned char *, int),
    int (*distWrite)(void *, unsigned char *, int),
    int width, int height,
    int *retWidth, int *retHeight,
    float zm,
    Zoom_Type zt)
{
    Zoom_Info info = {
        .width = width,
        .height = height,
        .widthOut = (int)(width * zm),
        .heightOut = (int)(height * zm),
    };

    //参数检查
    if (zm <= 0 || width < 1 || height < 1)
        return;

    //输入流,行缓冲内存准备(至少2行)
    info.rgb = (Zoom_Rgb *)calloc(info.width * 2, sizeof(Zoom_Rgb));
    //输出流,行缓冲内存准备(只需1行)
    info.rgbOut = (Zoom_Rgb *)calloc(info.widthOut, sizeof(Zoom_Rgb));

    //开始缩放
    if (zt == ZT_LINEAR)
        _zoom_linear_stream(&info, objSrc, objDist, srcRead, distWrite);
    else
        _zoom_near_stream(&info, objSrc, objDist, srcRead, distWrite);

    //返回
    if (retWidth)
        *retWidth = info.widthOut;
    if (retHeight)
        *retHeight = info.heightOut;

    //内内回收
    free(info.rgb);
    free(info.rgbOut);
}
