/**
 * @file st7735s.c
 * @brief ST7735S Display Driver Implementation (Layer 2)
 */

#include "st7735s.h"
#include "fonts.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* MADCTL bits */
#define MADCTL_MY   0x80
#define MADCTL_MX   0x40
#define MADCTL_MV   0x20
#define MADCTL_ML   0x10
#define MADCTL_BGR  0x08
#define MADCTL_MH   0x04
#define MADCTL_RGB  0x00

struct st7735s_handle_s {
    gpio_handle_t gpio;
    spi_handle_t spi;
    st7735s_config_t config;
    uint16_t width;
    uint16_t height;
    uint8_t col_offset;
    uint8_t row_offset;
    bool initialized;
};

static void delay_ms(int ms) { usleep(ms * 1000); }

static void set_dc(st7735s_handle_t h, bool data)
{
    gpio_write(h->gpio, h->config.pins.dc_pin, 
               data ? GPIO_STATE_HIGH : GPIO_STATE_LOW);
}

static void set_rst(st7735s_handle_t h, bool high)
{
    gpio_write(h->gpio, h->config.pins.rst_pin,
               high ? GPIO_STATE_HIGH : GPIO_STATE_LOW);
}

static void write_cmd(st7735s_handle_t h, uint8_t cmd)
{
    set_dc(h, false);
    spi_write(h->spi, &cmd, 1);
}

static void write_data(st7735s_handle_t h, uint8_t data)
{
    set_dc(h, true);
    spi_write(h->spi, &data, 1);
}

static void write_data_multi(st7735s_handle_t h, const uint8_t* data, size_t len)
{
    set_dc(h, true);
    spi_write(h->spi, data, len);
}

static void set_window(st7735s_handle_t h, int x, int y, int w, int he)
{
    int x0 = x + h->col_offset;
    int x1 = x + h->col_offset + w - 1;
    int y0 = y + h->row_offset;
    int y1 = y + h->row_offset + he - 1;
    
    write_cmd(h, 0x2A);
    write_data(h, x0 >> 8);
    write_data(h, x0 & 0xFF);
    write_data(h, x1 >> 8);
    write_data(h, x1 & 0xFF);
    
    write_cmd(h, 0x2B);
    write_data(h, y0 >> 8);
    write_data(h, y0 & 0xFF);
    write_data(h, y1 >> 8);
    write_data(h, y1 & 0xFF);
    
    write_cmd(h, 0x2C);
}

static void init_display(st7735s_handle_t h)
{
    /* Hardware reset */
    set_rst(h, true); delay_ms(50);
    set_rst(h, false); delay_ms(50);
    set_rst(h, true); delay_ms(150);
    
    /* Software reset */
    write_cmd(h, 0x01);
    delay_ms(150);
    
    /* Sleep out */
    write_cmd(h, 0x11);
    delay_ms(255);
    
    /* Frame rate control */
    write_cmd(h, 0xB1);
    write_data(h, 0x01);
    write_data(h, 0x2C);
    write_data(h, 0x2D);
    
    write_cmd(h, 0xB2);
    write_data(h, 0x01);
    write_data(h, 0x2C);
    write_data(h, 0x2D);
    
    write_cmd(h, 0xB3);
    write_data(h, 0x01);
    write_data(h, 0x2C);
    write_data(h, 0x2D);
    write_data(h, 0x01);
    write_data(h, 0x2C);
    write_data(h, 0x2D);
    
    /* Display inversion control */
    write_cmd(h, 0xB4);
    write_data(h, 0x07);
    
    /* Power control */
    write_cmd(h, 0xC0);
    write_data(h, 0xA2);
    write_data(h, 0x02);
    write_data(h, 0x84);
    
    write_cmd(h, 0xC1);
    write_data(h, 0xC5);
    
    write_cmd(h, 0xC2);
    write_data(h, 0x0A);
    write_data(h, 0x00);
    
    write_cmd(h, 0xC3);
    write_data(h, 0x8A);
    write_data(h, 0x2A);
    
    write_cmd(h, 0xC4);
    write_data(h, 0x8A);
    write_data(h, 0xEE);
    
    /* VCOM control */
    write_cmd(h, 0xC5);
    write_data(h, 0x0E);
    
    /* Inversion OFF for GREENTAB */
    write_cmd(h, 0x20);
    
    /* MADCTL - MX | MY | RGB for WeAct display */
    write_cmd(h, 0x36);
    write_data(h, MADCTL_MX | MADCTL_MY | MADCTL_RGB);
    
    /* Color mode: 16-bit */
    write_cmd(h, 0x3A);
    write_data(h, 0x05);
    
    /* Gamma correction */
    write_cmd(h, 0xE0);
    uint8_t gamma_pos[] = {0x02,0x1C,0x07,0x12,0x37,0x32,0x29,0x2D,
                          0x29,0x25,0x2B,0x39,0x00,0x01,0x03,0x10};
    write_data_multi(h, gamma_pos, sizeof(gamma_pos));
    
    write_cmd(h, 0xE1);
    uint8_t gamma_neg[] = {0x03,0x1D,0x07,0x06,0x2E,0x2C,0x29,0x2D,
                          0x2E,0x2E,0x37,0x3F,0x00,0x00,0x02,0x10};
    write_data_multi(h, gamma_neg, sizeof(gamma_neg));
    
    /* Normal display on */
    write_cmd(h, 0x13);
    delay_ms(10);
    
    /* Display on */
    write_cmd(h, 0x29);
    delay_ms(100);
}

st7735s_status_t st7735s_init(gpio_handle_t gpio, spi_handle_t spi,
                               const st7735s_config_t* config, st7735s_handle_t* handle)
{
    if (!gpio || !spi || !config || !handle) return ST7735S_ERR_INVALID_ARG;
    
    st7735s_handle_t h = calloc(1, sizeof(struct st7735s_handle_s));
    if (!h) return ST7735S_ERR_UNKNOWN;
    
    h->gpio = gpio;
    h->spi = spi;
    h->config = *config;
    h->width = config->width;
    h->height = config->height;
    h->col_offset = config->col_offset;
    h->row_offset = config->row_offset;
    
    init_display(h);
    h->initialized = true;
    
    *handle = h;
    return ST7735S_OK;
}

void st7735s_deinit(st7735s_handle_t handle)
{
    if (handle) free(handle);
}

void st7735s_set_backlight(st7735s_handle_t handle, bool on)
{
    if (!handle) return;
    gpio_state_t state = on ? GPIO_STATE_LOW : GPIO_STATE_HIGH;
    if (!handle->config.pins.bl_active_low) {
        state = on ? GPIO_STATE_HIGH : GPIO_STATE_LOW;
    }
    gpio_write(handle->gpio, handle->config.pins.bl_pin, state);
}

void st7735s_fill(st7735s_handle_t handle, uint16_t color)
{
    if (!handle) return;
    st7735s_fill_rect(handle, 0, 0, handle->width, handle->height, color);
}

void st7735s_fill_rect(st7735s_handle_t handle, int16_t x, int16_t y,
                       uint16_t w, uint16_t h, uint16_t color)
{
    if (!handle) return;
    
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > handle->width) w = handle->width - x;
    if (y + h > handle->height) h = handle->height - y;
    if (w <= 0 || h <= 0) return;
    
    set_window(handle, x, y, w, h);
    
    uint8_t buf[256];
    int pixels = w * h;
    int bufpix = sizeof(buf) / 2;
    
    for (int i = 0; i < bufpix && i < pixels; i++) {
        buf[i*2] = color >> 8;
        buf[i*2+1] = color & 0xFF;
    }
    
    set_dc(handle, true);
    while (pixels > 0) {
        int chunk = (pixels > bufpix) ? bufpix : pixels;
        spi_write(handle->spi, buf, chunk * 2);
        pixels -= chunk;
    }
}

void st7735s_draw_pixel(st7735s_handle_t handle, int16_t x, int16_t y, uint16_t color)
{
    if (!handle || x < 0 || y < 0 || x >= handle->width || y >= handle->height) return;
    st7735s_fill_rect(handle, x, y, 1, 1, color);
}

void st7735s_draw_hline(st7735s_handle_t handle, int16_t x, int16_t y,
                        uint16_t w, uint16_t color)
{
    st7735s_fill_rect(handle, x, y, w, 1, color);
}

void st7735s_draw_vline(st7735s_handle_t handle, int16_t x, int16_t y,
                        uint16_t h, uint16_t color)
{
    st7735s_fill_rect(handle, x, y, 1, h, color);
}

void st7735s_draw_char(st7735s_handle_t handle, int16_t x, int16_t y, char c,
                       uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (!handle) return;
    
    const uint8_t* char_data = font_get_char_data(&font_5x7, c);
    if (!char_data) return;
    
    for (int col = 0; col < 5; col++) {
        uint8_t line = char_data[col];
        for (int row = 0; row < 7; row++) {
            uint16_t color = (line & (1 << row)) ? fg : bg;
            if (scale == 1) {
                st7735s_fill_rect(handle, x + col, y + row, 1, 1, color);
            } else {
                st7735s_fill_rect(handle, x + col*scale, y + row*scale, scale, scale, color);
            }
        }
    }
}

void st7735s_draw_string(st7735s_handle_t handle, int16_t x, int16_t y,
                         const char* str, uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (!handle || !str) return;
    while (*str) {
        st7735s_draw_char(handle, x, y, *str, fg, bg, scale);
        x += 6 * scale;
        str++;
    }
}

void st7735s_draw_progress_bar(st7735s_handle_t handle, int16_t x, int16_t y,
                                uint16_t w, uint16_t h, uint8_t percent,
                                uint16_t fg, uint16_t bg, uint16_t border)
{
    if (!handle) return;
    if (percent > 100) percent = 100;
    
    st7735s_fill_rect(handle, x, y, w, h, bg);
    int fill_w = (w * percent) / 100;
    if (fill_w > 0) {
        st7735s_fill_rect(handle, x, y, fill_w, h, fg);
    }
    st7735s_draw_hline(handle, x, y, w, border);
    st7735s_draw_hline(handle, x, y + h - 1, w, border);
}

uint16_t st7735s_get_width(st7735s_handle_t handle)
{
    return handle ? handle->width : 0;
}

uint16_t st7735s_get_height(st7735s_handle_t handle)
{
    return handle ? handle->height : 0;
}

void st7735s_set_rotation(st7735s_handle_t handle, st7735s_rotation_t rotation)
{
    (void)handle; (void)rotation;
    /* TODO: implement rotation */
}

void st7735s_invert(st7735s_handle_t handle, bool invert)
{
    if (!handle) return;
    write_cmd(handle, invert ? 0x21 : 0x20);
}

const char* st7735s_status_str(st7735s_status_t status)
{
    switch (status) {
        case ST7735S_OK:             return "OK";
        case ST7735S_ERR_INVALID_ARG: return "Invalid argument";
        case ST7735S_ERR_GPIO:       return "GPIO error";
        case ST7735S_ERR_SPI:        return "SPI error";
        case ST7735S_ERR_NOT_INIT:   return "Not initialized";
        case ST7735S_ERR_UNKNOWN:    return "Unknown error";
        default:                     return "Invalid status";
    }
}
