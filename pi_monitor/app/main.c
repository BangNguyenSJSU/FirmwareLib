/**
 * @file main.c
 * @brief Pi System Monitor Main Application (Layer 3)
 * 
 * Build: make
 * Run:   sudo ./bin/pi_monitor
 */

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

/* Pin definitions */
#define PIN_DC   24
#define PIN_RST  25
#define PIN_BL   18

/* UI Layout */
#define HEADER_HEIGHT   14
#define BAR_HEIGHT      8
#define MARGIN          4
#define CHAR_WIDTH      6
#define CHAR_HEIGHT     8

static volatile int running = 1;
static gpio_handle_t gpio = NULL;
static spi_handle_t spi = NULL;
static st7735s_handle_t display = NULL;

static uint16_t get_level_color(int percent)
{
    if (percent < 50) return ST7735S_GREEN;
    if (percent < 75) return ST7735S_YELLOW;
    if (percent < 90) return ST7735S_ORANGE;
    return ST7735S_RED;
}

/* Draw fixed-width string (pads with spaces to prevent flicker) */
static void draw_fixed_string(int16_t x, int16_t y, const char* str, int max_chars,
                              uint16_t fg, uint16_t bg)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%-*s", max_chars, str);
    buf[max_chars] = '\0';
    st7735s_draw_string(display, x, y, buf, fg, bg, 1);
}

/* Draw static UI elements (called once at startup) */
static void draw_static_ui(void)
{
    int y = 0;
    uint16_t width = st7735s_get_width(display);
    
    /* Clear screen */
    st7735s_fill(display, ST7735S_BLACK);
    
    /* Header background */
    st7735s_fill_rect(display, 0, y, width, HEADER_HEIGHT, ST7735S_DARK_BLUE);
    st7735s_draw_string(display, 4, y + 3, "Pi5 Monitor", ST7735S_WHITE, ST7735S_DARK_BLUE, 1);
    
    y += HEADER_HEIGHT + 2;
    
    /* CPU label */
    st7735s_draw_string(display, MARGIN, y, "CPU", ST7735S_WHITE, ST7735S_BLACK, 1);
    y += 10 + BAR_HEIGHT + 6;
    
    /* Memory label */
    st7735s_draw_string(display, MARGIN, y, "MEM", ST7735S_WHITE, ST7735S_BLACK, 1);
    y += 10 + BAR_HEIGHT + 2;
    
    /* Memory size label position */
    y += 10 + 6;
    
    /* Disk label */
    st7735s_draw_string(display, MARGIN, y, "DISK", ST7735S_WHITE, ST7735S_BLACK, 1);
    y += 10 + BAR_HEIGHT + 6;
    
    /* Temperature label */
    st7735s_draw_string(display, MARGIN, y, "TEMP", ST7735S_WHITE, ST7735S_BLACK, 1);
    y += 10 + BAR_HEIGHT + 6;
    
    /* IP/Uptime labels */
    st7735s_draw_string(display, MARGIN, y, "IP:", ST7735S_WHITE, ST7735S_BLACK, 1);
    y += 10;
    st7735s_draw_string(display, MARGIN, y, "UP:", ST7735S_WHITE, ST7735S_BLACK, 1);
}

/* Draw dynamic values only (called every update cycle) */
static void draw_dynamic_values(void)
{
    int y = 0;
    char buf[64];
    uint16_t width = st7735s_get_width(display);
    
    /* Update time in header - use fixed width to overwrite */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    st7735s_draw_string(display, width - 32, 3, buf, ST7735S_CYAN, ST7735S_DARK_BLUE, 1);
    
    y = HEADER_HEIGHT + 2;
    
    /* CPU - fixed width percentage */
    cpu_info_t cpu;
    sysinfo_get_cpu(&cpu);
    snprintf(buf, sizeof(buf), "%3d%%", cpu.percent);
    st7735s_draw_string(display, width - 28, y, buf, get_level_color(cpu.percent), ST7735S_BLACK, 1);
    y += 10;
    st7735s_draw_progress_bar(display, MARGIN, y, width - 2*MARGIN, BAR_HEIGHT, 
                              cpu.percent, get_level_color(cpu.percent), ST7735S_DARK_GRAY, ST7735S_LIGHT_GRAY);
    y += BAR_HEIGHT + 6;
    
    /* Memory - fixed width percentage */
    mem_info_t mem;
    sysinfo_get_memory(&mem);
    snprintf(buf, sizeof(buf), "%3d%%", mem.percent);
    st7735s_draw_string(display, width - 28, y, buf, get_level_color(mem.percent), ST7735S_BLACK, 1);
    y += 10;
    st7735s_draw_progress_bar(display, MARGIN, y, width - 2*MARGIN, BAR_HEIGHT,
                              mem.percent, get_level_color(mem.percent), ST7735S_DARK_GRAY, ST7735S_LIGHT_GRAY);
    y += BAR_HEIGHT + 2;
    
    /* Memory details - fixed width */
    snprintf(buf, sizeof(buf), "%4dMB / %4dMB", mem.used_mb, mem.total_mb);
    draw_fixed_string(MARGIN, y, buf, 18, ST7735S_LIGHT_GRAY, ST7735S_BLACK);
    y += 10 + 6;
    
    /* Disk - fixed width percentage */
    disk_info_t disk;
    sysinfo_get_disk(&disk);
    snprintf(buf, sizeof(buf), "%3d%%", disk.percent);
    st7735s_draw_string(display, width - 28, y, buf, get_level_color(disk.percent), ST7735S_BLACK, 1);
    y += 10;
    st7735s_draw_progress_bar(display, MARGIN, y, width - 2*MARGIN, BAR_HEIGHT,
                              disk.percent, get_level_color(disk.percent), ST7735S_DARK_GRAY, ST7735S_LIGHT_GRAY);
    y += BAR_HEIGHT + 6;
    
    /* Temperature - fixed width */
    temp_info_t temp;
    sysinfo_get_temperature(&temp);
    int temp_percent = (temp.celsius > 80) ? 100 : (int)(temp.celsius * 100 / 80);
    snprintf(buf, sizeof(buf), "%5.1fC", temp.celsius);
    st7735s_draw_string(display, width - 38, y, buf, get_level_color(temp_percent), ST7735S_BLACK, 1);
    y += 10;
    st7735s_draw_progress_bar(display, MARGIN, y, width - 2*MARGIN, BAR_HEIGHT,
                              temp_percent, get_level_color(temp_percent), ST7735S_DARK_GRAY, ST7735S_LIGHT_GRAY);
    y += BAR_HEIGHT + 6;
    
    /* IP Address - fixed width */
    char ip[32];
    sysinfo_get_ip(ip, sizeof(ip));
    draw_fixed_string(MARGIN + 18, y, ip, 16, ST7735S_CYAN, ST7735S_BLACK);
    y += 10;
    
    /* Uptime - fixed width */
    char uptime[32];
    sysinfo_get_uptime(uptime, sizeof(uptime));
    draw_fixed_string(MARGIN + 18, y, uptime, 16, ST7735S_MAGENTA, ST7735S_BLACK);
}

static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

static int init_hardware(void)
{
    /* Initialize GPIO */
    if (gpio_init("/dev/gpiochip4", &gpio) != GPIO_OK) {
        fprintf(stderr, "Failed to open GPIO\n");
        return -1;
    }
    
    /* Configure GPIO pins */
    gpio_config_t pins[] = {
        { .pin = PIN_DC,  .direction = GPIO_DIR_OUTPUT, .initial_state = GPIO_STATE_LOW },
        { .pin = PIN_RST, .direction = GPIO_DIR_OUTPUT, .initial_state = GPIO_STATE_HIGH },
        { .pin = PIN_BL,  .direction = GPIO_DIR_OUTPUT, .initial_state = GPIO_STATE_HIGH }
    };
    if (gpio_configure(gpio, pins, 3, "pi_monitor") != GPIO_OK) {
        fprintf(stderr, "Failed to configure GPIO pins\n");
        return -1;
    }
    
    /* Initialize SPI */
    spi_config_t spi_cfg = {
        .device = "/dev/spidev0.0",
        .speed_hz = 32000000,
        .mode = SPI_MODE_0,
        .bits_per_word = 8,
        .bit_order = SPI_BIT_ORDER_MSB_FIRST
    };
    if (spi_init(&spi_cfg, &spi) != SPI_OK) {
        fprintf(stderr, "Failed to open SPI\n");
        return -1;
    }
    
    /* Initialize display */
    st7735s_config_t cfg = ST7735S_CONFIG_WEACT_DEFAULT;
    if (st7735s_init(gpio, spi, &cfg, &display) != ST7735S_OK) {
        fprintf(stderr, "Failed to initialize display\n");
        return -1;
    }
    
    /* Turn on backlight (active low) */
    gpio_write(gpio, PIN_BL, GPIO_STATE_LOW);
    
    return 0;
}

static void cleanup_hardware(void)
{
    if (display) {
        gpio_write(gpio, PIN_BL, GPIO_STATE_HIGH);
        st7735s_fill(display, ST7735S_BLACK);
        st7735s_deinit(display);
    }
    if (spi) spi_deinit(spi);
    if (gpio) gpio_deinit(gpio);
}

int main(void)
{
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("Pi System Monitor v1.0\n");
    printf("Press Ctrl+C to exit\n");
    
    if (init_hardware() < 0) {
        cleanup_hardware();
        return 1;
    }
    
    sysinfo_init();
    draw_static_ui();
    
    while (running) {
        draw_dynamic_values();
        usleep(1000000);
    }
    
    printf("\nShutting down...\n");
    cleanup_hardware();
    return 0;
}
