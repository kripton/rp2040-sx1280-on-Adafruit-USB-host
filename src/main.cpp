extern "C" {
    #include <stdio.h>
    #include <stdint.h>
    #include "hardware/clocks.h"
    #include <hardware/vreg.h>      // To set 1.15V instead of the default 1.10V

    #include "pico/stdlib.h"
    #include "pico/multicore.h"
    #include "stdio_usb.h"

    #include "pins.h"
    #include "picotool_binary_information.h"

    #include "tusb_lwip_glue.h"
}

// define pins to be used
#define SPI_PORT spi1
#define SPI_MISO    8
#define SPI_MOSI   15
#define SPI_SCK    14

#define SX1280_NSS   0
#define SX1280_DIO1 11
#define SX1280_NRST 12
#define SX1280_BUSY  5

// include the library
#include <RadioLib.h>

// include the hardware abstraction layer
#include "hal/RPiPico/PicoHal.h"

#include "log.h"
#include "webserver.h"

// Super-globals (for all modules)
Log logger;
WebServer webServer;

bool radioInitDone = false;

// flag to indicate that a packet was received
volatile bool receivedFlag = false;

PicoHal* hal = new PicoHal(SPI_PORT, SPI_MISO, SPI_MOSI, SPI_SCK);
SX1280 radio = new Module(hal, SX1280_NSS, SX1280_DIO1, SX1280_NRST, SX1280_BUSY);

void core1_tasks(void);

void setFlag(void) {
    LOG("SETTING FLAG");
    receivedFlag = true;
  }

int main() {
    // Overclock the board to 199MHz. According to
    // https://www.youtube.com/watch?v=G2BuoFNLo this should be
    // totally safe with the default 1.10V Vcore
    // However, we use 1.15V now since that is the "default" since SDK 2.1.1
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    set_sys_clock_khz(198000, true);

    //stdio_init_all();
    logger.init();

    // /!\ Do NOT use LOG() above this line! /!\

    // Enable USB interface, the debugging console and the logger
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

#define RX_IRQ

// Core1: I2C (GPIO expander), SPI to the Timos, Status LEDs,
//        Config reset (Button reading and state machine but NOT flash)
void core1_tasks() {
    int state;
    uint8_t byteArr[255];

    while (true) {
        //LOG("Core1 idling about ...");

        if (!radioInitDone) {
            state = radio.beginFLRC(2400.0, 1300, 3, 10, 16, 2);
            uint8_t syncWord[] = {0xFA, 0xAC, 0x55, 0x37};
            state = radio.setSyncWord(syncWord, 4);
            if (state == RADIOLIB_ERR_NONE) {
                LOG("SX1280 INIT OK");
            } else {
                LOG("SX1280 INIT ERROR: %d", state);
            }

            // set the function that will be called
            // when new packet is received
            radio.setPacketReceivedAction(setFlag);

            radioInitDone = true;

            // RX EN
            gpio_init(4);
            gpio_set_dir(4, GPIO_OUT);

            // TX EN
            gpio_init(13);
            gpio_set_dir(13, GPIO_OUT);

#ifdef RX_IRQ
            // put module to listen mode
            //radio.startReceive();
            receivedFlag = true;
#endif
        }

#ifdef TX
        gpio_put(13, true);
        sleep_ms(20);
        uint32_t ms = to_ms_since_boot(get_absolute_time());
        memcpy(byteArr, (void*)&ms, 4);
        memcpy(byteArr+4, (void*)&ms, 4);
        state = radio.transmit(byteArr, 8);
        if (state == RADIOLIB_ERR_NONE) {
            LOG("[SX1280] Packet transmitted successfully!");
        } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
            LOG("[SX1280] Packet too long!");
        } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
            LOG("[SX1280] Timed out while transmitting!");
        } else {
            LOG("[SX1280] Failed to transmit packet, code %d", state);
        }
        gpio_put(13, false);
#endif

#ifdef RX_BLOCKING
        gpio_put(4, true);
        sleep_ms(20);
        state = radio.receive(byteArr, 8);
        if (state == RADIOLIB_ERR_NONE) {
            LOG("[SX1280] Received packet! Data:");
            LOGHEX(8, byteArr);
        } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
            LOG("[SX1280] Timed out while waiting for packet!");
        } else {
            LOG("[SX1280] Failed to receive packet, code %d", state);
        }
        gpio_put(4, false);
#endif

#ifdef RX_IRQ
        if (receivedFlag) {
            // reset flag
            receivedFlag = false;

            // you can also read received data as byte array
            int numBytes = radio.getPacketLength();
            state = radio.readData(byteArr, numBytes);

            if (state == RADIOLIB_ERR_NONE) {
                // packet was successfully received
                LOG("[SX1280] Received packet! Data: ");

                // print data of the packet
                LOGHEX(8, byteArr);

                // print RSSI (Received Signal Strength Indicator)
                LOG("[SX1280] RSSI:\t\t%f dBm", radio.getRSSI());

            } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
                // packet was received, but is malformed
                LOG("CRC error!");

            } else {
                // some other error occurred
                LOG("failed, code %d", state);
            }

            // put module back to listen mode
            radio.startReceive();
        }
#endif

        //gpio_put(PICO_DEFAULT_LED_PIN, !gpio_get(PICO_DEFAULT_LED_PIN);
        sleep_us(500);
    }
};