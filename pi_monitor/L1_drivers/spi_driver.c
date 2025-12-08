/**
 * @file spi_driver.c
 * @brief SPI Driver Implementation (Layer 1)
 */

#include "spi_driver.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <errno.h>

struct spi_handle_s {
    int fd;
    spi_config_t config;
};

spi_status_t spi_init(const spi_config_t* config, spi_handle_t* handle)
{
    if (!config || !handle || !config->device) return SPI_ERR_INVALID_ARG;
    
    spi_handle_t h = calloc(1, sizeof(struct spi_handle_s));
    if (!h) return SPI_ERR_UNKNOWN;
    
    h->fd = open(config->device, O_RDWR);
    if (h->fd < 0) {
        free(h);
        if (errno == ENOENT) return SPI_ERR_NOT_FOUND;
        if (errno == EACCES || errno == EPERM) return SPI_ERR_PERMISSION;
        if (errno == EBUSY) return SPI_ERR_BUSY;
        return SPI_ERR_UNKNOWN;
    }
    
    uint8_t mode = (uint8_t)config->mode;
    if (config->bit_order == SPI_BIT_ORDER_LSB_FIRST) mode |= SPI_LSB_FIRST;
    if (ioctl(h->fd, SPI_IOC_WR_MODE, &mode) < 0) {
        close(h->fd);
        free(h);
        return SPI_ERR_IO;
    }
    
    uint8_t bits = config->bits_per_word ? config->bits_per_word : 8;
    if (ioctl(h->fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        close(h->fd);
        free(h);
        return SPI_ERR_IO;
    }
    
    uint32_t speed = config->speed_hz;
    if (ioctl(h->fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        close(h->fd);
        free(h);
        return SPI_ERR_IO;
    }
    
    h->config = *config;
    h->config.bits_per_word = bits;
    
    *handle = h;
    return SPI_OK;
}

spi_status_t spi_write(spi_handle_t handle, const uint8_t* data, size_t len)
{
    if (!handle || !data || len == 0) return SPI_ERR_INVALID_ARG;
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)data,
        .rx_buf = 0,
        .len = len,
        .speed_hz = handle->config.speed_hz,
        .bits_per_word = handle->config.bits_per_word,
        .delay_usecs = 0,
    };
    
    if (ioctl(handle->fd, SPI_IOC_MESSAGE(1), &tr) < 0) return SPI_ERR_IO;
    return SPI_OK;
}

spi_status_t spi_read(spi_handle_t handle, uint8_t* data, size_t len)
{
    if (!handle || !data || len == 0) return SPI_ERR_INVALID_ARG;
    
    struct spi_ioc_transfer tr = {
        .tx_buf = 0,
        .rx_buf = (unsigned long)data,
        .len = len,
        .speed_hz = handle->config.speed_hz,
        .bits_per_word = handle->config.bits_per_word,
        .delay_usecs = 0,
    };
    
    if (ioctl(handle->fd, SPI_IOC_MESSAGE(1), &tr) < 0) return SPI_ERR_IO;
    return SPI_OK;
}

spi_status_t spi_transfer(spi_handle_t handle, const uint8_t* tx, uint8_t* rx, size_t len)
{
    if (!handle || len == 0 || (!tx && !rx)) return SPI_ERR_INVALID_ARG;
    
    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = len,
        .speed_hz = handle->config.speed_hz,
        .bits_per_word = handle->config.bits_per_word,
        .delay_usecs = 0,
    };
    
    if (ioctl(handle->fd, SPI_IOC_MESSAGE(1), &tr) < 0) return SPI_ERR_IO;
    return SPI_OK;
}

spi_status_t spi_write_byte(spi_handle_t handle, uint8_t byte)
{
    return spi_write(handle, &byte, 1);
}

spi_status_t spi_set_speed(spi_handle_t handle, uint32_t speed_hz)
{
    if (!handle || speed_hz == 0) return SPI_ERR_INVALID_ARG;
    if (ioctl(handle->fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed_hz) < 0) return SPI_ERR_IO;
    handle->config.speed_hz = speed_hz;
    return SPI_OK;
}

const char* spi_status_str(spi_status_t status)
{
    switch (status) {
        case SPI_OK:             return "OK";
        case SPI_ERR_INVALID_ARG: return "Invalid argument";
        case SPI_ERR_NOT_FOUND:  return "SPI device not found";
        case SPI_ERR_PERMISSION: return "Permission denied";
        case SPI_ERR_IO:         return "I/O error";
        case SPI_ERR_BUSY:       return "SPI device busy";
        case SPI_ERR_UNKNOWN:    return "Unknown error";
        default:                 return "Invalid status";
    }
}

void spi_deinit(spi_handle_t handle)
{
    if (!handle) return;
    if (handle->fd >= 0) close(handle->fd);
    free(handle);
}
