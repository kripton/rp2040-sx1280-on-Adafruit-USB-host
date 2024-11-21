#ifndef PICOTOOL_BINARY_INFORMATION_H
#define PICOTOOL_BINARY_INFORMATION_H

#include "pico/binary_info.h"
#include "pins.h"
#include "version.h"

bi_decl(bi_program_name("rp2040-sx1280-on-Adafruit-USB-host"));
bi_decl(bi_program_description("Just another template"));
//bi_decl(bi_program_url("https://github.com/ORG/REPO"));

bi_decl(bi_program_version_string(VERSION));

//bi_decl(bi_program_feature("Integrated webserver"));
bi_decl(bi_program_feature("Reboot on baudrate change: 1200 = Bootloader, 2400 = reset"));

bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SCL_PIN, PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C));

bi_decl(bi_1pin_with_name(PICO_DEFAULT_LED_PIN, "On-board status LED"));

#endif // PICOTOOL_BINARY_INFORMATION_H
