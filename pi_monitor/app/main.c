#include "gpio_driver.h"
#include "spi_driver.h"
#include "st7735s.h"
#include "system_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>

#define PIN_DC   24
#define PIN_RST  25
#define PIN_BL   18
#define HEADER_HEIGHT 14
#define BAR_HEIGHT    6
#define MARGIN        4
#define ROW_GAP       4

static volatile int running = 1;
static gpio_handle_t gpio = NULL;
static spi_handle_t spi = NULL;
static st7735s_handle_t display = NULL;

static uint16_t get_level_color(int p) {
    if (p < 50) return ST7735S_GREEN;
    if (p < 75) return ST7735S_YELLOW;
    if (p < 90) return ST7735S_ORANGE;
    return ST7735S_RED;
}

static uint16_t get_fan_color(int rpm) {
    if (rpm == 0) return ST7735S_GRAY;
    if (rpm < 3000) return ST7735S_GREEN;
    if (rpm < 5000) return ST7735S_YELLOW;
    return ST7735S_ORANGE;
}

static void draw_fixed_string(int16_t x, int16_t y, const char* str, int len, uint16_t fg, uint16_t bg) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%-*s", len, str);
    buf[len] = '\0';
    st7735s_draw_string(display, x, y, buf, fg, bg, 1);
}

static void draw_static_ui(void) {
    int y = 0;
    uint16_t w = st7735s_get_width(display);
    st7735s_fill(display, ST7735S_BLACK);
    st7735s_fill_rect(display, 0, y, w, HEADER_HEIGHT, ST7735S_DARK_BLUE);
    st7735s_draw_string(display, 4, y+3, "Pi5 Monitor", ST7735S_WHITE, ST7735S_DARK_BLUE, 1);
    y += HEADER_HEIGHT + 2;
    st7735s_draw_string(display, MARGIN, y, "CPU", ST7735S_WHITE, ST7735S_BLACK, 1);
    y += 9 + BAR_HEIGHT + ROW_GAP;
    st7735s_draw_string(display, MARGIN, y, "MEM", ST7735S_WHITE, ST7735S_BLACK, 1);
    y += 9 + BAR_HEIGHT + ROW_GAP;
    st7735s_draw_string(display, MARGIN, y, "DISK", ST7735S_WHITE, ST7735S_BLACK, 1);
    y += 9 + BAR_HEIGHT + ROW_GAP;
    st7735s_draw_string(display, MARGIN, y, "TEMP", ST7735S_WHITE, ST7735S_BLACK, 1);
    y += 9 + BAR_HEIGHT + ROW_GAP;
    st7735s_draw_string(display, MARGIN, y, "FAN", ST7735S_WHITE, ST7735S_BLACK, 1);
    y += 9 + BAR_HEIGHT + ROW_GAP;
    st7735s_draw_hline(display, MARGIN, y, w - 2*MARGIN, ST7735S_DARK_GRAY);
    y += 4;
    st7735s_draw_string(display, MARGIN, y, "IP:", ST7735S_WHITE, ST7735S_BLACK, 1);
    y += 10;
    st7735s_draw_string(display, MARGIN, y, "UP:", ST7735S_WHITE, ST7735S_BLACK, 1);
}

static void draw_dynamic_values(void) {
    int y = 0;
    char buf[64];
    uint16_t w = st7735s_get_width(display);
    
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    st7735s_draw_string(display, w - 32, 3, buf, ST7735S_CYAN, ST7735S_DARK_BLUE, 1);
    
    y = HEADER_HEIGHT + 2;
    
    cpu_info_t cpu; sysinfo_get_cpu(&cpu);
    snprintf(buf, sizeof(buf), "%3d%%", cpu.percent);
    st7735s_draw_string(display, w - 28, y, buf, get_level_color(cpu.percent), ST7735S_BLACK, 1);
    y += 9;
    st7735s_draw_progress_bar(display, MARGIN, y, w - 2*MARGIN, BAR_HEIGHT, cpu.percent, get_level_color(cpu.percent), ST7735S_DARK_GRAY, ST7735S_LIGHT_GRAY);
    y += BAR_HEIGHT + ROW_GAP;
    
    mem_info_t mem; sysinfo_get_memory(&mem);
    snprintf(buf, sizeof(buf), "%3d%%", mem.percent);
    st7735s_draw_string(display, w - 28, y, buf, get_level_color(mem.percent), ST7735S_BLACK, 1);
    y += 9;
    st7735s_draw_progress_bar(display, MARGIN, y, w - 2*MARGIN, BAR_HEIGHT, mem.percent, get_level_color(mem.percent), ST7735S_DARK_GRAY, ST7735S_LIGHT_GRAY);
    y += BAR_HEIGHT + ROW_GAP;
    
    disk_info_t disk; sysinfo_get_disk(&disk);
    snprintf(buf, sizeof(buf), "%3d%%", disk.percent);
    st7735s_draw_string(display, w - 28, y, buf, get_level_color(disk.percent), ST7735S_BLACK, 1);
    y += 9;
    st7735s_draw_progress_bar(display, MARGIN, y, w - 2*MARGIN, BAR_HEIGHT, disk.percent, get_level_color(disk.percent), ST7735S_DARK_GRAY, ST7735S_LIGHT_GRAY);
    y += BAR_HEIGHT + ROW_GAP;
    
    temp_info_t temp; sysinfo_get_temperature(&temp);
    int tp = (temp.celsius > 80) ? 100 : (int)(temp.celsius * 100 / 80);
    snprintf(buf, sizeof(buf), "%4.1fC", temp.celsius);
    st7735s_draw_string(display, w - 36, y, buf, get_level_color(tp), ST7735S_BLACK, 1);
    y += 9;
    st7735s_draw_progress_bar(display, MARGIN, y, w - 2*MARGIN, BAR_HEIGHT, tp, get_level_color(tp), ST7735S_DARK_GRAY, ST7735S_LIGHT_GRAY);
    y += BAR_HEIGHT + ROW_GAP;
    
    fan_info_t fan; sysinfo_get_fan(&fan);
    if (fan.rpm > 0) snprintf(buf, sizeof(buf), "%4dRPM", fan.rpm);
    else snprintf(buf, sizeof(buf), "    OFF");
    st7735s_draw_string(display, w - 48, y, buf, get_fan_color(fan.rpm), ST7735S_BLACK, 1);
    y += 9;
    st7735s_draw_progress_bar(display, MARGIN, y, w - 2*MARGIN, BAR_HEIGHT, fan.percent, get_fan_color(fan.rpm), ST7735S_DARK_GRAY, ST7735S_LIGHT_GRAY);
    y += BAR_HEIGHT + ROW_GAP + 4;
    
    char ip[32]; sysinfo_get_ip(ip, sizeof(ip));
    draw_fixed_string(MARGIN + 18, y, ip, 16, ST7735S_CYAN, ST7735S_BLACK);
    y += 10;
    char up[32]; sysinfo_get_uptime(up, sizeof(up));
    draw_fixed_string(MARGIN + 18, y, up, 16, ST7735S_MAGENTA, ST7735S_BLACK);
}

static void signal_handler(int sig) { (void)sig; running = 0; }

static int init_hardware(void) {
    if (gpio_init("/dev/gpiochip4", &gpio) != GPIO_OK) { fprintf(stderr, "GPIO fail\n"); return -1; }
    gpio_config_t pins[] = {
        { .pin = PIN_DC, .direction = GPIO_DIR_OUTPUT, .initial_state = GPIO_STATE_LOW },
        { .pin = PIN_RST, .direction = GPIO_DIR_OUTPUT, .initial_state = GPIO_STATE_HIGH },
        { .pin = PIN_BL, .direction = GPIO_DIR_OUTPUT, .initial_state = GPIO_STATE_HIGH }
    };
    if (gpio_configure(gpio, pins, 3, "pi_monitor") != GPIO_OK) { fprintf(stderr, "GPIO cfg fail\n"); return -1; }
    spi_config_t spi_cfg = { .device = "/dev/spidev0.0", .speed_hz = 32000000, .mode = SPI_MODE_0, .bits_per_word = 8, .bit_order = SPI_BIT_ORDER_MSB_FIRST };
    if (spi_init(&spi_cfg, &spi) != SPI_OK) { fprintf(stderr, "SPI fail\n"); return -1; }
    st7735s_config_t cfg = ST7735S_CONFIG_WEACT_DEFAULT;
    if (st7735s_init(gpio, spi, &cfg, &display) != ST7735S_OK) { fprintf(stderr, "Display fail\n"); return -1; }
    gpio_write(gpio, PIN_BL, GPIO_STATE_LOW);
    return 0;
}

static void cleanup_hardware(void) {
    if (display) { gpio_write(gpio, PIN_BL, GPIO_STATE_HIGH); st7735s_fill(display, ST7735S_BLACK); st7735s_deinit(display); }
    if (spi) spi_deinit(spi);
    if (gpio) gpio_deinit(gpio);
}

int main(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    printf("Pi System Monitor v1.1\nPress Ctrl+C to exit\n");
    if (init_hardware() < 0) { cleanup_hardware(); return 1; }
    sysinfo_init();
    draw_static_ui();
    while (running) { draw_dynamic_values(); usleep(1000000); }
    printf("\nShutting down...\n");
    cleanup_hardware();
    return 0;
}
