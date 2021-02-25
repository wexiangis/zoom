#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jpeg.h"
#include "bmp.h"
#include "zoom.h"

#include <sys/time.h>
long getTickUs(void)
{
    struct timeval tv = {0};
    gettimeofday(&tv, NULL);
    return (long)(tv.tv_sec * 1000000u + tv.tv_usec);
}

char *get_tail(char *file)
{
    int i, len = strlen(file);
    for (i = len - 1; i > 0; i--)
    {
        if (file[i] == '.')
            break;
    }
    if (i < 0)
        i = 0;
    return &file[i];
}

void help(char **argv)
{
    printf(
        "Usage: %s [file: .jpg .bmp] [zoom: 0.0~1.0~max] [type: 0/near(default) 1/linear]\r\n"
        "Example: %s ./in.bmp 3\r\n",
        argv[0], argv[0]);
}

int main(int argc, char **argv)
{
    char *tail;
    long tickUs1, tickUs2, tickUs3, tickUs4;

    //输入图像参数
    unsigned char *map;
    int width = 0, height = 0, pb = 3;

    //输出图像参数
    unsigned char *outMap;
    int outWidth, outHeight;

    //缩放倍数: 0~1缩小,等于1不变,大于1放大
    float zm = 1.0;
    //缩放方式: 默认使用最近插值
    Zoom_Type zt = ZT_NEAR;

    if (argc < 3)
    {
        help(argv);
        return 0;
    }

    //用时
    tickUs1 = getTickUs();

    //解文件
    tail = get_tail(argv[1]);
    if (strstr(tail, ".bmp") || strstr(tail, ".BMP"))
    {
        map = bmp_get(argv[1], &width, &height, &pb);
    }
    else if (strstr(tail, ".jpg") || strstr(tail, ".JPG") ||
             strstr(tail, ".jpeg") || strstr(tail, ".JPEG"))
    {
        map = jpeg_get(argv[1], &width, &height, &pb);
    }
    else
    {
        printf("Error: unknow file type \"%s\" \r\n", argv[1]);
        help(argv);
        return 1;
    }

    //缩放倍数
    zm = atof(argv[2]);

    //缩放方式
    if (argc > 3)
        zt = atoi(argv[3]);

    printf("input: %s / %dx%dx%d bytes / zoom %.2f / type %d \r\n",
           argv[1], width, height, pb, zm, zt);

    //用时
    tickUs2 = getTickUs();

    //缩放
    outMap = zoom(map, width, height, &outWidth, &outHeight, zm, zt);

    //用时
    tickUs3 = getTickUs();

    //输出文件
    if (outMap)
    {
        // bmp_create("./out.bmp", outMap, outWidth, outHeight, pb);
        jpeg_create("./out.jpg", outMap, outWidth, outHeight, pb, 100);

        //用时
        tickUs4 = getTickUs();

        printf("output: out.jpg / %dx%dx%d bytes / zoom time %.3fms / total time %.3fms\r\n",
               outWidth, outHeight, pb,
               (float)(tickUs3 - tickUs2) / 1000,
               (float)(tickUs4 - tickUs1) / 1000);

        //内存回收
        free(outMap);
    }
    else
        printf("Error: zoom failed !!\r\n");

    //内存回收
    free(map);
    return 0;
}
