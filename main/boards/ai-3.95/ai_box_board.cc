#include "wifi_board.h"
#include "codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "pin_config.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_io_additions.h>
#include <esp_lcd_st7701.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <driver/gpio.h>

#define TAG "AIBox"

// ============== PCA9557 IO 扩展器（控制 LCD 电源/复位） ==============
class Pca9557 : public I2cDevice {
public:
    Pca9557(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        // 寄存器 0x01 = Config Port（输入=1，输出=0）
        // 寄存器 0x03 = Output Port
        // 默认 4 个输出口全高（拉高使能 LCD）
        WriteReg(0x01, 0x03);   // P0/P1 = 输出，P2/P3 = 输入
        WriteReg(0x03, 0xF0);   // 输出全高
    }

    void SetOutputState(uint8_t bit, uint8_t level) {
        uint8_t data = ReadReg(0x03);
        data = (data & ~(1 << bit)) | ((level ? 1 : 0) << bit);
        WriteReg(0x03, data);
    }
};

// ============== 主板类 ==============
class AIBox : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Button boot_button_;
    LcdDisplay* display_ = nullptr;
    Pca9557* pca9557_ = nullptr;

    // ---------- I2C 总线（与音频 codec 共享） ----------
    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)1,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
        pca9557_ = new Pca9557(i2c_bus_, 0x19);
    }

    // ---------- RGB 显示屏（3.95" ST7701） ----------
    void InitializeSt7701Display() {
        ESP_LOGI(TAG, "Init 3.95\" ST7701 RGB panel");

        // ST7701 厂商 init 序列（来自 1.5.9 已验证的鱼鹰光电 3.95 屏配置）
        static const st7701_lcd_init_cmd_t lcd_init_cmds[] = {
            {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
            {0xEF, (uint8_t[]){0x08}, 1, 0},
            {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
            {0xC0, (uint8_t[]){0x3B, 0x00}, 2, 0},
            {0xC1, (uint8_t[]){0x0B, 0x02}, 2, 0},
            {0xC2, (uint8_t[]){0x37, 0x02}, 2, 0},
            {0xCC, (uint8_t[]){0x10}, 1, 0},
            {0xB0, (uint8_t[]){0x00, 0x0F, 0x16, 0x0E, 0x11, 0x07, 0x09, 0x09, 0x08, 0x23, 0x05, 0x11, 0x0F, 0x28, 0x2D, 0x18}, 16, 0},
            {0xB1, (uint8_t[]){0x00, 0x0F, 0x16, 0x0E, 0x11, 0x07, 0x09, 0x08, 0x09, 0x23, 0x05, 0x11, 0x0F, 0x28, 0x2D, 0x18}, 16, 0},
            {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},
            {0xB0, (uint8_t[]){0x4D}, 1, 0},
            {0xB1, (uint8_t[]){0x33}, 1, 0},
            {0xB2, (uint8_t[]){0x87}, 1, 0},
            {0xB5, (uint8_t[]){0x4B}, 1, 0},
            {0xB7, (uint8_t[]){0x8C}, 1, 0},
            {0xB8, (uint8_t[]){0x20}, 1, 0},
            {0xC1, (uint8_t[]){0x78}, 1, 0},
            {0xC2, (uint8_t[]){0x78}, 1, 0},
            {0xD0, (uint8_t[]){0x88}, 1, 0},
            {0xE0, (uint8_t[]){0x00, 0x00, 0x02}, 3, 0},
            {0xE1, (uint8_t[]){0x02, 0xF0, 0x00, 0x00, 0x03, 0xF0, 0x00, 0x00, 0x00, 0x44, 0x44}, 11, 0},
            {0xE2, (uint8_t[]){0x10, 0x10, 0x40, 0x40, 0xF2, 0xF0, 0x00, 0x00, 0xF2, 0xF0, 0x00, 0x00}, 12, 0},
            {0xE3, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
            {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
            {0xE5, (uint8_t[]){0x07, 0xEF, 0xF0, 0xF0, 0x09, 0xF1, 0xF0, 0xF0, 0x03, 0xF3, 0xF0, 0xF0, 0x05, 0xED, 0xF0, 0xF0}, 16, 0},
            {0xE6, (uint8_t[]){0x00, 0x00, 0x11, 0x11}, 4, 0},
            {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
            {0xE8, (uint8_t[]){0x08, 0xF0, 0xF0, 0xF0, 0x0A, 0xF2, 0xF0, 0xF0, 0x04, 0xF4, 0xF0, 0xF0, 0x06, 0xEE, 0xF0, 0xF0}, 16, 0},
            {0xEB, (uint8_t[]){0x00, 0x00, 0xE4, 0xE4, 0x44, 0x88, 0x40}, 7, 0},
            {0xEC, (uint8_t[]){0x78, 0x00}, 2, 0},
            {0xED, (uint8_t[]){0x20, 0xF9, 0x87, 0x76, 0x65, 0x54, 0x4F, 0xFF, 0xFF, 0xF4, 0x45, 0x56, 0x67, 0x78, 0x9F, 0x02}, 16, 0},
            {0xEF, (uint8_t[]){0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F}, 6, 0},
            {0x11, (uint8_t[]){0x00}, 0, 120},  // Sleep Out
            {0x20, (uint8_t[]){0x00}, 0, 0},    // Display Inversion OFF
            {0x29, (uint8_t[]){0x00}, 0, 0},    // Display On
        };

        // 3-wire SPI panel IO（发 ST7701 init 命令）
        spi_line_config_t line_config = {
            .cs_io_type = IO_TYPE_GPIO,
            .cs_gpio_num = LCD_IO_SPI_CS,
            .scl_io_type = IO_TYPE_GPIO,
            .scl_gpio_num = LCD_IO_SPI_SCL,
            .sda_io_type = IO_TYPE_GPIO,
            .sda_gpio_num = LCD_IO_SPI_SDA,
            .io_expander = NULL,
        };
        esp_lcd_panel_io_3wire_spi_config_t io_config = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line_config, 0);
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_3wire_spi(&io_config, &panel_io));

        // RGB 面板配置（IDF 5.5 / esp_lcd_st7701 2.0.2 结构，颜色由 data_width=16 隐含 RGB565）
        esp_lcd_rgb_panel_config_t rgb_config = {
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .timings = {
                .pclk_hz = 16 * 1000 * 1000,
                .h_res = LCD_H_RES,
                .v_res = LCD_V_RES,
                .hsync_pulse_width = 10,
                .hsync_back_porch = 10,
                .hsync_front_porch = 20,
                .vsync_pulse_width = 10,
                .vsync_back_porch = 10,
                .vsync_front_porch = 10,
                .flags = {
                    .pclk_active_neg = false,
                },
            },
            .data_width = RGB_DATA_WIDTH,
            .num_fbs = 1,
            .bounce_buffer_size_px = LCD_H_RES * LCD_BUFF_LINES,
            .dma_burst_size = 64,
            .hsync_gpio_num = LCD_IO_RGB_HSYNC,
            .vsync_gpio_num = LCD_IO_RGB_VSYNC,
            .de_gpio_num = LCD_IO_RGB_DE,
            .pclk_gpio_num = LCD_IO_RGB_PCLK,
            .disp_gpio_num = LCD_IO_RGB_DISP,
            .data_gpio_nums = {
                LCD_IO_RGB_DATA0,  LCD_IO_RGB_DATA1,  LCD_IO_RGB_DATA2,  LCD_IO_RGB_DATA3,
                LCD_IO_RGB_DATA4,  LCD_IO_RGB_DATA5,  LCD_IO_RGB_DATA6,  LCD_IO_RGB_DATA7,
                LCD_IO_RGB_DATA8,  LCD_IO_RGB_DATA9,  LCD_IO_RGB_DATA10, LCD_IO_RGB_DATA11,
                LCD_IO_RGB_DATA12, LCD_IO_RGB_DATA13, LCD_IO_RGB_DATA14, LCD_IO_RGB_DATA15,
            },
            .flags = {
                .fb_in_psram = 1,
            },
        };

        st7701_vendor_config_t vendor_config = {
            .init_cmds = lcd_init_cmds,
            .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
            .rgb_config = &rgb_config,
            .flags = {
                .mirror_by_cmd = 0,
                .auto_del_panel_io = 1,
            },
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = LCD_BIT_PER_PIXEL;
        panel_config.reset_gpio_num = LCD_IO_RST;
        panel_config.vendor_config = &vendor_config;

        esp_lcd_panel_handle_t panel_handle = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7701(panel_io, &panel_config, &panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

        // 通过 PCA9557 全部使能（保留 1.5.9 行为：开 IO 扩展器的所有位）
        pca9557_->SetOutputState(0, 1);
        pca9557_->SetOutputState(1, 1);
        pca9557_->SetOutputState(2, 1);
        pca9557_->SetOutputState(3, 1);

        display_ = new RgbLcdDisplay(panel_io, panel_handle,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

    // ---------- 按键 ----------
    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

public:
    AIBox() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSt7701Display();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR,
            AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(AIBox);
