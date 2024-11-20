extern "C" {
    #include <stdio.h>
    #include <stdint.h>
    #include "hardware/clocks.h"

    #include "pico/stdlib.h"

    #include "pins.h"
    #include "picotool_binary_information.h"
}

#include "log.h"

// Super-globals (for all modules)
Log logger;


int main() {
    // Overclock the board to 250MHz. According to
    // https://www.youtube.com/watch?v=G2BuoFNLo this should be
    // totally safe with the default 1.10V Vcore
    set_sys_clock_khz(250000, true);

    stdio_init_all();
    logger.init();

    // /!\ Do NOT use LOG() above this line! /!\

    // SETUP COMPLETE
    LOG("SYSTEM: SETUP COMPLETE");

    while (true) {
        LOG("Still alive. Implement something! ...");
        sleep_ms(3000);
    }
};
