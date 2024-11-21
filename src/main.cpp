extern "C" {
    #include <stdio.h>
    #include <stdint.h>
    #include "hardware/clocks.h"

    #include "pico/stdlib.h"
    #include "pico/multicore.h"
    #include "stdio_usb.h"

    #include "pins.h"
    #include "picotool_binary_information.h"

    #include "tusb_lwip_glue.h"
}

#include "log.h"
#include "webserver.h"

// Super-globals (for all modules)
Log logger;
WebServer webServer;

void core1_tasks(void);

int main() {
    // Overclock the board to 250MHz. According to
    // https://www.youtube.com/watch?v=G2BuoFNLo this should be
    // totally safe with the default 1.10V Vcore
    set_sys_clock_khz(250000, true);

    //stdio_init_all();
    logger.init();

    // /!\ Do NOT use LOG() above this line! /!\

    // Now that we know our final product name (depends on the hardware
    // connected): Enable USB interface, the debugging console and the logger
    tusb_init();
    stdio_usb_init();
    Log::stdioReady = true;

    // Initialize lwip, dhcpd and httpd
    // TinyUSB already needs to be initialized at this point
    init_lwip();
    wait_for_netif_is_up();

    dhcpd_init();

    webServer.init();

    // SETUP COMPLETE
    LOG("SYSTEM: SETUP COMPLETE");

    // Run all important tasks at least once before we start AUX tasks on core1
    // so the USB device enumeration doesn't time-out
    tud_task();
    tud_task();
    tud_task();
    webServer.cyclicTask();

    // Now get core1 running ...
    LOG("SYSTEM: Starting core 1 ...");
    multicore_launch_core1(core1_tasks);

    while (true) {
        tud_task();

        webServer.cyclicTask(); // Make sure this is on core0 since it
                                // WILL halt core1 when writing to the flash!
                                // Handles USB-ETH traffic
    }
};

// Core1: I2C (GPIO expander), SPI to the Timos, Status LEDs,
//        Config reset (Button reading and state machine but NOT flash)
void core1_tasks() {
    while (true) {
        LOG("Core1 idling about ...");
        sleep_ms(5000);
    }
};