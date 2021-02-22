#ifndef __ZOOM_H_
#define __ZOOM_H_

typedef enum {
    ZT_NEAR = 0, //最近点插值
    ZT_LINEAR, //双线性插值
} Zoom_Type;

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
    Zoom_Type zt);

#endif
