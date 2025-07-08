extern "C" {
    #include <stdio.h>
    #include <stdint.h>
    #include "hardware/clocks.h"
    #include <hardware/vreg.h>      // To set 1.15V instead of the default 1.10V

    #include "pico/stdlib.h"
    #include "pico/multicore.h"
    #include "hardware/pio.h"
    #include "stdio_usb.h"

    #include "pins.h"
    #include "picotool_binary_information.h"
    #include "crc_modbus.h"
    #include "eastron-SDM72M.h"

    #include "uart_rx.pio.h"
    #include "uart_tx.pio.h"
    #include "wait_irq_and_put_low.pio.h"

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

// PIO program offsets
int offsetTx;
int offsetRx;

void core1_tasks(void);

void setFlag(void) {
    LOG("SETTING FLAG");
    receivedFlag = true;
  }

int main() {
    // Overclock the board to 200MHz. According to
    // https://www.youtube.com/watch?v=G2BuoFNLo this should be
    // totally safe with the default 1.10V Vcore
    // However, we use 1.15V now since that is the "default" since SDK 2.1.1
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    set_sys_clock_khz(200000, true);

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


    // UART TX is on PIO0 (since that is what one does first)
    offsetTx = pio_add_program(pio0, &uart_tx_program);

    // UART RX is on PIO1 (since that is what one does second)
    offsetRx = pio_add_program(pio1, &uart_rx_program);

    uart_tx_program_init(pio0, 0, offsetTx, 24, 25, 19200);
    uart_rx_program_init(pio1, 0, offsetRx, 24, 19200);

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

int modbus_request(uint8_t slave_address, uint16_t function_code,
                   uint16_t start_address, uint16_t word_count,
                   uint8_t* response_buffer, size_t response_buffer_size) {

    // Prepare the request
    uint8_t request[12];
    request[0] = 0x07; // Length - 1 (7 bytes of data)
    request[1] = slave_address; // Slave address
    request[2] = function_code; // Function code
    request[3] = (start_address >> 8) & 0xFF; // Start address high byte
    request[4] = start_address & 0xFF; // Start address low byte
    request[5] = (word_count >> 8) & 0xFF; // Word count high byte
    request[6] = word_count & 0xFF; // Word count low byte

    uint16_t crc = crc_init();
    crc = crc_update(crc, request+1, 6);
    crc = crc_finalize(crc);

    request[7] = crc & 0xFF; // CRC low byte
    request[8] = (crc >> 8) & 0xFF; // CRC high byte
    //LOG("[UART TX] Sending request: ");
    //LOGHEX(9, request);
    // Send the request
    uart_tx_program_init(pio0, 0, offsetTx, 24, 25, 19200);
    for (size_t i = 0; i < 9; i++) {
        uart_tx_program_putc(pio0, 0, request[i]);
    }
    sleep_us(4760); // Wait for the data to be sent
    //LOG("[UART TX] Request sent, waiting for response...");
    // MUX our data pin to PIO1 for receiving
    uart_rx_program_init(pio1, 0, offsetRx, 24, 19200);
    // Wait for the response
    uint8_t byte_count;
    size_t bytes_received = 0;
    uint16_t crc_received;

    for (size_t i = 0; i < response_buffer_size; i++) {
        uint8_t c = uart_rx_program_getc(pio1, 0, 10);

        if (i == 0 && c != slave_address) {
            //LOG("[UART RX] Slave address does not match. Expected: %02x, Got: %02x", slave_address, c);
            return -1; // Invalid response
        }

        if (i == 1 && c != function_code) {
            //LOG("[UART RX] Function code does not match. Expected: %02x, Got: %02x", function_code, c);
            return -1; // Invalid response
        }

        if (i == 2) {
            // Byte count (Raw payload, excluding slave address, function code and CRC)
            byte_count = c;
        }

        if (i == (3 + byte_count)) {
            crc_received = (c << 8); // High byte of CRC
        } else if (i == (4 + byte_count)) {
            crc_received |= c; // Low byte of CRC
            // Validate CRC
            uint16_t crc_calculated = crc_init();
            crc_calculated = crc_update(crc_calculated, response_buffer, 3 + byte_count);
            crc_calculated = crc_finalize(crc_calculated);

            // Swap the bytes for comparison
            crc_calculated = (crc_calculated >> 8) | (crc_calculated << 8);

            if (crc_calculated != crc_received) {
                //LOG("[UART RX] CRC mismatch! Expected: %04x, Got: %04x", crc_calculated, crc_received);
                return -1; // Invalid response
            }
            //LOG("[UART RX] CRC valid.");
            return bytes_received; // Return the number of bytes received
            break; // Exit loop after receiving the full response
        }

        response_buffer[i] = c;
        bytes_received++;
        //LOG("[UART RX] Got byte: %02x", c);
    }

    return bytes_received;
}

// Core1: I2C (GPIO expander), SPI to the Timos, Status LEDs,
//        Config reset (Button reading and state machine but NOT flash)
void core1_tasks() {
    int state;
    int retVal;
    uint8_t curModbusReg = 0xff; // Current Modbus register to read
    uint8_t byteArr[255];

    uint8_t numModbusReg = sizeof(eastron_sdm72m_input_registers) / sizeof(eastron_sdm72m_input_registers[0]);

    while (true) {
        //LOG("Core1 idling about ...");

        curModbusReg++;
        if (curModbusReg >= numModbusReg) {
            LOG("Core1: All registers read, resetting to 0");
            curModbusReg = 0;
        }

        retVal = modbus_request(0x01, 0x04, eastron_sdm72m_input_registers[curModbusReg], 0x0002, byteArr, sizeof(byteArr));

        if (retVal > 0) {
            // Convert and print the received data
            float val;
            memcpy(&val, byteArr+3, sizeof(float));

            // Reverse the 4 bytes of val
            uint8_t* valBytes = (uint8_t*)&val;
            valBytes[0] = byteArr[6];
            valBytes[1] = byteArr[5];
            valBytes[2] = byteArr[4];
            valBytes[3] = byteArr[3];
            LOG("Register %04x: %f", eastron_sdm72m_input_registers[curModbusReg], val);
        }

        // 20ms works as well, 10ms crashes the SDM72M
        sleep_ms(40);
    }

    /*
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
    }*/
};