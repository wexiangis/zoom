
#ifndef _JPEG_H
#define _JPEG_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 *  bmp 图片数据获取
 *  参数:
 *      fileInput : 路径
 *      retBuffSize : 返回图片数据字节数, 不接收置NULL
 *      width : 返回图片宽(像素), 不接收置NULL
 *      height : 返回图片高(像素), 不接收置NULL
 *      per : 返回图片每像素的字节数, 不接收置NULL
 * 
 *  返回 : 图片数据指针, 已分配内存, 用完记得释放
 */
unsigned char *jpeg_get(char *fileInput, int *retBuffSize, int *width, int *height, int *pw);

/*
 *  生成 bmp 图片
 *  参数:
 *      fileOutput : 路径
 *      image_buffer : 原始数据
 *      width : 宽(像素)
 *      height : 高(像素)
 *      per : 每像素字节数
 *  返回 : 0成功 -1失败
 */
int jpeg_create(char *fileOutput, unsigned char *image_buffer, int width, int height, int pw);

#ifdef __cplusplus
};
#endif

#endif
