#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jpeg.h"
#include "zoom.h"

/*
 *  模式选择:
 *      0: 使用 jpeg_zoom 缩放(临近点插值)
 *      1: 使用 jpeg + zoom 流模式缩放(临近点插值、双线性插值)
 *      2: 使用 jpeg + zoom 整图加载多线程处理模式(临近点插值、双线性插值)
 */
#define TEST_MODE 0

#include <sys/time.h>
long getTickUs(void)
{
    struct timeval tv = {0};
    gettimeofday(&tv, NULL);
    return (long)(tv.tv_sec * 1000000u + tv.tv_usec);
}

void help(char **argv)
{
    printf(
        "Usage: %s [file: .jpg] [zoom: 0.0~1.0~max] [type: 0/near(default) 1/linear]\r\n"
        "Example: %s ./in.jpg 3\r\n",
        argv[0], argv[0]);
}

#if(TEST_MODE == 0) // 使用 jpeg_zoom 缩放

int main(int argc, char **argv)
{
    long tickUs1, tickUs2;
    //缩放倍数: 0~1缩小,等于1不变,大于1放大
    float zm = 1.0;
    printf("mode 0 \r\n");
    //传参检查
    if (argc < 3)
    {
        help(argv);
        return 0;
    }
    //缩放倍数
    zm = atof(argv[2]);
    //用时
    tickUs1 = getTickUs();
    //开始缩放
    jpeg_zoom(argv[1], "./out.jpg", zm, 75);
    // jpeg_zoom2(argv[1], "./out.jpg", 75);
    //用时
    tickUs2 = getTickUs();
    printf("total time: %.3fms \r\n", (float)(tickUs2 - tickUs1) / 1000);
    return 0;
}

#elif(TEST_MODE == 1) //使用 jpeg + zoom 流模式缩放

int main(int argc, char **argv)
{
    long tickUs1, tickUs2, tickUs3, tickUs4;
    void *jpSrc = NULL, *jpDist = NULL;
    //输入图像参数
    int width = 0, height = 0, pb = 3;
    //输出图像参数
    int outWidth = 0, outHeight = 0;
    //缩放倍数: 0~1缩小,等于1不变,大于1放大
    float zm = 1.0;
    //缩放方式: 默认使用最近插值
    Zoom_Type zt = ZT_NEAR;
    printf("mode 1 \r\n");
    //传参检查
    if (argc < 3)
    {
        help(argv);
        return 0;
    }
    //用时
    tickUs1 = getTickUs();
    //缩放倍数
    zm = atof(argv[2]);
    //缩放方式
    if (argc > 3)
        zt = atoi(argv[3]);
    //解文件
    jpSrc = jpeg_getLine(argv[1], &width, &height, &pb);
    printf("input: %s / %dx%dx%d bytes / zoom %.2f / type %d \r\n",
           argv[1], width, height, pb, zm, zt);
    //输出流准备
    if (jpSrc)
        jpDist = jpeg_createLine("./out.jpg", (int)(width * zm), (int)(height * zm), pb, 75);
    //用时
    tickUs2 = getTickUs();
    //缩放
    if (jpSrc && jpDist)
    {
        zoom_stream(
            jpSrc, jpDist, &jpeg_line, &jpeg_line,
            width, height, &outWidth, &outHeight, zm, zt);
    }
    //用时
    tickUs3 = getTickUs();
    //内存回收
    jpeg_closeLine(jpSrc);
    jpeg_closeLine(jpDist);
    //用时
    tickUs4 = getTickUs();
    printf("output: out.jpg / %dx%dx%d bytes / zoom time %.3fms / total time %.3fms\r\n",
           outWidth, outHeight, pb,
           (float)(tickUs3 - tickUs2) / 1000,
           (float)(tickUs4 - tickUs1) / 1000);
    return 0;
}

#elif(TEST_MODE == 2) // 使用 jpeg + zoom 整图加载多线程处理模式

int main(int argc, char **argv)
{
    long tickUs1, tickUs2, tickUs3, tickUs4;
    //输入图像参数
    unsigned char *map = NULL;
    int width = 0, height = 0, pb = 3;
    //输出图像参数
    unsigned char *outMap = NULL;
    int outWidth = 0, outHeight = 0;
    //缩放倍数: 0~1缩小,等于1不变,大于1放大
    float zm = 1.0;
    //缩放方式: 默认使用最近插值
    Zoom_Type zt = ZT_NEAR;
    printf("mode 2 \r\n");
    if (argc < 3)
    {
        help(argv);
        return 0;
    }
    //用时
    tickUs1 = getTickUs();
    //缩放倍数
    zm = atof(argv[2]);
    //缩放方式
    if (argc > 3)
        zt = atoi(argv[3]);
    //解文件
    map = jpeg_get(argv[1], &width, &height, &pb);
    printf("input: %s / %dx%dx%d bytes / zoom %.2f / type %d \r\n",
           argv[1], width, height, pb, zm, zt);
    //用时
    tickUs2 = getTickUs();
    //缩放
    if (map)
        outMap = zoom(map, width, height, &outWidth, &outHeight, zm, zt);
    //用时
    tickUs3 = getTickUs();
    //输出文件
    if (outMap)
    {
        jpeg_create("./out.jpg", outMap, outWidth, outHeight, pb, 75);
        //用时
        tickUs4 = getTickUs();
        printf("output: out.jpg / %dx%dx%d bytes / zoom time %.3fms / total time %.3fms\r\n",
               outWidth, outHeight, pb,
               (float)(tickUs3 - tickUs2) / 1000,
               (float)(tickUs4 - tickUs1) / 1000);
    }
    else
        printf("Error: zoom failed !!\r\n");
    //内存回收
    if (map)
        free(map);
    if (outMap)
        free(outMap);

    return 0;
}

#endif