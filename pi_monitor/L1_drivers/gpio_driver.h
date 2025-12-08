/**
 * @file gpio_driver.h
 * @brief GPIO Driver Interface (Layer 1)
 * 
 * Platform-independent GPIO abstraction layer for Raspberry Pi.
 * Uses libgpiod v2 for GPIO access.
 */

#ifndef GPIO_DRIVER_H
#define GPIO_DRIVER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GPIO_OK = 0,
    GPIO_ERR_INVALID_ARG,
    GPIO_ERR_NOT_FOUND,
    GPIO_ERR_BUSY,
    GPIO_ERR_PERMISSION,
    GPIO_ERR_IO,
    GPIO_ERR_NOT_CONFIGURED,
    GPIO_ERR_UNKNOWN
} gpio_status_t;

typedef enum {
    GPIO_DIR_INPUT = 0,
    GPIO_DIR_OUTPUT
} gpio_direction_t;

typedef enum {
    GPIO_STATE_LOW = 0,
    GPIO_STATE_HIGH
} gpio_state_t;

typedef enum {
    GPIO_PULL_NONE = 0,
    GPIO_PULL_UP,
    GPIO_PULL_DOWN
} gpio_pull_t;

typedef struct {
    uint32_t pin;
    gpio_direction_t direction;
    gpio_state_t initial_state;
    gpio_pull_t pull;
} gpio_config_t;

typedef struct gpio_handle_s* gpio_handle_t;

gpio_status_t gpio_init(const char* chip_path, gpio_handle_t* handle);
gpio_status_t gpio_configure(gpio_handle_t handle, const gpio_config_t* configs,
                             size_t count, const char* consumer);
gpio_status_t gpio_write(gpio_handle_t handle, uint32_t pin, gpio_state_t state);
gpio_status_t gpio_read(gpio_handle_t handle, uint32_t pin, gpio_state_t* state);
gpio_status_t gpio_write_multiple(gpio_handle_t handle, const uint32_t* pins,
                                  const gpio_state_t* states, size_t count);
gpio_status_t gpio_toggle(gpio_handle_t handle, uint32_t pin);
bool gpio_is_configured(gpio_handle_t handle, uint32_t pin);
const char* gpio_status_str(gpio_status_t status);
void gpio_deinit(gpio_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_DRIVER_H */
