#ifndef __ZOOM_H_
#define __ZOOM_H_

typedef enum
{
    ZT_NEAR = 0, //最近点插值
    ZT_LINEAR,   //双线性插值
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

/*
 *  数据流处理(为避免大张图片占用巨大内存空间)
 *  参数:
 *      obj: 用户私有参数,在调用下面回调函数时传回给用户
 *      srcRead: 源图片行数据读取回调函数
 *             : 函数原型 int srcRead(void *obj, unsigned char *rgbLine, int line)
 *      distWrite: 输出图片行数据回调函数
 *             : 函数原型 int distWrite(void *obj, unsigned char *rgbLine, int line)
 *  说明: 关于回调函数的返回,返回成功读写行数,返回0异常或结束
 */
void zoom_stream(
    void *objSrc, void *objDist,
    int (*srcRead)(void *, unsigned char *, int),
    int (*distWrite)(void *, unsigned char *, int),
    int width, int height,
    int *retWidth, int *retHeight,
    float zm,
    Zoom_Type zt);

#endif
