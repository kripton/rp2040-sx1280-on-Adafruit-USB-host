#ifndef PICOTOOL_BINARY_INFORMATION_H
#define PICOTOOL_BINARY_INFORMATION_H

#include "pico/binary_info.h"
#include "pins.h"
#include "version.h"

bi_decl(bi_program_name("rp2040-servolenkung"));
bi_decl(bi_program_description("Generates a PWM signal with a frequency depending on an input signal"));
//bi_decl(bi_program_url("https://github.com/OpenLightingProject/rp2040-dmxsun"));

bi_decl(bi_program_version_string(VERSION));

//bi_decl(bi_program_feature("Integrated webserver"));
//bi_decl(bi_program_feature("Reboot on baudrate change: 1200 = Bootloader, 2400 = reset"));

//bi_decl(bi_2pins_with_func(PIN_I2C_SCL, PIN_I2C_SDA, GPIO_FUNC_I2C));

bi_decl(bi_1pin_with_name(PIN_LED_PICO, "On-board status LED"));

#endif // PICOTOOL_BINARY_INFORMATION_H