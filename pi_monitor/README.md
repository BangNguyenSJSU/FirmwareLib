# Pi System Monitor

A real-time system monitor for Raspberry Pi 5 with ST7735S TFT LCD display, built with a professional layered firmware architecture.

## Table of Contents

- [Overview](#overview)
- [Project Structure](#project-structure)
- [Architecture](#architecture)
- [Layer Details](#layer-details)
- [Dependencies](#dependencies)
- [Building](#building)
- [Installation](#installation)
- [Running](#running)
- [Makefile Reference](#makefile-reference)
- [Hardware Setup](#hardware-setup)
- [API Reference](#api-reference)
- [Troubleshooting](#troubleshooting)

## Overview

Pi Monitor displays real-time system statistics on a 128x160 ST7735S TFT LCD:
- CPU usage with color-coded progress bar
- Memory usage and capacity
- Disk usage
- CPU temperature
- IP address and uptime

The codebase follows a 3-layer architecture for maintainability, portability, and testability.

## Project Structure

```
pi_monitor/
+-- include/                    # Public header files
|   +-- gpio_driver.h           # GPIO driver interface
|   +-- spi_driver.h            # SPI driver interface
|   +-- st7735s.h               # Display driver interface
|   +-- st7735s_colors.h        # RGB565 color definitions
|   +-- fonts.h                 # Font rendering interface
|   +-- system_info.h           # System information interface
|
+-- drivers/                    # Layer 1: Hardware Abstraction
|   +-- gpio_driver.c           # GPIO implementation (libgpiod v2)
|   +-- spi_driver.c            # SPI implementation (Linux spidev)
|
+-- services/                   # Layer 2: Device Services
|   +-- st7735s.c               # ST7735S display driver
|   +-- fonts.c                 # 5x7 bitmap font data
|
+-- app/                        # Layer 3: Application
|   +-- main.c                  # Main application entry point
|   +-- system_info.c           # Linux system info collectors
|
+-- build/                      # Compiled object files (generated)
+-- bin/                        # Output binary (generated)
+-- Makefile                    # Build system
+-- README.md                   # This file
```

## Architecture

The firmware follows a strict 3-layer architecture where each layer only depends on layers below it:

```
+---------------------------------------------------------------+
|                      APPLICATION LAYER                        |
|                         (Layer 3)                             |
|  +-------------------+  +-----------------------------------+ |
|  |     main.c        |  |       system_info.c               | |
|  |  - UI composition |  |  - /proc/stat CPU parsing         | |
|  |  - Main loop      |  |  - /proc/meminfo parsing          | |
|  |  - Signal handler |  |  - /sys/class/thermal reading     | |
|  |  - HW init/deinit |  |  - statvfs() disk usage           | |
|  +-------------------+  +-----------------------------------+ |
+---------------------------------------------------------------+
|                       SERVICE LAYER                           |
|                         (Layer 2)                             |
|  +---------------------------------------------------------+  |
|  |                 st7735s.c / st7735s.h                   |  |
|  |  - Display initialization & command sequences          |  |
|  |  - Drawing primitives (pixel, rect, line)              |  |
|  |  - Text rendering with scalable fonts                  |  |
|  |  - Progress bar widget                                 |  |
|  |  - Color management (RGB565)                           |  |
|  +---------------------------------------------------------+  |
|  +---------------------------------------------------------+  |
|  |                 fonts.c / fonts.h                       |  |
|  |  - 5x7 pixel bitmap font (ASCII 32-126)                |  |
|  |  - Character lookup function                           |  |
|  +---------------------------------------------------------+  |
+---------------------------------------------------------------+
|                       DRIVER LAYER                            |
|                         (Layer 1)                             |
|  +-------------------------+  +-----------------------------+ |
|  |    gpio_driver.c        |  |       spi_driver.c          | |
|  |  - libgpiod v2 wrapper  |  |  - Linux spidev wrapper     | |
|  |  - Pin configuration    |  |  - Full-duplex transfer     | |
|  |  - Read/Write/Toggle    |  |  - Speed/mode configuration | |
|  |  - Multi-pin operations |  |  - Byte and block writes    | |
|  +-------------------------+  +-----------------------------+ |
+---------------------------------------------------------------+
|                         HARDWARE                              |
|              Raspberry Pi 5 + WeAct ST7735S LCD               |
+---------------------------------------------------------------+
```

### Design Principles

1. **Separation of Concerns**: Each layer has a single responsibility
2. **Dependency Inversion**: Upper layers depend on abstractions (headers), not implementations
3. **Portability**: Replace Layer 1 drivers to port to different platforms
4. **Testability**: Each layer can be unit tested independently
5. **Encapsulation**: Internal state hidden via opaque handle pointers

## Layer Details

### Layer 1: Driver Layer (Hardware Abstraction)

#### gpio_driver.c / gpio_driver.h

GPIO abstraction using libgpiod v2 API for Raspberry Pi 5.

| Function | Description |
|----------|-------------|
| `gpio_init()` | Open GPIO chip device |
| `gpio_configure()` | Configure pins (direction, pull, initial state) |
| `gpio_write()` | Set pin output state |
| `gpio_read()` | Read pin input state |
| `gpio_toggle()` | Toggle pin output |
| `gpio_write_multiple()` | Atomic multi-pin write |
| `gpio_deinit()` | Release GPIO resources |

**Key types:**
- `gpio_handle_t` - Opaque handle to GPIO context
- `gpio_config_t` - Pin configuration structure
- `gpio_status_t` - Error codes enum

#### spi_driver.c / spi_driver.h

SPI abstraction using Linux spidev interface.

| Function | Description |
|----------|-------------|
| `spi_init()` | Open and configure SPI device |
| `spi_write()` | Write data buffer |
| `spi_read()` | Read data buffer |
| `spi_transfer()` | Full-duplex transfer |
| `spi_write_byte()` | Write single byte |
| `spi_set_speed()` | Change clock speed at runtime |
| `spi_deinit()` | Close SPI device |

**Key types:**
- `spi_handle_t` - Opaque handle to SPI context
- `spi_config_t` - SPI configuration (device, speed, mode, bits)
- `spi_mode_t` - SPI modes 0-3 (CPOL/CPHA combinations)

### Layer 2: Service Layer (Device Drivers)

#### st7735s.c / st7735s.h

ST7735S TFT LCD display driver with drawing primitives.

| Function | Description |
|----------|-------------|
| `st7735s_init()` | Initialize display with configuration |
| `st7735s_fill()` | Fill entire screen with color |
| `st7735s_fill_rect()` | Draw filled rectangle |
| `st7735s_draw_pixel()` | Set single pixel |
| `st7735s_draw_hline()` | Draw horizontal line |
| `st7735s_draw_vline()` | Draw vertical line |
| `st7735s_draw_char()` | Render single character |
| `st7735s_draw_string()` | Render text string |
| `st7735s_draw_progress_bar()` | Draw progress bar widget |
| `st7735s_set_backlight()` | Control backlight |
| `st7735s_set_rotation()` | Set display rotation |
| `st7735s_invert()` | Invert display colors |
| `st7735s_deinit()` | Release display resources |

**Configuration presets:**
- `ST7735S_CONFIG_WEACT_DEFAULT` - WeAct Studio V1.5 GREENTAB display

#### st7735s_colors.h

RGB565 color definitions and utilities.

| Macro/Function | Description |
|----------------|-------------|
| `ST7735S_RGB(r,g,b)` | Convert 24-bit RGB to RGB565 |
| `st7735s_blend()` | Alpha blend two colors |
| `ST7735S_BLACK`, etc. | Predefined color constants |

#### fonts.c / fonts.h

Bitmap font data and rendering support.

| Function | Description |
|----------|-------------|
| `font_get_char()` | Get bitmap data for ASCII character |

Font specifications: 5x7 pixels, ASCII 32-126 (printable characters)

### Layer 3: Application Layer

#### main.c

Application entry point and UI logic.

| Component | Description |
|-----------|-------------|
| `main()` | Entry point, signal setup, main loop |
| `init_hardware()` | Initialize GPIO, SPI, display |
| `cleanup_hardware()` | Graceful shutdown and resource release |
| `draw_static_ui()` | Draw static elements (labels, header) once |
| `draw_dynamic_values()` | Update changing values (called each second) |
| `signal_handler()` | Handle SIGINT/SIGTERM for clean exit |

#### system_info.c / system_info.h

Linux system information collectors.

| Function | Data Source | Description |
|----------|-------------|-------------|
| `sysinfo_init()` | - | Prime CPU calculation baseline |
| `sysinfo_get_cpu()` | `/proc/stat` | CPU usage percentage |
| `sysinfo_get_memory()` | `/proc/meminfo` | RAM usage (%, MB used/total) |
| `sysinfo_get_temperature()` | `/sys/class/thermal/.../temp` | CPU temperature in Celsius |
| `sysinfo_get_disk()` | `statvfs("/")` | Root filesystem usage |
| `sysinfo_get_ip()` | `getifaddrs()` | First non-loopback IPv4 address |
| `sysinfo_get_uptime()` | `/proc/uptime` | System uptime formatted |

## Dependencies

### Required Libraries

| Library | Version | Package | Description |
|---------|---------|---------|-------------|
| libgpiod | 2.x | `libgpiod-dev` | GPIO character device interface |
| libc | - | (built-in) | Standard C library |
| libm | - | (built-in) | Math library |

### Raspberry Pi OS Requirements

- **OS**: Raspberry Pi OS (Bookworm or later) / Debian 12+
- **Kernel**: 6.1+ (with GPIO character device support)
- **Hardware**: Raspberry Pi 5 (uses `/dev/gpiochip4`)

**Note**: Raspberry Pi 4 and earlier use `/dev/gpiochip0`. Modify `PIN_DC`, `PIN_RST`, `PIN_BL` and gpiochip path in `main.c` accordingly.

### Install Dependencies

```bash
# Update package list
sudo apt update

# Install libgpiod development files
sudo apt install libgpiod-dev

# Verify installation
dpkg -l | grep libgpiod
# Should show: libgpiod-dev, libgpiod3 (version 2.x)
```

### Enable SPI

```bash
# Enable SPI interface
sudo raspi-config
# Navigate: Interface Options -> SPI -> Enable

# Or manually edit /boot/firmware/config.txt:
# dtparam=spi=on

# Reboot to apply
sudo reboot

# Verify SPI is enabled
ls /dev/spidev*
# Should show: /dev/spidev0.0  /dev/spidev0.1
```

## Building

### Quick Build

```bash
cd pi_monitor
make
```

### Build Output

```
gcc -Wall -Wextra -Iinclude -O2 -c drivers/gpio_driver.c -o build/gpio_driver.o
gcc -Wall -Wextra -Iinclude -O2 -c drivers/spi_driver.c -o build/spi_driver.o
gcc -Wall -Wextra -Iinclude -O2 -c services/st7735s.c -o build/st7735s.o
gcc -Wall -Wextra -Iinclude -O2 -c services/fonts.c -o build/fonts.o
gcc -Wall -Wextra -Iinclude -O2 -c app/main.c -o build/main.o
gcc -Wall -Wextra -Iinclude -O2 -c app/system_info.c -o build/system_info.o
gcc build/*.o -o bin/pi_monitor -lgpiod -lm
Built: bin/pi_monitor
```

### Debug Build

```bash
make debug
```

Adds `-g` (debug symbols) and `-DDEBUG` preprocessor flag.

### Clean Build

```bash
make clean    # Remove build artifacts
make          # Full rebuild
```

## Installation

### Install to System

```bash
# Build and install to /usr/local/bin
sudo make install

# Verify installation
which pi_monitor
# Output: /usr/local/bin/pi_monitor
```

### Create Systemd Service (Auto-start)

```bash
# Create service file
sudo tee /etc/systemd/system/pi-monitor.service << 'EOF'
[Unit]
Description=Pi System Monitor
After=multi-user.target

[Service]
Type=simple
ExecStart=/usr/local/bin/pi_monitor
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

# Enable and start service
sudo systemctl daemon-reload
sudo systemctl enable pi-monitor
sudo systemctl start pi-monitor

# Check status
sudo systemctl status pi-monitor
```

### Uninstall

```bash
# Stop and disable service (if installed)
sudo systemctl stop pi-monitor
sudo systemctl disable pi-monitor
sudo rm /etc/systemd/system/pi-monitor.service

# Remove binary
sudo rm /usr/local/bin/pi_monitor
```

## Running

### Direct Execution

```bash
# Requires sudo for GPIO/SPI access
sudo ./bin/pi_monitor

# Or after installation
sudo pi_monitor
```

### Using Make

```bash
make run    # Builds (if needed) and runs with sudo
```

### Stop

Press `Ctrl+C` for graceful shutdown (clears display, releases GPIO).

## Makefile Reference

### Directory Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `INC_DIR` | `include` | Header files directory |
| `DRV_DIR` | `drivers` | Layer 1 source files |
| `SVC_DIR` | `services` | Layer 2 source files |
| `APP_DIR` | `app` | Layer 3 source files |
| `BUILD_DIR` | `build` | Object file output |
| `BIN_DIR` | `bin` | Binary output |

### Compiler Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `CC` | `gcc` | C compiler |
| `CFLAGS` | `-Wall -Wextra -Iinclude -O2` | Compiler flags |
| `LDFLAGS` | `-lgpiod -lm` | Linker flags |
| `DEBUG_FLAGS` | `-g -DDEBUG` | Debug build flags |

### Targets

| Target | Description |
|--------|-------------|
| `make` / `make all` | Build the project |
| `make debug` | Build with debug symbols |
| `make clean` | Remove build/ and bin/ directories |
| `make run` | Build and run with sudo |
| `make install` | Install binary to /usr/local/bin |
| `make info` | Display source file lists |

### Adding New Files

1. **New driver** (Layer 1):
   - Add source to `drivers/`
   - Add header to `include/`
   - Update `DRV_SRCS` in Makefile
   - Add compile rule for new `.o` file

2. **New service** (Layer 2):
   - Add source to `services/`
   - Add header to `include/`
   - Update `SVC_SRCS` in Makefile
   - Add compile rule for new `.o` file

3. **New app module** (Layer 3):
   - Add source to `app/`
   - Add header to `include/` (if public)
   - Update `APP_SRCS` in Makefile
   - Add compile rule for new `.o` file

Example adding a new driver:

```makefile
# In Makefile, add to DRV_SRCS:
DRV_SRCS = $(DRV_DIR)/gpio_driver.c $(DRV_DIR)/spi_driver.c $(DRV_DIR)/i2c_driver.c

# Add compile rule:
$(BUILD_DIR)/i2c_driver.o: $(DRV_DIR)/i2c_driver.c
    $(CC) $(CFLAGS) -c $< -o $@
```

## Hardware Setup

### Pin Connections

| ST7735S Pin | Function | RPi 5 GPIO | RPi 5 Header Pin |
|-------------|----------|------------|------------------|
| VCC | Power (3.3V) | 3.3V | Pin 1 |
| GND | Ground | GND | Pin 6 |
| SCL | SPI Clock | GPIO11 (SCLK) | Pin 23 |
| SDA | SPI MOSI | GPIO10 (MOSI) | Pin 19 |
| CS | Chip Select | GPIO8 (CE0) | Pin 24 |
| DC | Data/Command | GPIO24 | Pin 18 |
| RST | Reset | GPIO25 | Pin 22 |
| BL | Backlight | GPIO18 | Pin 12 |

### Wiring Diagram

```
ST7735S          Raspberry Pi 5
+--------+       +-----------------------+
| VCC    |------>| 3.3V (Pin 1)          |
| GND    |------>| GND  (Pin 6)          |
| SCL    |------>| GPIO11 (Pin 23) SCLK  |
| SDA    |------>| GPIO10 (Pin 19) MOSI  |
| CS     |------>| GPIO8  (Pin 24) CE0   |
| DC     |------>| GPIO24 (Pin 18)       |
| RST    |------>| GPIO25 (Pin 22)       |
| BL     |------>| GPIO18 (Pin 12)       |
+--------+       +-----------------------+
```

## API Reference

See individual header files in `include/` for complete API documentation.

### Quick Example

```c
#include "gpio_driver.h"
#include "spi_driver.h"
#include "st7735s.h"
#include "system_info.h"

int main(void) {
    gpio_handle_t gpio;
    spi_handle_t spi;
    st7735s_handle_t display;
    
    // Initialize GPIO
    gpio_init("/dev/gpiochip4", &gpio);
    gpio_config_t pins[] = {
        { .pin = 24, .direction = GPIO_DIR_OUTPUT },
        { .pin = 25, .direction = GPIO_DIR_OUTPUT },
        { .pin = 18, .direction = GPIO_DIR_OUTPUT }
    };
    gpio_configure(gpio, pins, 3, "my_app");
    
    // Initialize SPI
    spi_config_t spi_cfg = {
        .device = "/dev/spidev0.0",
        .speed_hz = 32000000,
        .mode = SPI_MODE_0,
        .bits_per_word = 8
    };
    spi_init(&spi_cfg, &spi);
    
    // Initialize display
    st7735s_config_t cfg = ST7735S_CONFIG_WEACT_DEFAULT;
    st7735s_init(gpio, spi, &cfg, &display);
    
    // Draw something
    st7735s_fill(display, ST7735S_BLACK);
    st7735s_draw_string(display, 10, 10, "Hello Pi!", 
                        ST7735S_WHITE, ST7735S_BLACK, 2);
    
    // Get system info
    sysinfo_init();
    cpu_info_t cpu;
    sysinfo_get_cpu(&cpu);
    
    // Cleanup
    st7735s_deinit(display);
    spi_deinit(spi);
    gpio_deinit(gpio);
    
    return 0;
}
```

## Troubleshooting

### "Failed to open GPIO"

```
Failed to open GPIO
```

**Solutions:**
1. Run with `sudo` (required for GPIO access)
2. Verify gpiochip: `ls /dev/gpiochip*`
3. For Pi 5, use `/dev/gpiochip4`; for Pi 4, use `/dev/gpiochip0`

### "Failed to open SPI"

```
Failed to open SPI
```

**Solutions:**
1. Enable SPI via `sudo raspi-config`
2. Check SPI device exists: `ls /dev/spidev*`
3. Run with `sudo`

### Display Shows Nothing

1. Check wiring connections
2. Verify backlight pin is active (GPIO18 LOW for WeAct display)
3. Check display variant - try different offsets in config

### Build Errors

```
fatal error: gpiod.h: No such file or directory
```

**Solution:** Install libgpiod-dev:
```bash
sudo apt install libgpiod-dev
```

### Permission Denied

Always run with `sudo` for hardware access:
```bash
sudo ./bin/pi_monitor
```

## License

MIT License
