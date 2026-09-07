#pragma once
#include <stdbool.h>
#include "esp_err.h"
typedef struct { unsigned rx_buffer_size, tx_buffer_size; } usb_serial_jtag_driver_config_t;
bool usb_serial_jtag_is_driver_installed(void);
esp_err_t usb_serial_jtag_driver_install(const usb_serial_jtag_driver_config_t *config);
