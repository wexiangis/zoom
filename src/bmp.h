
/*
 *  bmp文件读写
 */
#ifndef _BMP_H
#define _BMP_H

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
unsigned char *bmp_get(char *filePath, int *width, int *height, int *pixelBytes);

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
int bmp_create(char *filePath, unsigned char *rgb, int width, int height, int pixelBytes);

/*
 *  连续输出帧图片
 *  参数:
 *      order: 帧序号,用来生成图片名称效果如: 0001.bmp
 *      folder: 帧图片保存路径,格式如: /tmp
 *      data: 传入, 图片矩阵数据的指针,rgb格式
 *      width: 传入, 图片横向的像素个数
 *      height: 传入, 图片纵向的像素个数
 *      per: 传入, 图片每像素占用字节数
 */
void bmp_create2(int order, char *folder, unsigned char *data, int width, int height, int per);

#endif
