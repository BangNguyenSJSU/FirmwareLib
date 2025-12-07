/*
 * ST7735 Minimal Debug Test
 * Tests each hardware component step by step
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <gpiod.h>
#include <time.h>

/* Pin definitions */
#define PIN_DC      24
#define PIN_RST     25
#define PIN_BL      18

#define SPI_DEVICE  "/dev/spidev0.0"
#define SPI_SPEED   4000000   /* Start slow: 4 MHz */

/* Globals */
static int spi_fd = -1;
static struct gpiod_chip *chip = NULL;
static struct gpiod_line_request *gpio_req = NULL;
static const unsigned int gpios[] = {PIN_DC, PIN_RST, PIN_BL};

static void delay_ms(int ms) {
    usleep(ms * 1000);
}

static void gpio_set(int pin, int val) {
    gpiod_line_request_set_value(gpio_req, pin,
        val ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
}

static void spi_write_byte(uint8_t b) {
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)&b,
        .len = 1,
        .speed_hz = SPI_SPEED,
        .bits_per_word = 8,
    };
    ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
}

static void lcd_cmd(uint8_t cmd) {
    gpio_set(PIN_DC, 0);  /* Command mode */
    spi_write_byte(cmd);
}

static void lcd_data(uint8_t data) {
    gpio_set(PIN_DC, 1);  /* Data mode */
    spi_write_byte(data);
}

int main(void) {
    printf("=== ST7735 Debug Test ===\n\n");
    
    /* Step 1: Open GPIO */
    printf("[1] Opening GPIO chip...\n");
    chip = gpiod_chip_open("/dev/gpiochip4");
    if (!chip) {
        chip = gpiod_chip_open("/dev/gpiochip0");
    }
    if (!chip) {
        printf("    FAILED: Cannot open GPIO chip\n");
        printf("    Try: ls -la /dev/gpiochip*\n");
        return 1;
    }
    printf("    OK: GPIO chip opened\n");
    
    /* Step 2: Request GPIO lines */
    printf("[2] Requesting GPIO lines (DC=%d, RST=%d, BL=%d)...\n", PIN_DC, PIN_RST, PIN_BL);
    
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(settings, GPIOD_LINE_VALUE_INACTIVE);
    
    struct gpiod_line_config *line_cfg = gpiod_line_config_new();
    gpiod_line_config_add_line_settings(line_cfg, gpios, 3, settings);
    
    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    gpiod_request_config_set_consumer(req_cfg, "st7735_debug");
    
    gpio_req = gpiod_chip_request_lines(chip, req_cfg, line_cfg);
    
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    
    if (!gpio_req) {
        printf("    FAILED: Cannot request GPIO lines\n");
        printf("    Check if another process is using these pins\n");
        gpiod_chip_close(chip);
        return 1;
    }
    printf("    OK: GPIO lines acquired\n");
    
    /* Step 3: Open SPI */
    printf("[3] Opening SPI device %s...\n", SPI_DEVICE);
    spi_fd = open(SPI_DEVICE, O_RDWR);
    if (spi_fd < 0) {
        printf("    FAILED: Cannot open SPI device\n");
        printf("    Enable SPI: sudo raspi-config -> Interface Options -> SPI\n");
        gpiod_line_request_release(gpio_req);
        gpiod_chip_close(chip);
        return 1;
    }
    
    uint8_t mode = 0;
    uint32_t speed = SPI_SPEED;
    ioctl(spi_fd, SPI_IOC_WR_MODE, &mode);
    ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
    printf("    OK: SPI opened at %d Hz\n", speed);
    
    /* Step 4: Test backlight */
    printf("[4] Testing backlight...\n");
    printf("    Setting BL HIGH (if display uses active-high)...\n");
    gpio_set(PIN_BL, 1);
    delay_ms(2500);
    printf("    Setting BL LOW (if display uses active-low)...\n");
    gpio_set(PIN_BL, 0);
    delay_ms(2500);
    printf("    Did backlight turn on at any point? (remember which)\n");
    printf("    Press Enter to continue...");
    getchar();
    
    /* Keep backlight on - try LOW first (WeAct is active-low) */
    gpio_set(PIN_BL, 0);
    
    /* Step 5: Hardware reset */
    printf("[5] Performing hardware reset...\n");
    gpio_set(PIN_RST, 1);
    delay_ms(50);
    printf("    RST -> LOW\n");
    gpio_set(PIN_RST, 0);
    delay_ms(50);
    printf("    RST -> HIGH\n");
    gpio_set(PIN_RST, 1);
    delay_ms(150);
    printf("    OK: Reset complete\n");
    
    /* Step 6: Send init commands */
    printf("[6] Sending initialization commands...\n");
    
    printf("    SWRESET...\n");
    lcd_cmd(0x01);  /* Software reset */
    delay_ms(150);
    
    printf("    SLPOUT...\n");
    lcd_cmd(0x11);  /* Sleep out */
    delay_ms(255);
    
    printf("    INVON (for WeAct)...\n");
    lcd_cmd(0x21);  /* Inversion ON */
    
    printf("    MADCTL...\n");
    lcd_cmd(0x36);  /* Memory access control */
    lcd_data(0x00);
    
    printf("    COLMOD (16-bit color)...\n");
    lcd_cmd(0x3A);  /* Color mode */
    lcd_data(0x05); /* 16-bit */
    
    printf("    NORON...\n");
    lcd_cmd(0x13);  /* Normal display on */
    delay_ms(10);
    
    printf("    DISPON...\n");
    lcd_cmd(0x29);  /* Display on */
    delay_ms(100);
    
    printf("    OK: Init commands sent\n");
    
    /* Step 7: Try to fill screen with RED */
    printf("[7] Filling screen with RED...\n");
    
    /* Set column address (with offset 2 for WeAct) */
    lcd_cmd(0x2A);
    lcd_data(0x00);
    lcd_data(0x02);      /* Start col = 2 */
    lcd_data(0x00);
    lcd_data(0x81);      /* End col = 129 (2+127) */
    
    /* Set row address (with offset 1 for WeAct) */
    lcd_cmd(0x2B);
    lcd_data(0x00);
    lcd_data(0x01);      /* Start row = 1 */
    lcd_data(0x00);
    lcd_data(0xA0);      /* End row = 160 (1+159) */
    
    /* Write pixels */
    lcd_cmd(0x2C);
    gpio_set(PIN_DC, 1);
    
    /* RED in RGB565 = 0xF800 */
    uint8_t red_hi = 0xF8;
    uint8_t red_lo = 0x00;
    
    printf("    Writing %d pixels...\n", 128 * 160);
    for (int i = 0; i < 128 * 160; i++) {
        spi_write_byte(red_hi);
        spi_write_byte(red_lo);
    }
    printf("    Done.\n");
    
    printf("\n=== Results ===\n");
    printf("Do you see a RED screen? (y/n): ");
    fflush(stdout);
    char c = getchar();
    
    if (c == 'y' || c == 'Y') {
        printf("\nSUCCESS! The display is working.\n");
        printf("The issue was likely in the previous code's timing or init sequence.\n");
    } else {
        printf("\nTroubleshooting:\n");
        printf("1. Check wiring - especially DC, RST, and SPI connections\n");
        printf("2. Verify CS is connected to GPIO8 (CE0)\n");
        printf("3. Try swapping MOSI/SCK if reversed\n");
        printf("4. Check voltage - must be 3.3V, not 5V!\n");
        printf("5. Some ST7735 variants need INVOFF instead of INVON\n");
        printf("\nPin check:\n");
        printf("  VCC  -> 3.3V (Pin 1 or 17)\n");
        printf("  GND  -> GND (Pin 6)\n");
        printf("  SCL  -> GPIO11 (Pin 23)\n");
        printf("  SDA  -> GPIO10 (Pin 19)\n");
        printf("  CS   -> GPIO8 (Pin 24)\n");
        printf("  DC   -> GPIO24 (Pin 18)\n");
        printf("  RST  -> GPIO25 (Pin 22)\n");
        printf("  BL   -> GPIO18 (Pin 12)\n");
    }
    
    printf("\nPress Enter to exit and cleanup...");
    while (getchar() != '\n');
    getchar();
    
    /* Cleanup */
    gpio_set(PIN_BL, 1);  /* Turn off (active low) */
    close(spi_fd);
    gpiod_line_request_release(gpio_req);
    gpiod_chip_close(chip);
    
    return 0;
}
