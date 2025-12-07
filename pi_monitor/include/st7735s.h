/**
 * @file st7735s.h
 * @brief ST7735S Display Driver Interface (Layer 2)
 */

#ifndef ST7735S_H
#define ST7735S_H

#include "gpio_driver.h"
#include "spi_driver.h"
#include "st7735s_colors.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ST7735S_OK = 0,
    ST7735S_ERR_INVALID_ARG,
    ST7735S_ERR_GPIO,
    ST7735S_ERR_SPI,
    ST7735S_ERR_NOT_INIT,
    ST7735S_ERR_UNKNOWN
} st7735s_status_t;

typedef enum {
    ST7735S_ROTATION_0 = 0,
    ST7735S_ROTATION_90,
    ST7735S_ROTATION_180,
    ST7735S_ROTATION_270
} st7735s_rotation_t;

typedef enum {
    ST7735S_VARIANT_GREENTAB,
    ST7735S_VARIANT_REDTAB,
    ST7735S_VARIANT_BLACKTAB
} st7735s_variant_t;

typedef struct {
    st7735s_variant_t variant;
    uint16_t width;
    uint16_t height;
    uint8_t col_offset;
    uint8_t row_offset;
    st7735s_rotation_t rotation;
    struct {
        uint32_t dc_pin;
        uint32_t rst_pin;
        uint32_t bl_pin;
        bool bl_active_low;
    } pins;
} st7735s_config_t;

typedef struct st7735s_handle_s* st7735s_handle_t;

/* WeAct ST7735S V1.5 default configuration */
#define ST7735S_CONFIG_WEACT_DEFAULT { \
    .variant = ST7735S_VARIANT_GREENTAB, \
    .width = 128, \
    .height = 160, \
    .col_offset = 2, \
    .row_offset = 1, \
    .rotation = ST7735S_ROTATION_0, \
    .pins = { .dc_pin = 24, .rst_pin = 25, .bl_pin = 18, .bl_active_low = true } \
}

/* Initialization */
st7735s_status_t st7735s_init(gpio_handle_t gpio, spi_handle_t spi,
                               const st7735s_config_t* config, st7735s_handle_t* handle);
void st7735s_deinit(st7735s_handle_t handle);

/* Control */
void st7735s_set_backlight(st7735s_handle_t handle, bool on);
void st7735s_set_rotation(st7735s_handle_t handle, st7735s_rotation_t rotation);
void st7735s_invert(st7735s_handle_t handle, bool invert);

/* Drawing primitives */
void st7735s_fill(st7735s_handle_t handle, uint16_t color);
void st7735s_fill_rect(st7735s_handle_t handle, int16_t x, int16_t y,
                       uint16_t w, uint16_t h, uint16_t color);
void st7735s_draw_pixel(st7735s_handle_t handle, int16_t x, int16_t y, uint16_t color);
void st7735s_draw_hline(st7735s_handle_t handle, int16_t x, int16_t y,
                        uint16_t w, uint16_t color);
void st7735s_draw_vline(st7735s_handle_t handle, int16_t x, int16_t y,
                        uint16_t h, uint16_t color);

/* Text */
void st7735s_draw_char(st7735s_handle_t handle, int16_t x, int16_t y, char c,
                       uint16_t fg, uint16_t bg, uint8_t scale);
void st7735s_draw_string(st7735s_handle_t handle, int16_t x, int16_t y,
                         const char* str, uint16_t fg, uint16_t bg, uint8_t scale);

/* UI Components */
void st7735s_draw_progress_bar(st7735s_handle_t handle, int16_t x, int16_t y,
                                uint16_t w, uint16_t h, uint8_t percent,
                                uint16_t fg, uint16_t bg, uint16_t border);

/* Getters */
uint16_t st7735s_get_width(st7735s_handle_t handle);
uint16_t st7735s_get_height(st7735s_handle_t handle);

const char* st7735s_status_str(st7735s_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* ST7735S_H */
