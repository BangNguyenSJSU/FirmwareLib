/*
 * Raspberry Pi 5 System Monitor
 * For WeAct ST7735S V1.5 (20150303) 128x160 LCD
 * 
 * This display is a GREENTAB variant:
 *   - Requires BGR color order (MADCTL bit 3)
 *   - Column offset = 2, Row offset = 1
 *   - Inversion ON
 *
 * Build:
 *   gcc -o pi_monitor pi_monitor.c -lgpiod -lm
 *
 * Run:
 *   sudo ./pi_monitor
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>
#include <time.h>
#include <signal.h>
#include <math.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Hardware pins */
#define PIN_DC      24
#define PIN_RST     25
#define PIN_BL      18

#define SPI_DEVICE  "/dev/spidev0.0"
#define SPI_SPEED   12000000

/* Display - GREENTAB configuration */
#define LCD_WIDTH   128
#define LCD_HEIGHT  160
#define COL_OFFSET  2
#define ROW_OFFSET  1

/*
 * MADCTL bits
 */
#define MADCTL_MY   0x80  /* Row Address Order */
#define MADCTL_MX   0x40  /* Column Address Order */
#define MADCTL_MV   0x20  /* Row/Column Exchange */
#define MADCTL_ML   0x10  /* Vertical Refresh Order */
#define MADCTL_BGR  0x08  /* BGR color order */
#define MADCTL_MH   0x04  /* Horizontal Refresh Order */
#define MADCTL_RGB  0x00  /* RGB color order */

/*
 * For WeAct ST7735S GREENTAB (V1.5):
 * Rotation 0 (Portrait): MADCTL = MX | MY | BGR = 0xC8
 * 
 * This gives correct colors AND correct orientation
 */
#define MADCTL_CONFIG   (MADCTL_MX | MADCTL_MY | MADCTL_RGB)  /* 0xC8 */

/* Colors (RGB565) */
#define BLACK       0x0000
#define WHITE       0xFFFF
#define RED         0xF800
#define GREEN       0x07E0
#define BLUE        0x001F
#define CYAN        0x07FF
#define MAGENTA     0xF81F
#define YELLOW      0xFFE0
#define ORANGE      0xFD20
#define DARK_GRAY   0x4208
#define LIGHT_GRAY  0x8410
#define DARK_GREEN  0x03E0
#define DARK_BLUE   0x0010
#define DARK_RED    0x8000

/* UI Layout */
#define HEADER_HEIGHT   14
#define ROW_HEIGHT      24
#define BAR_HEIGHT      8
#define MARGIN          4
#define BAR_WIDTH       (LCD_WIDTH - 2*MARGIN - 40)

/* Globals */
static int spi_fd = -1;
static struct gpiod_chip *chip = NULL;
static struct gpiod_line_request *gpio_req = NULL;
static const unsigned int gpios[] = {PIN_DC, PIN_RST, PIN_BL};
static volatile int running = 1;

/* Previous CPU stats */
static unsigned long prev_idle = 0, prev_total = 0;

/* 5x7 font */
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /*   */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x00,0x08,0x14,0x22,0x41}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x41,0x22,0x14,0x08,0x00}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
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
    {0x7F,0x02,0x04,0x02,0x7F}, /* M */
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
    {0x00,0x7F,0x41,0x41,0x00}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* \ */
    {0x00,0x41,0x41,0x7F,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* _ */
    {0x00,0x01,0x02,0x04,0x00}, /* ` */
    {0x20,0x54,0x54,0x54,0x78}, /* a */
    {0x7F,0x48,0x44,0x44,0x38}, /* b */
    {0x38,0x44,0x44,0x44,0x20}, /* c */
    {0x38,0x44,0x44,0x48,0x7F}, /* d */
    {0x38,0x54,0x54,0x54,0x18}, /* e */
    {0x08,0x7E,0x09,0x01,0x02}, /* f */
    {0x08,0x14,0x54,0x54,0x3C}, /* g */
    {0x7F,0x08,0x04,0x04,0x78}, /* h */
    {0x00,0x44,0x7D,0x40,0x00}, /* i */
    {0x20,0x40,0x44,0x3D,0x00}, /* j */
    {0x7F,0x10,0x28,0x44,0x00}, /* k */
    {0x00,0x41,0x7F,0x40,0x00}, /* l */
    {0x7C,0x04,0x18,0x04,0x78}, /* m */
    {0x7C,0x08,0x04,0x04,0x78}, /* n */
    {0x38,0x44,0x44,0x44,0x38}, /* o */
    {0x7C,0x14,0x14,0x14,0x08}, /* p */
    {0x08,0x14,0x14,0x18,0x7C}, /* q */
    {0x7C,0x08,0x04,0x04,0x08}, /* r */
    {0x48,0x54,0x54,0x54,0x20}, /* s */
    {0x04,0x3F,0x44,0x40,0x20}, /* t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w */
    {0x44,0x28,0x10,0x28,0x44}, /* x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y */
    {0x44,0x64,0x54,0x4C,0x44}, /* z */
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

static void lcd_data_multi(const uint8_t *data, size_t len) {
    gpio_set(PIN_DC, 1);
    spi_write((uint8_t *)data, len);
}

/*
 * LCD Initialization for WeAct ST7735S V1.5 (GREENTAB)
 * 
 * Key settings:
 *   - MADCTL = 0xC8 (MX | MY | BGR) for correct colors and orientation
 *   - INVON for correct color inversion
 *   - Full power control and gamma correction
 */
static void lcd_init(void) {
    /* Hardware reset */
    gpio_set(PIN_RST, 1); delay_ms(50);
    gpio_set(PIN_RST, 0); delay_ms(50);
    gpio_set(PIN_RST, 1); delay_ms(150);
    
    /* Software reset */
    lcd_cmd(0x01);
    delay_ms(150);
    
    /* Sleep out */
    lcd_cmd(0x11);
    delay_ms(255);
    
    /* Frame rate control - normal mode */
    lcd_cmd(0xB1);
    lcd_data(0x01);
    lcd_data(0x2C);
    lcd_data(0x2D);
    
    /* Frame rate control - idle mode */
    lcd_cmd(0xB2);
    lcd_data(0x01);
    lcd_data(0x2C);
    lcd_data(0x2D);
    
    /* Frame rate control - partial mode */
    lcd_cmd(0xB3);
    lcd_data(0x01);
    lcd_data(0x2C);
    lcd_data(0x2D);
    lcd_data(0x01);
    lcd_data(0x2C);
    lcd_data(0x2D);
    
    /* Display inversion control */
    lcd_cmd(0xB4);
    lcd_data(0x07);
    
    /* Power control 1 */
    lcd_cmd(0xC0);
    lcd_data(0xA2);
    lcd_data(0x02);
    lcd_data(0x84);
    
    /* Power control 2 */
    lcd_cmd(0xC1);
    lcd_data(0xC5);
    
    /* Power control 3 (normal mode) */
    lcd_cmd(0xC2);
    lcd_data(0x0A);
    lcd_data(0x00);
    
    /* Power control 4 (idle mode) */
    lcd_cmd(0xC3);
    lcd_data(0x8A);
    lcd_data(0x2A);
    
    /* Power control 5 (partial mode) */
    lcd_cmd(0xC4);
    lcd_data(0x8A);
    lcd_data(0xEE);
    
    /* VCOM control */
    lcd_cmd(0xC5);
    lcd_data(0x0E);
    
    /* Inversion OFF - required for this display */
    lcd_cmd(0x20);
    
    /*
     * MADCTL - Memory Access Control
     * 
     * For WeAct ST7735S V1.5 (GREENTAB):
     * 0xC8 = MX (0x40) | MY (0x80) | BGR (0x08)
     * 
     * This gives:
     *   - Correct color order (BGR)
     *   - Correct screen orientation (not rotated)
     */
    lcd_cmd(0x36);
    lcd_data(MADCTL_CONFIG);
    
    /* Color mode: 16-bit RGB565 */
    lcd_cmd(0x3A);
    lcd_data(0x05);
    
    /* Gamma positive correction */
    lcd_cmd(0xE0);
    {
        const uint8_t gamma_pos[] = {
            0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
            0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10
        };
        lcd_data_multi(gamma_pos, sizeof(gamma_pos));
    }
    
    /* Gamma negative correction */
    lcd_cmd(0xE1);
    {
        const uint8_t gamma_neg[] = {
            0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
            0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10
        };
        lcd_data_multi(gamma_neg, sizeof(gamma_neg));
    }
    
    /* Normal display on */
    lcd_cmd(0x13);
    delay_ms(10);
    
    /* Display on */
    lcd_cmd(0x29);
    delay_ms(100);
    
    printf("LCD initialized:\n");
    printf("  MADCTL = 0x%02X (MX|MY|BGR)\n", MADCTL_CONFIG);
    printf("  Offsets: col=%d, row=%d\n", COL_OFFSET, ROW_OFFSET);
    printf("  Inversion: ON\n");
}

static void lcd_set_window(int x, int y, int w, int h) {
    int x0 = x + COL_OFFSET;
    int x1 = x + COL_OFFSET + w - 1;
    int y0 = y + ROW_OFFSET;
    int y1 = y + ROW_OFFSET + h - 1;
    
    lcd_cmd(0x2A);
    lcd_data(x0 >> 8);
    lcd_data(x0 & 0xFF);
    lcd_data(x1 >> 8);
    lcd_data(x1 & 0xFF);
    
    lcd_cmd(0x2B);
    lcd_data(y0 >> 8);
    lcd_data(y0 & 0xFF);
    lcd_data(y1 >> 8);
    lcd_data(y1 & 0xFF);
    
    lcd_cmd(0x2C);
}

static void lcd_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LCD_WIDTH) w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
    if (w <= 0 || h <= 0) return;
    
    lcd_set_window(x, y, w, h);
    
    uint8_t buf[256];
    int pixels = w * h;
    int bufpix = sizeof(buf) / 2;
    
    /* Fill buffer with color (big-endian for SPI) */
    for (int i = 0; i < bufpix && i < pixels; i++) {
        buf[i*2] = color >> 8;
        buf[i*2+1] = color & 0xFF;
    }
    
    gpio_set(PIN_DC, 1);
    while (pixels > 0) {
        int chunk = (pixels > bufpix) ? bufpix : pixels;
        spi_write(buf, chunk * 2);
        pixels -= chunk;
    }
}

static void lcd_fill(uint16_t color) {
    lcd_fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

static int char_index(char c) {
    if (c >= ' ' && c <= '`') return c - ' ';
    if (c >= 'a' && c <= 'z') return c - 'a' + 65;
    return 0;
}

static void lcd_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg, int scale) {
    int idx = char_index(c);
    if (idx < 0 || idx >= (int)(sizeof(font5x7)/sizeof(font5x7[0]))) return;
    
    for (int col = 0; col < 5; col++) {
        uint8_t line = font5x7[idx][col];
        for (int row = 0; row < 7; row++) {
            uint16_t color = (line & (1 << row)) ? fg : bg;
            if (scale == 1) {
                lcd_fill_rect(x + col, y + row, 1, 1, color);
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

static void lcd_draw_hline(int x, int y, int w, uint16_t color) {
    lcd_fill_rect(x, y, w, 1, color);
}

static void draw_bar(int x, int y, int w, int h, int percent, uint16_t fg, uint16_t bg) {
    lcd_fill_rect(x, y, w, h, bg);
    int fill_w = (w * percent) / 100;
    if (fill_w > 0) {
        lcd_fill_rect(x, y, fill_w, h, fg);
    }
    lcd_draw_hline(x, y, w, LIGHT_GRAY);
    lcd_draw_hline(x, y + h - 1, w, LIGHT_GRAY);
}

static uint16_t get_level_color(int percent) {
    if (percent < 50) return GREEN;
    if (percent < 75) return YELLOW;
    if (percent < 90) return ORANGE;
    return RED;
}

static int get_cpu_usage(void) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return 0;
    
    char line[256];
    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return 0;
    }
    fclose(fp);
    
    unsigned long user, nice, system, idle, iowait, irq, softirq;
    sscanf(line, "cpu %lu %lu %lu %lu %lu %lu %lu",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq);
    
    unsigned long total = user + nice + system + idle + iowait + irq + softirq;
    unsigned long idle_time = idle + iowait;
    
    unsigned long total_diff = total - prev_total;
    unsigned long idle_diff = idle_time - prev_idle;
    
    prev_total = total;
    prev_idle = idle_time;
    
    if (total_diff == 0) return 0;
    return (int)(100 * (total_diff - idle_diff) / total_diff);
}

static int get_mem_usage(int *used_mb, int *total_mb) {
    struct sysinfo si;
    if (sysinfo(&si) != 0) return 0;
    
    unsigned long total = si.totalram / (1024 * 1024);
    unsigned long free_mem = si.freeram / (1024 * 1024);
    unsigned long buffers = si.bufferram / (1024 * 1024);
    
    unsigned long cached = 0;
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Cached:", 7) == 0) {
                sscanf(line, "Cached: %lu", &cached);
                cached /= 1024;
                break;
            }
        }
        fclose(fp);
    }
    
    unsigned long used = total - free_mem - buffers - cached;
    *total_mb = (int)total;
    *used_mb = (int)used;
    return (int)(100 * used / total);
}

static float get_cpu_temp(void) {
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!fp) return 0.0f;
    int temp;
    if (fscanf(fp, "%d", &temp) != 1) temp = 0;
    fclose(fp);
    return temp / 1000.0f;
}

static int get_disk_usage(int *used_gb, int *total_gb) {
    struct statvfs st;
    if (statvfs("/", &st) != 0) return 0;
    
    unsigned long total = (st.f_blocks * st.f_frsize) / (1024 * 1024 * 1024);
    unsigned long free_space = (st.f_bfree * st.f_frsize) / (1024 * 1024 * 1024);
    unsigned long used = total - free_space;
    
    *total_gb = (int)total;
    *used_gb = (int)used;
    if (total == 0) return 0;
    return (int)(100 * used / total);
}

static void get_ip_address(char *buf, int buflen) {
    struct ifaddrs *ifaddr, *ifa;
    strcpy(buf, "No IP");
    if (getifaddrs(&ifaddr) == -1) return;
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        if (strcmp(ifa->ifa_name, "lo") == 0) continue;
        
        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &addr->sin_addr, buf, buflen);
        break;
    }
    freeifaddrs(ifaddr);
}

static void get_uptime_str(char *buf, int buflen) {
    struct sysinfo si;
    if (sysinfo(&si) != 0) {
        strcpy(buf, "??:??");
        return;
    }
    
    long uptime = si.uptime;
    int days = uptime / 86400;
    int hours = (uptime % 86400) / 3600;
    int mins = (uptime % 3600) / 60;
    
    if (days > 0) {
        snprintf(buf, buflen, "%dd %dh", days, hours);
    } else {
        snprintf(buf, buflen, "%dh %dm", hours, mins);
    }
}

static void draw_monitor(void) {
    int y = 0;
    char buf[64];
    
    /* Header */
    lcd_fill_rect(0, y, LCD_WIDTH, HEADER_HEIGHT, DARK_BLUE);
    lcd_print(4, y + 3, "Pi5 Monitor", WHITE, DARK_BLUE, 1);
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    lcd_print(LCD_WIDTH - 32, y + 3, buf, CYAN, DARK_BLUE, 1);
    
    y += HEADER_HEIGHT + 2;
    
    /* CPU */
    int cpu = get_cpu_usage();
    lcd_print(MARGIN, y, "CPU", WHITE, BLACK, 1);
    snprintf(buf, sizeof(buf), "%3d%%", cpu);
    lcd_print(LCD_WIDTH - 28, y, buf, get_level_color(cpu), BLACK, 1);
    y += 10;
    draw_bar(MARGIN, y, LCD_WIDTH - 2*MARGIN, BAR_HEIGHT, cpu, get_level_color(cpu), DARK_GRAY);
    y += BAR_HEIGHT + 6;
    
    /* Memory */
    int mem_used, mem_total;
    int mem = get_mem_usage(&mem_used, &mem_total);
    lcd_print(MARGIN, y, "MEM", WHITE, BLACK, 1);
    snprintf(buf, sizeof(buf), "%3d%%", mem);
    lcd_print(LCD_WIDTH - 28, y, buf, get_level_color(mem), BLACK, 1);
    y += 10;
    draw_bar(MARGIN, y, LCD_WIDTH - 2*MARGIN, BAR_HEIGHT, mem, get_level_color(mem), DARK_GRAY);
    y += BAR_HEIGHT + 2;
    snprintf(buf, sizeof(buf), "%dMB/%dMB", mem_used, mem_total);
    lcd_print(MARGIN, y, buf, LIGHT_GRAY, BLACK, 1);
    y += 12;
    
    /* Temperature */
    float temp = get_cpu_temp();
    int temp_pct = (int)((temp / 85.0f) * 100);
    if (temp_pct > 100) temp_pct = 100;
    lcd_print(MARGIN, y, "TEMP", WHITE, BLACK, 1);
    snprintf(buf, sizeof(buf), "%.1fC", temp);
    uint16_t temp_color = (temp < 50) ? GREEN : (temp < 70) ? YELLOW : RED;
    lcd_print(LCD_WIDTH - 36, y, buf, temp_color, BLACK, 1);
    y += 10;
    draw_bar(MARGIN, y, LCD_WIDTH - 2*MARGIN, BAR_HEIGHT, temp_pct, temp_color, DARK_GRAY);
    y += BAR_HEIGHT + 6;
    
    /* Disk */
    int disk_used, disk_total;
    int disk = get_disk_usage(&disk_used, &disk_total);
    lcd_print(MARGIN, y, "DISK", WHITE, BLACK, 1);
    snprintf(buf, sizeof(buf), "%3d%%", disk);
    lcd_print(LCD_WIDTH - 28, y, buf, get_level_color(disk), BLACK, 1);
    y += 10;
    draw_bar(MARGIN, y, LCD_WIDTH - 2*MARGIN, BAR_HEIGHT, disk, get_level_color(disk), DARK_GRAY);
    y += BAR_HEIGHT + 2;
    snprintf(buf, sizeof(buf), "%dGB/%dGB", disk_used, disk_total);
    lcd_print(MARGIN, y, buf, LIGHT_GRAY, BLACK, 1);
    y += 14;
    
    /* IP Address */
    char ip[32];
    get_ip_address(ip, sizeof(ip));
    lcd_print(MARGIN, y, "IP:", CYAN, BLACK, 1);
    lcd_print(MARGIN + 20, y, ip, WHITE, BLACK, 1);
    y += 12;
    
    /* Uptime */
    char uptime[32];
    get_uptime_str(uptime, sizeof(uptime));
    lcd_print(MARGIN, y, "UP:", CYAN, BLACK, 1);
    lcd_print(MARGIN + 20, y, uptime, WHITE, BLACK, 1);
}

static int hw_init(void) {
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) return -1;
    
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    
    struct gpiod_line_config *lcfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(lcfg, gpios, 3, settings);
    
    struct gpiod_request_config *rcfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(rcfg, "pi_monitor");
    
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
    printf("\nCleaning up...\n");
    gpio_set(PIN_BL, 1);
    if (gpio_req) gpiod_line_request_release(gpio_req);
    if (chip) gpiod_chip_close(chip);
    if (spi_fd >= 0) close(spi_fd);
}

static void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

/* Color test */
static void color_test(void) {
    printf("\nColor test - you should see:\n");
    printf("  1. RED\n");
    printf("  2. GREEN\n");
    printf("  3. BLUE\n");
    printf("  4. WHITE\n\n");
    
    printf("Showing RED...\n");
    lcd_fill(RED);
    sleep(2);
    
    printf("Showing GREEN...\n");
    lcd_fill(GREEN);
    sleep(2);
    
    printf("Showing BLUE...\n");
    lcd_fill(BLUE);
    sleep(2);
    
    printf("Showing WHITE...\n");
    lcd_fill(WHITE);
    sleep(2);
    
    printf("Color test complete.\n\n");
}

int main(int argc, char *argv[]) {
    int do_test = (argc > 1 && strcmp(argv[1], "-t") == 0);
    
    printf("==========================================\n");
    printf("Raspberry Pi 5 System Monitor\n");
    printf("WeAct ST7735S V1.5 (GREENTAB)\n");
    printf("==========================================\n\n");
    
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    
    if (hw_init() < 0) {
        printf("Hardware init failed!\n");
        cleanup();
        return 1;
    }
    
    lcd_init();
    gpio_set(PIN_BL, 0);  /* Backlight ON (active low) */
    
    if (do_test) {
        color_test();
    }
    
    get_cpu_usage();
    delay_ms(100);
    
    lcd_fill(BLACK);
    
    printf("Monitor running. Press Ctrl+C to exit.\n");
    printf("Run with -t for color test.\n\n");
    
    while (running) {
        draw_monitor();
        sleep(1);
    }
    
    cleanup();
    return 0;
}