/**
 * @file st7735s_colors.h
 * @brief ST7735S Color Definitions (RGB565)
 */

#ifndef ST7735S_COLORS_H
#define ST7735S_COLORS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ST7735S_RGB(r, g, b) \
    ((uint16_t)(((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))

#define ST7735S_RED_COMPONENT(c)   (((c) >> 8) & 0xF8)
#define ST7735S_GREEN_COMPONENT(c) (((c) >> 3) & 0xFC)
#define ST7735S_BLUE_COMPONENT(c)  (((c) << 3) & 0xF8)

#define ST7735S_BLACK       0x0000
#define ST7735S_WHITE       0xFFFF
#define ST7735S_RED         0xF800
#define ST7735S_GREEN       0x07E0
#define ST7735S_BLUE        0x001F
#define ST7735S_CYAN        0x07FF
#define ST7735S_MAGENTA     0xF81F
#define ST7735S_YELLOW      0xFFE0
#define ST7735S_ORANGE      0xFD20
#define ST7735S_GRAY        0x8410
#define ST7735S_DARK_GRAY   0x4208
#define ST7735S_LIGHT_GRAY  0xC618
#define ST7735S_DARK_RED    0x8000
#define ST7735S_DARK_GREEN  0x03E0
#define ST7735S_DARK_BLUE   0x0010

static inline uint16_t st7735s_blend(uint16_t c1, uint16_t c2, uint8_t alpha)
{
    uint8_t r1 = ST7735S_RED_COMPONENT(c1);
    uint8_t g1 = ST7735S_GREEN_COMPONENT(c1);
    uint8_t b1 = ST7735S_BLUE_COMPONENT(c1);
    uint8_t r2 = ST7735S_RED_COMPONENT(c2);
    uint8_t g2 = ST7735S_GREEN_COMPONENT(c2);
    uint8_t b2 = ST7735S_BLUE_COMPONENT(c2);
    uint8_t r = r1 + (((r2 - r1) * alpha) >> 8);
    uint8_t g = g1 + (((g2 - g1) * alpha) >> 8);
    uint8_t b = b1 + (((b2 - b1) * alpha) >> 8);
    return ST7735S_RGB(r, g, b);
}

#ifdef __cplusplus
}
#endif

#endif /* ST7735S_COLORS_H */
