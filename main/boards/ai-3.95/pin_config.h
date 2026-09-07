#pragma once

#include <driver/gpio.h>

// ============== 3.95" 480x480 ST7701 RGB 引脚定义 ==============
// 沿用 1.5.9 SILA-86hezi 板的管脚映射（已验证能驱动这块屏）
// ⚠️ GPIO0 复用：BOOT_BUTTON_GPIO 和 LCD_DB0 共用，需评估实际板子是否同时需要
//     （如有问题，把 LCD_DB0 改成其它空闲 GPIO，例如 GPIO 38）

// LCD 分辨率与色深
#define LCD_H_RES         (480)
#define LCD_V_RES         (480)
#define LCD_BIT_PER_PIXEL (16)   // RGB565
#define RGB_DATA_WIDTH    (16)
#define LCD_BUFF_LINES    (10)   // 每行一行的 buffer 行数（影响 PSRAM 占用）

// RGB 接口信号
#define LCD_IO_RGB_DISP  (-1)             // 无独立 DISP 脚
#define LCD_IO_RGB_VSYNC (GPIO_NUM_21)
#define LCD_IO_RGB_HSYNC (GPIO_NUM_14)
#define LCD_IO_RGB_DE    (GPIO_NUM_47)
#define LCD_IO_RGB_PCLK  (GPIO_NUM_48)

// RGB 数据线 D[0..15]（按 1.5.9 板子的接线）
#define LCD_IO_RGB_DATA0  (GPIO_NUM_0)
#define LCD_IO_RGB_DATA1  (GPIO_NUM_12)
#define LCD_IO_RGB_DATA2  (GPIO_NUM_11)
#define LCD_IO_RGB_DATA3  (GPIO_NUM_10)
#define LCD_IO_RGB_DATA4  (GPIO_NUM_9)
#define LCD_IO_RGB_DATA5  (GPIO_NUM_46)
#define LCD_IO_RGB_DATA6  (GPIO_NUM_3)
#define LCD_IO_RGB_DATA7  (GPIO_NUM_20)
#define LCD_IO_RGB_DATA8  (GPIO_NUM_19)
#define LCD_IO_RGB_DATA9  (GPIO_NUM_8)
#define LCD_IO_RGB_DATA10 (GPIO_NUM_18)
#define LCD_IO_RGB_DATA11 (GPIO_NUM_17)
#define LCD_IO_RGB_DATA12 (GPIO_NUM_16)
#define LCD_IO_RGB_DATA13 (GPIO_NUM_15)
#define LCD_IO_RGB_DATA14 (GPIO_NUM_7)
#define LCD_IO_RGB_DATA15 (GPIO_NUM_6)

// ST7701 3-wire SPI（用于发送 init 命令）
#define LCD_IO_SPI_CS  (GPIO_NUM_45)
#define LCD_IO_SPI_SCL (GPIO_NUM_38)
#define LCD_IO_SPI_SDA (GPIO_NUM_39)
#define LCD_IO_RST     (-1)  // 无独立复位脚，复位由 IO 扩展控制
