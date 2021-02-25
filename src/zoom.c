#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <sys/sysinfo.h> // get_nprocs() 获取有效cpu 核心数

#include "zoom.h"

typedef struct
{
    unsigned char *rgb;
    int width, height;
    unsigned char *outRgb;
    int outWidth, outHeight;
    //多线程
    int divLine;
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

/*
 *  对左右和上下4个相邻点作线性插值
 *  参数:
 *      p11, p12, p21, p22: 左上、右上、左下、右下四个点
 *      errUp, errDown, errLeft, errRight: 上、下、左、右距离
 *      ret: 编辑计算得到的点
 */
void _aver_of_4_point(
    unsigned char *p11,
    unsigned char *p12,
    unsigned char *p21,
    unsigned char *p22,
    float errUp,
    float errDown,
    float errLeft,
    float errRight,
    unsigned char *ret)
{
    int i;
    //rgb逐个处理
    for (i = 0; i < 3; i++)
    {
        ret[i] = (unsigned char)((errRight * p11[i] + errLeft * p12[i]) * errDown +
                                 (errRight * p21[i] + errLeft * p22[i]) * errUp);
    }
}

void _zoom_linear(Zoom_Info *info)
{
    float floorX, floorY, ceilX, ceilY;
    float errUp, errDown, errLeft, errRight;
    int x1, x2, y1, y2;
    float xStep, yStep, xDiv, yDiv;
    int x, y, offset;

    //多线程
    int startLine, endLine;
    //多线程,获得自己处理行信息
    startLine = info->divLine * (info->threadCount++);
    endLine = startLine + info->divLine;
    if (endLine > info->outHeight)
        endLine = info->outHeight;

    //步宽计算(注意谁除以谁,这里表示的是在输出图像上每跳动一行、列等价于源图像跳过的行、列量)
    xDiv = (float)info->width / info->outWidth;
    yDiv = (float)info->height / info->outHeight;

    //列像素遍历
    for (y = startLine, yStep = startLine * yDiv,
        offset = startLine * info->outWidth * 3; y < endLine; y += 1, yStep += yDiv)
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
        for (x = 0, xStep = 0; x < info->outWidth; x += 1, xStep += xDiv, offset += 3)
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
            _aver_of_4_point(
                &info->rgb[(y1 + x1) * 3],
                &info->rgb[(y1 + x2) * 3],
                &info->rgb[(y2 + x1) * 3],
                &info->rgb[(y2 + x2) * 3],
                errUp,
                errDown,
                errLeft,
                errRight,
                &info->outRgb[offset]);
        }
    }

    //多线程,处理完成行数
    info->threadFinsh++;
}

void _zoom_near(Zoom_Info *info)
{
    int xSrc, ySrc;
    float xStep, yStep, xDiv, yDiv;
    int x, y, offset, offsetSrc;

    //多线程
    int startLine, endLine;
    //多线程,获得自己处理行信息
    startLine = info->divLine * (info->threadCount++);
    endLine = startLine + info->divLine;
    if (endLine > info->outHeight)
        endLine = info->outHeight;

    //步宽计算(注意谁除以谁,这里表示的是在输出图像上每跳动一行、列等价于源图像跳过的行、列量)
    xDiv = (float)info->width / info->outWidth;
    yDiv = (float)info->height / info->outHeight;

    //列像素遍历
    for (y = startLine, yStep = startLine * yDiv,
        offset = startLine * info->outWidth * 3; y < endLine; y += 1, yStep += yDiv)
    {
        //最近y值
        ySrc = (int)round(yStep);
        if (ySrc == info->height)
            ySrc -= 1;

        //避免下面for循环中重复该乘法
        ySrc *= info->width;

        //行像素遍历
        for (x = 0, xStep = 0; x < info->outWidth; x += 1, xStep += xDiv)
        {
            //最近x值
            xSrc = (int)round(xStep);
            if (xSrc == info->width)
                xSrc -= 1;
            //拷贝最近点
            offsetSrc = (ySrc + xSrc) * 3;
            info->outRgb[offset++] = info->rgb[offsetSrc++];
            info->outRgb[offset++] = info->rgb[offsetSrc++];
            info->outRgb[offset++] = info->rgb[offsetSrc++];
        }
    }

    //多线程,处理完成行数
    info->threadFinsh++;
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
    int processor;
    void (*callback)(Zoom_Info *);

    Zoom_Info info = {
        .rgb = rgb,
        .width = width,
        .height = height,
        .outWidth = (int)(width * zm),
        .outHeight = (int)(height * zm),
        .threadCount = 0,
        .threadFinsh = 0,
    };

    //参数检查
    if (zm <= 0 || width < 1 || height < 1)
        return NULL;

    //输出图像内存准备
    outSize = info.outWidth * info.outHeight * 3;
    info.outRgb = (unsigned char *)calloc(outSize, 1);

    //缩放方式
    if (zt == ZT_LINEAR)
        callback = &_zoom_linear;
    else
        callback = &_zoom_near;

    //多线程处理(输出图像大于320x240x3)
    if (outSize > 230400)
    {
        //获取cpu可用核心数
        processor = get_nprocs(); // * 2;
        if (processor == 0)
            processor = 4;
        //每核心处理行数
        info.divLine = info.outHeight / processor;
        if (info.divLine < 1)
            info.divLine = 1;
        //多线程
        for (i = threadCount = 0; i < info.outHeight; i += info.divLine)
        {
            new_thread(&info, callback);
            threadCount += 1;
        }
        //等待各线程处理完毕
        while (info.threadFinsh != threadCount)
            usleep(1000);
    }
    //普通处理
    else
    {
        info.divLine = info.outHeight;
        callback(&info);
    }

    //返回
    if (retWidth)
        *retWidth = info.outWidth;
    if (retHeight)
        *retHeight = info.outHeight;
    return info.outRgb;
}
