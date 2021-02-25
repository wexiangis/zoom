#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "zoom.h"

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

void _zoom_linear(
    unsigned char *rgb,
    int width, int height,
    unsigned char *outRgb,
    int outWidth, int outHeight)
{
    float floorX, floorY, ceilX, ceilY;
    float errUp, errDown, errLeft, errRight;
    int x1, x2, y1, y2;
    float xStep, yStep, xDiv, yDiv;
    int x, y;
    int offset = 0;

    //步宽计算(注意谁除以谁,这里表示的是在输出图像上每跳动一行、列等价于源图像跳过的行、列量)
    xDiv = (float)width / outWidth;
    yDiv = (float)height / outHeight;

    //列像素遍历
    for (y = 0, yStep = 0; y < outHeight; y += 1, yStep += yDiv)
    {
        //上下2个相邻点: 距离计算
        floorY = floor(yStep);
        ceilY = ceil(yStep);
        errUp = yStep - floorY;
        errDown = 1 - errUp; // errDown = ceilY - yStep;

        //上下2个相邻点: 序号
        y1 = (int)floorY;
        y2 = (int)ceilY;
        if (y2 == height)
            y2 -= 1;

        //行像素遍历
        for (x = 0, xStep = 0; x < outWidth; x += 1, xStep += xDiv)
        {
            //左右2个相邻点: 距离计算
            floorX = floor(xStep);
            ceilX = ceil(xStep);
            errLeft = xStep - floorX;
            errRight = 1 - errLeft; // errRight = ceilX - xStep;

            //左右2个相邻点: 序号
            x1 = (int)floorX;
            x2 = (int)ceilX;
            if (x2 == width)
                x2 -= 1;

            //双线性插值
            _aver_of_4_point(
                &rgb[(y1 * width + x1) * 3],
                &rgb[(y1 * width + x2) * 3],
                &rgb[(y2 * width + x1) * 3],
                &rgb[(y2 * width + x2) * 3],
                errUp,
                errDown,
                errLeft,
                errRight,
                &outRgb[offset]);

            offset += 3;
        }
    }
}

void _zoom_near(
    unsigned char *rgb,
    int width, int height,
    unsigned char *outRgb,
    int outWidth, int outHeight)
{
    int xSrc, ySrc;
    float xStep, yStep, xDiv, yDiv;

    int x, y;
    int offset = 0, offsetSrc = 0;

    //步宽计算(注意谁除以谁,这里表示的是在输出图像上每跳动一行、列等价于源图像跳过的行、列量)
    xDiv = (float)width / outWidth;
    yDiv = (float)height / outHeight;

    //列像素遍历
    for (y = 0, yStep = 0; y < outHeight; y += 1, yStep += yDiv)
    {
        //最近y值
        ySrc = (int)round(yStep);
        if (ySrc == height)
            ySrc -= 1;

        //行像素遍历
        for (x = 0, xStep = 0; x < outWidth; x += 1, xStep += xDiv)
        {
            //最近x值
            xSrc = (int)round(xStep);
            if (xSrc == width)
                xSrc -= 1;
            //拷贝最近点
            offsetSrc = (ySrc * width + xSrc) * 3;
            outRgb[offset++] = rgb[offsetSrc++];
            outRgb[offset++] = rgb[offsetSrc++];
            outRgb[offset++] = rgb[offsetSrc++];
        }
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
    unsigned char *outRgb;
    int outWidth, outHeight;

    //参数检查
    if (zm <= 0 || width < 1 || height < 1)
        return NULL;

    //输出图像准备
    outWidth = (int)(width * zm);
    outHeight = (int)(height * zm);
    outRgb = (unsigned char *)calloc(outWidth * outHeight * 3, 1);

    //缩放
    if (zt == ZT_LINEAR)
        _zoom_linear(rgb, width, height, outRgb, outWidth, outHeight);
    else
        _zoom_near(rgb, width, height, outRgb, outWidth, outHeight);

    //返回
    if (retWidth)
        *retWidth = outWidth;
    if (retHeight)
        *retHeight = outHeight;
    return outRgb;
}
