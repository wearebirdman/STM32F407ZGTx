#ifndef __LCD_IMAGE_H
#define __LCD_IMAGE_H

#include "main.h"

/**
 * @brief  LCD 图片结构体
 */
typedef struct {
    uint16_t width;          // 宽度（像素）
    uint16_t height;         // 高度（像素）
    const uint16_t *data;    // 像素数据（RGB565）
} LCD_Image_t;

/**
 * @brief  定义图片对象的宏
 */
#define LCD_DEFINE_IMAGE(name, w, h, arr) \
    const LCD_Image_t name = { .width = w, .height = h, .data = arr }

/**
 * @brief  获取图片总像素数
 */
#define LCD_IMAGE_PIXELS(img)  ((uint32_t)(img)->width * (img)->height)

/**
 * @brief  获取图片数据大小（字节）
 */
#define LCD_IMAGE_SIZE(img)    ((uint32_t)(img)->width * (img)->height * 2)

#endif /* __LCD_IMAGE_H */