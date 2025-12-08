/**
 * @file fonts.h
 * @brief Font definitions for display rendering
 */

#ifndef FONTS_H
#define FONTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const uint8_t* data;
    uint8_t width;
    uint8_t height;
    uint8_t first_char;
    uint8_t char_count;
} font_t;

extern const font_t font_5x7;

const uint8_t* font_get_char_data(const font_t* font, char c);
uint8_t font_get_char_width(const font_t* font);
uint8_t font_get_char_height(const font_t* font);

#ifdef __cplusplus
}
#endif

#endif /* FONTS_H */
