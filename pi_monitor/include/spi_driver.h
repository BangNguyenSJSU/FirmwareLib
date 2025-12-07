/**
 * @file spi_driver.h
 * @brief SPI Driver Interface (Layer 1)
 */

#ifndef SPI_DRIVER_H
#define SPI_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SPI_OK = 0,
    SPI_ERR_INVALID_ARG,
    SPI_ERR_NOT_FOUND,
    SPI_ERR_PERMISSION,
    SPI_ERR_IO,
    SPI_ERR_BUSY,
    SPI_ERR_UNKNOWN
} spi_status_t;

typedef enum {
    SPI_MODE_0 = 0,
    SPI_MODE_1 = 1,
    SPI_MODE_2 = 2,
    SPI_MODE_3 = 3
} spi_mode_t;

typedef enum {
    SPI_BIT_ORDER_MSB_FIRST = 0,
    SPI_BIT_ORDER_LSB_FIRST
} spi_bit_order_t;

typedef struct {
    const char* device;
    uint32_t speed_hz;
    spi_mode_t mode;
    uint8_t bits_per_word;
    spi_bit_order_t bit_order;
} spi_config_t;

typedef struct spi_handle_s* spi_handle_t;

#define SPI_CONFIG_DEFAULT { \
    .device = "/dev/spidev0.0", \
    .speed_hz = 10000000, \
    .mode = SPI_MODE_0, \
    .bits_per_word = 8, \
    .bit_order = SPI_BIT_ORDER_MSB_FIRST \
}

spi_status_t spi_init(const spi_config_t* config, spi_handle_t* handle);
spi_status_t spi_write(spi_handle_t handle, const uint8_t* data, size_t len);
spi_status_t spi_read(spi_handle_t handle, uint8_t* data, size_t len);
spi_status_t spi_transfer(spi_handle_t handle, const uint8_t* tx, uint8_t* rx, size_t len);
spi_status_t spi_write_byte(spi_handle_t handle, uint8_t byte);
spi_status_t spi_set_speed(spi_handle_t handle, uint32_t speed_hz);
const char* spi_status_str(spi_status_t status);
void spi_deinit(spi_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* SPI_DRIVER_H */
