/*
 * ST7735 Simple Hello World Test - WeAct Display
 * Fills screen with blue and draws "HELLO WORLD" in white
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>
#include <string.h>

#define PIN_DC      24
#define PIN_RST     25
#define PIN_BL      18

#define SPI_DEVICE  "/dev/spidev0.0"
#define SPI_SPEED   32000000

#define LCD_WIDTH   128
#define LCD_HEIGHT  160
#define COL_OFFSET  2
#define ROW_OFFSET  1

/* Colors */
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F

static int spi_fd = -1;
static struct gpiod_chip *chip = NULL;
static struct gpiod_line_request *gpio_req = NULL;
static const unsigned int gpios[] = {PIN_DC, PIN_RST, PIN_BL};

/* Simple 5x7 font for basic characters */
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* space */
    {0x7C,0x12,0x11,0x12,0x7C}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
};

static void delay_ms(int ms) { usleep(ms * 1000); }

static void gpio_set(int pin, int val) {
    gpiod_line_request_set_value(gpio_req, pin,
        val ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
}

static void spi_write(uint8_t *data, size_t len) {
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)data,
        .len = len,
        .speed_hz = SPI_SPEED,
        .bits_per_word = 8,
    };
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
}

static void lcd_cmd(uint8_t cmd) {
    gpio_set(PIN_DC, 0);
    spi_write(&cmd, 1);
}

static void lcd_data(uint8_t data) {
    gpio_set(PIN_DC, 1);
    spi_write(&data, 1);
}

static void lcd_init(void) {
    /* Reset */
    gpio_set(PIN_RST, 1); delay_ms(50);
    gpio_set(PIN_RST, 0); delay_ms(50);
    gpio_set(PIN_RST, 1); delay_ms(150);
    
    lcd_cmd(0x01); delay_ms(150);  /* SWRESET */
    lcd_cmd(0x11); delay_ms(255);  /* SLPOUT */
    
    lcd_cmd(0xB1); lcd_data(0x01); lcd_data(0x2C); lcd_data(0x2D);
    lcd_cmd(0xB2); lcd_data(0x01); lcd_data(0x2C); lcd_data(0x2D);
    lcd_cmd(0xB3); lcd_data(0x01); lcd_data(0x2C); lcd_data(0x2D);
                   lcd_data(0x01); lcd_data(0x2C); lcd_data(0x2D);
    lcd_cmd(0xB4); lcd_data(0x07);
    lcd_cmd(0xC0); lcd_data(0xA2); lcd_data(0x02); lcd_data(0x84);
    lcd_cmd(0xC1); lcd_data(0xC5);
    lcd_cmd(0xC2); lcd_data(0x0A); lcd_data(0x00);
    lcd_cmd(0xC3); lcd_data(0x8A); lcd_data(0x2A);
    lcd_cmd(0xC4); lcd_data(0x8A); lcd_data(0xEE);
    lcd_cmd(0xC5); lcd_data(0x0E);
    
    lcd_cmd(0x21);  /* INVON for WeAct */
    lcd_cmd(0x36); lcd_data(0x00);  /* MADCTL */
    lcd_cmd(0x3A); lcd_data(0x05);  /* 16-bit color */
    
    lcd_cmd(0x13); delay_ms(10);
    lcd_cmd(0x29); delay_ms(100);
}

static void lcd_set_window(int x, int y, int w, int h) {
    lcd_cmd(0x2A);
    lcd_data(0); lcd_data(x + COL_OFFSET);
    lcd_data(0); lcd_data(x + COL_OFFSET + w - 1);
    
    lcd_cmd(0x2B);
    lcd_data(0); lcd_data(y + ROW_OFFSET);
    lcd_data(0); lcd_data(y + ROW_OFFSET + h - 1);
    
    lcd_cmd(0x2C);
}

static void lcd_fill(uint16_t color) {
    lcd_set_window(0, 0, LCD_WIDTH, LCD_HEIGHT);
    
    uint8_t buf[LCD_WIDTH * 2];
    for (int i = 0; i < LCD_WIDTH; i++) {
        buf[i*2] = color >> 8;
        buf[i*2+1] = color & 0xFF;
    }
    
    gpio_set(PIN_DC, 1);
    for (int y = 0; y < LCD_HEIGHT; y++) {
        spi_write(buf, sizeof(buf));
    }
}

static void lcd_pixel(int x, int y, uint16_t color) {
    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) return;
    
    lcd_set_window(x, y, 1, 1);
    gpio_set(PIN_DC, 1);
    uint8_t px[2] = {color >> 8, color & 0xFF};
    spi_write(px, 2);
}

static void lcd_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
    
    lcd_set_window(x, y, w, h);
    
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    uint8_t buf[256];
    int buflen = (w < 128) ? w * 2 : 256;
    
    for (int i = 0; i < buflen/2; i++) {
        buf[i*2] = hi;
        buf[i*2+1] = lo;
    }
    
    gpio_set(PIN_DC, 1);
    for (int row = 0; row < h; row++) {
        spi_write(buf, w * 2);
    }
}

static void lcd_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale) {
    int idx;
    if (c == ' ') idx = 0;
    else if (c >= 'A' && c <= 'Z') idx = c - 'A' + 1;
    else if (c >= 'a' && c <= 'z') idx = c - 'a' + 1;
    else return;
    
    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[idx][col];
        for (int row = 0; row < 7; row++) {
            uint16_t color = (line & (1 << row)) ? fg : bg;
            if (scale == 1) {
                lcd_pixel(x + col, y + row, color);
            } else {
                lcd_fill_rect(x + col*scale, y + row*scale, scale, scale, color);
            }
        }
    }
}

static void lcd_print(int x, int y, const char *str, uint16_t fg, uint16_t bg, int scale) {
    while (*str) {
        lcd_draw_char(x, y, *str, fg, bg, scale);
        x += 6 * scale;
        str++;
    }
}

static int hw_init(void) {
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) return -1;
    
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    
    struct gpiod_line_config *lcfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(lcfg, gpios, 3, settings);
    
    struct gpiod_request_config *rcfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(rcfg, "st7735");
    
    gpio_req = gpiod_chip_request_lines(chip, rcfg, lcfg);
    
    gpiod_request_config_free(rcfg);
    gpiod_line_config_free(lcfg);
    gpiod_line_settings_free(settings);
    
    if (!gpio_req) return -1;
    
    spi_fd = open(SPI_DEVICE, O_RDWR);
    if (spi_fd < 0) return -1;
    
    uint8_t mode = 0;
    uint32_t speed = SPI_SPEED;
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    
    return 0;
}

static void cleanup(void) {
    gpio_set(PIN_BL, 1);
    if (gpio_req) gpiod_line_request_release(gpio_req);
    if (chip) gpiod_chip_close(chip);
    if (spi_fd >= 0) close(spi_fd);
}

int main(void) {
    printf("ST7735 Hello World Test\n");
    
    if (hw_init() < 0) {
        printf("Init failed!\n");
        return 1;
    }
    
    lcd_init();
    gpio_set(PIN_BL, 0);  /* Backlight ON (active low) */
    
    /* Blue background */
    printf("Filling blue...\n");
    lcd_fill(BLUE);
    
    /* Print HELLO WORLD */
    printf("Drawing text...\n");
    lcd_print(10, 60, "HELLO", WHITE, BLUE, 3);
    lcd_print(10, 90, "WORLD", WHITE, BLUE, 3);
    
    printf("Done! Press Enter to exit.\n");
    getchar();
    
    cleanup();
    return 0;
}
