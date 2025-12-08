/**
 * @file gpio_driver.c
 * @brief GPIO Driver Implementation (Layer 1)
 */

#include "gpio_driver.h"
#include <gpiod.h>
#include <stdlib.h>
#include <string.h>

#define GPIO_MAX_PINS 64

struct gpio_handle_s {
    struct gpiod_chip *chip;
    struct gpiod_line_request *req;
    uint32_t configured_pins[GPIO_MAX_PINS];
    gpio_state_t pin_states[GPIO_MAX_PINS];
    size_t num_pins;
    bool is_configured;
};

static int find_pin_index(gpio_handle_t handle, uint32_t pin)
{
    if (!handle) return -1;
    for (size_t i = 0; i < handle->num_pins; i++) {
        if (handle->configured_pins[i] == pin) return (int)i;
    }
    return -1;
}

gpio_status_t gpio_init(const char* chip_path, gpio_handle_t* handle)
{
    if (!chip_path || !handle) return GPIO_ERR_INVALID_ARG;
    
    gpio_handle_t h = calloc(1, sizeof(struct gpio_handle_s));
    if (!h) return GPIO_ERR_UNKNOWN;
    
    h->chip = gpiod_chip_open(chip_path);
    if (!h->chip) {
        free(h);
        return GPIO_ERR_NOT_FOUND;
    }
    
    h->req = NULL;
    h->num_pins = 0;
    h->is_configured = false;
    
    *handle = h;
    return GPIO_OK;
}

gpio_status_t gpio_configure(gpio_handle_t handle, const gpio_config_t* configs,
                             size_t count, const char* consumer)
{
    if (!handle || !configs || count == 0 || !consumer) return GPIO_ERR_INVALID_ARG;
    if (count > GPIO_MAX_PINS) return GPIO_ERR_INVALID_ARG;
    
    if (handle->req) {
        gpiod_line_request_release(handle->req);
        handle->req = NULL;
    }
    
    struct gpiod_line_settings *settings = gpiod_line_settings_new();
    if (!settings) return GPIO_ERR_UNKNOWN;
    
    struct gpiod_line_config *line_cfg = gpiod_line_config_new();
    if (!line_cfg) {
        gpiod_line_settings_free(settings);
        return GPIO_ERR_UNKNOWN;
    }
    
    struct gpiod_request_config *req_cfg = gpiod_request_config_new();
    if (!req_cfg) {
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        return GPIO_ERR_UNKNOWN;
    }
    
    gpiod_request_config_set_consumer(req_cfg, consumer);
    
    unsigned int offsets[GPIO_MAX_PINS];
    for (size_t i = 0; i < count; i++) {
        offsets[i] = configs[i].pin;
        handle->configured_pins[i] = configs[i].pin;
        handle->pin_states[i] = configs[i].initial_state;
    }
    handle->num_pins = count;
    
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_OUTPUT);
    
    if (gpiod_line_config_add_line_settings(line_cfg, offsets, count, settings) < 0) {
        gpiod_request_config_free(req_cfg);
        gpiod_line_config_free(line_cfg);
        gpiod_line_settings_free(settings);
        return GPIO_ERR_UNKNOWN;
    }
    
    handle->req = gpiod_chip_request_lines(handle->chip, req_cfg, line_cfg);
    
    gpiod_request_config_free(req_cfg);
    gpiod_line_config_free(line_cfg);
    gpiod_line_settings_free(settings);
    
    if (!handle->req) return GPIO_ERR_BUSY;
    
    for (size_t i = 0; i < count; i++) {
        if (configs[i].direction == GPIO_DIR_OUTPUT) {
            enum gpiod_line_value val = (configs[i].initial_state == GPIO_STATE_HIGH) 
                ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
            gpiod_line_request_set_value(handle->req, configs[i].pin, val);
        }
    }
    
    handle->is_configured = true;
    return GPIO_OK;
}

gpio_status_t gpio_write(gpio_handle_t handle, uint32_t pin, gpio_state_t state)
{
    if (!handle) return GPIO_ERR_INVALID_ARG;
    if (!handle->is_configured || !handle->req) return GPIO_ERR_NOT_CONFIGURED;
    
    int idx = find_pin_index(handle, pin);
    if (idx < 0) return GPIO_ERR_NOT_CONFIGURED;
    
    enum gpiod_line_value val = (state == GPIO_STATE_HIGH) 
        ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    
    if (gpiod_line_request_set_value(handle->req, pin, val) < 0) return GPIO_ERR_IO;
    
    handle->pin_states[idx] = state;
    return GPIO_OK;
}

gpio_status_t gpio_read(gpio_handle_t handle, uint32_t pin, gpio_state_t* state)
{
    if (!handle || !state) return GPIO_ERR_INVALID_ARG;
    if (!handle->is_configured || !handle->req) return GPIO_ERR_NOT_CONFIGURED;
    
    int idx = find_pin_index(handle, pin);
    if (idx < 0) return GPIO_ERR_NOT_CONFIGURED;
    
    enum gpiod_line_value val = gpiod_line_request_get_value(handle->req, pin);
    if (val == GPIOD_LINE_VALUE_ERROR) return GPIO_ERR_IO;
    
    *state = (val == GPIOD_LINE_VALUE_ACTIVE) ? GPIO_STATE_HIGH : GPIO_STATE_LOW;
    return GPIO_OK;
}

gpio_status_t gpio_write_multiple(gpio_handle_t handle, const uint32_t* pins,
                                  const gpio_state_t* states, size_t count)
{
    if (!handle || !pins || !states || count == 0) return GPIO_ERR_INVALID_ARG;
    
    for (size_t i = 0; i < count; i++) {
        gpio_status_t status = gpio_write(handle, pins[i], states[i]);
        if (status != GPIO_OK) return status;
    }
    return GPIO_OK;
}

gpio_status_t gpio_toggle(gpio_handle_t handle, uint32_t pin)
{
    if (!handle) return GPIO_ERR_INVALID_ARG;
    
    int idx = find_pin_index(handle, pin);
    if (idx < 0) return GPIO_ERR_NOT_CONFIGURED;
    
    gpio_state_t new_state = (handle->pin_states[idx] == GPIO_STATE_HIGH) 
        ? GPIO_STATE_LOW : GPIO_STATE_HIGH;
    return gpio_write(handle, pin, new_state);
}

bool gpio_is_configured(gpio_handle_t handle, uint32_t pin)
{
    if (!handle || !handle->is_configured) return false;
    return find_pin_index(handle, pin) >= 0;
}

const char* gpio_status_str(gpio_status_t status)
{
    switch (status) {
        case GPIO_OK:              return "OK";
        case GPIO_ERR_INVALID_ARG: return "Invalid argument";
        case GPIO_ERR_NOT_FOUND:   return "GPIO chip not found";
        case GPIO_ERR_BUSY:        return "GPIO line busy";
        case GPIO_ERR_PERMISSION:  return "Permission denied";
        case GPIO_ERR_IO:          return "I/O error";
        case GPIO_ERR_NOT_CONFIGURED: return "GPIO not configured";
        case GPIO_ERR_UNKNOWN:     return "Unknown error";
        default:                   return "Invalid status";
    }
}

void gpio_deinit(gpio_handle_t handle)
{
    if (!handle) return;
    if (handle->req) gpiod_line_request_release(handle->req);
    if (handle->chip) gpiod_chip_close(handle->chip);
    free(handle);
}
