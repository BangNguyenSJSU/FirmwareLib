/*
 * Raspberry Pi 5 System Monitor
 * For WeAct ST7735S 128x160 LCD
 * 
 * Displays:
 *   - CPU usage (bar + percentage)
 *   - Memory usage (bar + percentage)
 *   - CPU temperature
 *   - Disk usage
 *   - Network IP address
 *   - Uptime
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
#define SPI_SPEED   32000000

/* Display */
#define LCD_WIDTH   128
#define LCD_HEIGHT  160
#define COL_OFFSET  2
#define ROW_OFFSET  1

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

/* Previous CPU stats for calculation */
static unsigned long prev_idle = 0, prev_total = 0;

/* 5x7 font - extended with numbers and symbols */
static const uint8_t font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /*   0 space */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! 1 */
    {0x00,0x07,0x00,0x07,0x00}, /* " 2 */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # 3 */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ 4 */
    {0x23,0x13,0x08,0x64,0x62}, /* % 5 */
    {0x36,0x49,0x55,0x22,0x50}, /* & 6 */
    {0x00,0x05,0x03,0x00,0x00}, /* ' 7 */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( 8 */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) 9 */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* * 10 */
    {0x08,0x08,0x3E,0x08,0x08}, /* + 11 */
    {0x00,0x50,0x30,0x00,0x00}, /* , 12 */
    {0x08,0x08,0x08,0x08,0x08}, /* - 13 */
    {0x00,0x60,0x60,0x00,0x00}, /* . 14 */
    {0x20,0x10,0x08,0x04,0x02}, /* / 15 */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 16 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 17 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 18 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 19 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 20 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 21 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 22 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 23 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 24 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 25 */
    {0x00,0x36,0x36,0x00,0x00}, /* : 26 */
    {0x00,0x56,0x36,0x00,0x00}, /* ; 27 */
    {0x00,0x08,0x14,0x22,0x41}, /* < 28 */
    {0x14,0x14,0x14,0x14,0x14}, /* = 29 */
    {0x41,0x22,0x14,0x08,0x00}, /* > 30 */
    {0x02,0x01,0x51,0x09,0x06}, /* ? 31 */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ 32 */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A 33 */
    {0x7F,0x49,0x49,0x49,0x36}, /* B 34 */
    {0x3E,0x41,0x41,0x41,0x22}, /* C 35 */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D 36 */
    {0x7F,0x49,0x49,0x49,0x41}, /* E 37 */
    {0x7F,0x09,0x09,0x09,0x01}, /* F 38 */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G 39 */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H 40 */
    {0x00,0x41,0x7F,0x41,0x00}, /* I 41 */
    {0x20,0x40,0x41,0x3F,0x01}, /* J 42 */
    {0x7F,0x08,0x14,0x22,0x41}, /* K 43 */
    {0x7F,0x40,0x40,0x40,0x40}, /* L 44 */
    {0x7F,0x02,0x04,0x02,0x7F}, /* M 45 */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N 46 */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O 47 */
    {0x7F,0x09,0x09,0x09,0x06}, /* P 48 */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q 49 */
    {0x7F,0x09,0x19,0x29,0x46}, /* R 50 */
    {0x46,0x49,0x49,0x49,0x31}, /* S 51 */
    {0x01,0x01,0x7F,0x01,0x01}, /* T 52 */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U 53 */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V 54 */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W 55 */
    {0x63,0x14,0x08,0x14,0x63}, /* X 56 */
    {0x07,0x08,0x70,0x08,0x07}, /* Y 57 */
    {0x61,0x51,0x49,0x45,0x43}, /* Z 58 */
    {0x00,0x7F,0x41,0x41,0x00}, /* [ 59 */
    {0x02,0x04,0x08,0x10,0x20}, /* \ 60 */
    {0x00,0x41,0x41,0x7F,0x00}, /* ] 61 */
    {0x04,0x02,0x01,0x02,0x04}, /* ^ 62 */
    {0x40,0x40,0x40,0x40,0x40}, /* _ 63 */
    {0x00,0x01,0x02,0x04,0x00}, /* ` 64 */
    {0x20,0x54,0x54,0x54,0x78}, /* a 65 */
    {0x7F,0x48,0x44,0x44,0x38}, /* b 66 */
    {0x38,0x44,0x44,0x44,0x20}, /* c 67 */
    {0x38,0x44,0x44,0x48,0x7F}, /* d 68 */
    {0x38,0x54,0x54,0x54,0x18}, /* e 69 */
    {0x08,0x7E,0x09,0x01,0x02}, /* f 70 */
    {0x08,0x14,0x54,0x54,0x3C}, /* g 71 */
    {0x7F,0x08,0x04,0x04,0x78}, /* h 72 */
    {0x00,0x44,0x7D,0x40,0x00}, /* i 73 */
    {0x20,0x40,0x44,0x3D,0x00}, /* j 74 */
    {0x7F,0x10,0x28,0x44,0x00}, /* k 75 */
    {0x00,0x41,0x7F,0x40,0x00}, /* l 76 */
    {0x7C,0x04,0x18,0x04,0x78}, /* m 77 */
    {0x7C,0x08,0x04,0x04,0x78}, /* n 78 */
    {0x38,0x44,0x44,0x44,0x38}, /* o 79 */
    {0x7C,0x14,0x14,0x14,0x08}, /* p 80 */
    {0x08,0x14,0x14,0x18,0x7C}, /* q 81 */
    {0x7C,0x08,0x04,0x04,0x08}, /* r 82 */
    {0x48,0x54,0x54,0x54,0x20}, /* s 83 */
    {0x04,0x3F,0x44,0x40,0x20}, /* t 84 */
    {0x3C,0x40,0x40,0x20,0x7C}, /* u 85 */
    {0x1C,0x20,0x40,0x20,0x1C}, /* v 86 */
    {0x3C,0x40,0x30,0x40,0x3C}, /* w 87 */
    {0x44,0x28,0x10,0x28,0x44}, /* x 88 */
    {0x0C,0x50,0x50,0x50,0x3C}, /* y 89 */
    {0x44,0x64,0x54,0x4C,0x44}, /* z 90 */
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
    gpio_set(PIN_RST, 1); delay_ms(50);
    gpio_set(PIN_RST, 0); delay_ms(50);
    gpio_set(PIN_RST, 1); delay_ms(150);
    
    lcd_cmd(0x01); delay_ms(150);
    lcd_cmd(0x11); delay_ms(255);
    
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
    
    lcd_cmd(0x21);  /* INVON */
    lcd_cmd(0x36); lcd_data(0x00);
    lcd_cmd(0x3A); lcd_data(0x05);
    
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

/* Draw a progress bar */
static void draw_bar(int x, int y, int w, int h, int percent, uint16_t fg, uint16_t bg) {
    /* Background */
    lcd_fill_rect(x, y, w, h, bg);
    
    /* Filled portion */
    int fill_w = (w * percent) / 100;
    if (fill_w > 0) {
        lcd_fill_rect(x, y, fill_w, h, fg);
    }
    
    /* Border */
    lcd_draw_hline(x, y, w, LIGHT_GRAY);
    lcd_draw_hline(x, y + h - 1, w, LIGHT_GRAY);
}

/* Get color based on percentage (green->yellow->red) */
static uint16_t get_level_color(int percent) {
    if (percent < 50) return GREEN;
    if (percent < 75) return YELLOW;
    if (percent < 90) return ORANGE;
    return RED;
}

/* Get CPU usage */
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

/* Get memory usage */
static int get_mem_usage(int *used_mb, int *total_mb) {
    struct sysinfo si;
    if (sysinfo(&si) != 0) return 0;
    
    unsigned long total = si.totalram / (1024 * 1024);
    unsigned long free = si.freeram / (1024 * 1024);
    unsigned long buffers = si.bufferram / (1024 * 1024);
    
    /* Get cached memory from /proc/meminfo */
    unsigned long cached = 0;
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "Cached:", 7) == 0) {
                sscanf(line, "Cached: %lu", &cached);
                cached /= 1024;  /* Convert to MB */
                break;
            }
        }
        fclose(fp);
    }
    
    unsigned long used = total - free - buffers - cached;
    
    *total_mb = (int)total;
    *used_mb = (int)used;
    
    return (int)(100 * used / total);
}

/* Get CPU temperature */
static float get_cpu_temp(void) {
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!fp) return 0.0f;
    
    int temp;
    if (fscanf(fp, "%d", &temp) != 1) temp = 0;
    fclose(fp);
    
    return temp / 1000.0f;
}

/* Get disk usage */
static int get_disk_usage(int *used_gb, int *total_gb) {
    struct statvfs st;
    if (statvfs("/", &st) != 0) return 0;
    
    unsigned long total = (st.f_blocks * st.f_frsize) / (1024 * 1024 * 1024);
    unsigned long free = (st.f_bfree * st.f_frsize) / (1024 * 1024 * 1024);
    unsigned long used = total - free;
    
    *total_gb = (int)total;
    *used_gb = (int)used;
    
    if (total == 0) return 0;
    return (int)(100 * used / total);
}

/* Get IP address */
static void get_ip_address(char *buf, int buflen) {
    struct ifaddrs *ifaddr, *ifa;
    
    strcpy(buf, "No IP");
    
    if (getifaddrs(&ifaddr) == -1) return;
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        if (ifa->ifa_addr->sa_family != AF_INET) continue;
        
        /* Skip loopback */
        if (strcmp(ifa->ifa_name, "lo") == 0) continue;
        
        struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
        inet_ntop(AF_INET, &addr->sin_addr, buf, buflen);
        break;
    }
    
    freeifaddrs(ifaddr);
}

/* Get uptime string */
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

/* Draw the monitor screen */
static void draw_monitor(void) {
    int y = 0;
    char buf[64];
    
    /* Header */
    lcd_fill_rect(0, y, LCD_WIDTH, HEADER_HEIGHT, DARK_BLUE);
    lcd_print(4, y + 3, "Pi5 Monitor", WHITE, DARK_BLUE, 1);
    
    /* Get current time */
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
    gpio_set(PIN_BL, 1);  /* Backlight off */
    if (gpio_req) gpiod_line_request_release(gpio_req);
    if (chip) gpiod_chip_close(chip);
    if (spi_fd >= 0) close(spi_fd);
}

static void sigint_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(void) {
    printf("Raspberry Pi 5 System Monitor\n");
    printf("Press Ctrl+C to exit\n\n");
    
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);
    
    if (hw_init() < 0) {
        printf("Hardware init failed!\n");
        cleanup();
        return 1;
    }
    
    lcd_init();
    gpio_set(PIN_BL, 0);  /* Backlight ON (active low) */
    
    /* Initial CPU reading to prime the delta calculation */
    get_cpu_usage();
    delay_ms(100);
    
    /* Clear screen */
    lcd_fill(BLACK);
    
    while (running) {
        draw_monitor();
        sleep(1);
    }
    
    cleanup();
    return 0;
}
