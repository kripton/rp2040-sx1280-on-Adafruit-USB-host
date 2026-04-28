
/**
 * SX1280 High-Speed FLRC Transmitter - Pico SDK
 * 
 * Sends 200-byte fixed-size frames as fast as possible with channel switching
 * between each packet using SPI control.
 * 
 * Pin Configuration:
 * - SPI0_TX (GP19) -> SX1280 MOSI
 * - SPI0_RX (GP16) -> SX1280 MISO
 * - SPI0_SCK (GP18) -> SX1280 CLK
 * - GP17 -> SX1280 NSS (CS)
 * - GP20 -> SX1280 RESET
 * - GP21 -> SX1280 BUSY
 */

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

    #include "tusb_lwip_glue.h"
}


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#include "log.h"
#include "webserver.h"

#include "json/json.h"

// Super-globals (for all modules)
Log logger;
WebServer webServer;

Json::Value storage;

/* SX1280 Register Definitions */
#define SX1280_CMD_SET_SLEEP              0x84
#define SX1280_CMD_SET_STANDBY            0x80
#define SX1280_CMD_SET_FS                 0x81
#define SX1280_CMD_SET_TX                 0x83
#define SX1280_CMD_SET_RX                 0x82
#define SX1280_CMD_SET_RXDUTYCYCLE        0x94
#define SX1280_CMD_SET_CAD                0xC5
#define SX1280_CMD_SET_TXCONTINUOUSWAVE   0xD1
#define SX1280_CMD_SET_TXCONTINUOUSPREAMBLE 0xD2
#define SX1280_CMD_SET_PACKETTYPE         0x8A
#define SX1280_CMD_GET_PACKETTYPE         0x11
#define SX1280_CMD_SET_MODPARAMS          0x8B
#define SX1280_CMD_SET_PACKETPARAMS       0x8C
#define SX1280_CMD_SET_DIOIRQPARAMS       0x8D
#define SX1280_CMD_GET_IRQSTATUS          0x12
#define SX1280_CMD_CLR_IRQSTATUS          0x97
#define SX1280_CMD_SET_RFFREQUENCY        0x86
#define SX1280_CMD_SET_TXPOWER            0x8E
#define SX1280_CMD_WRITE_BUFFER           0x0E
#define SX1280_CMD_READ_BUFFER            0x1E
#define SX1280_CMD_SET_BUFFERBASEADDRESS  0x8F
#define SX1280_CMD_SET_AUTOFS             0x96
#define SX1280_CMD_GET_STATUS             0xC0
#define SX1280_CMD_GET_RXBUFFERSTATUS     0x13
#define SX1280_CMD_GET_PACKETSTATUS       0x14

/* Packet Type */
#define SX1280_PACKET_TYPE_FLRC           0x04

/* IRQ Flags */
#define SX1280_IRQ_TX_DONE                0x01
#define SX1280_IRQ_RX_DONE                0x02
#define SX1280_IRQ_PREAMBLE_DETECTED      0x04
#define SX1280_IRQ_SYNCWORD_VALID         0x08
#define SX1280_IRQ_HEADER_VALID           0x10
#define SX1280_IRQ_HEADER_ERROR           0x20
#define SX1280_IRQ_CRC_ERROR              0x40
#define SX1280_IRQ_CAD_DONE               0x80
#define SX1280_IRQ_CAD_DETECTED           0x100
#define SX1280_IRQ_RX_TX_TIMEOUT          0x200

/* Pin Definitions */
#define SPI_PORT           spi1
#define SPI_TX_PIN         15
#define SPI_RX_PIN         8
#define SPI_SCK_PIN        14
#define SPI_CSN_PIN        0
#define SPI_RESET_PIN      12
#define SPI_BUSY_PIN       5
#define SX1280_RX_EN_PIN   4
#define SX1280_TX_EN_PIN   13

/* SPI Clock Speed (18 MHz max for SX1280) */
#define SPI_CLOCK_SPEED    10000000  /* 10 MHz for safety */

/* FLRC Configuration */
#define FLRC_BANDWIDTH     1200000   /* 1.2 MHz */
#define FLRC_BITRATE       1300000   /* 1300 kbps */
#define TX_POWER           13        /* dBm */
#define PAYLOAD_SIZE       200       /* bytes */

/* Frequency hopping table (2.4 GHz ISM band) */
#define NUM_CHANNELS       5
static uint32_t channels[NUM_CHANNELS] = {
    2402000000,  /* 2.402 GHz */
    2410000000,  /* 2.410 GHz */
    2418000000,  /* 2.418 GHz */
    2426000000,  /* 2.426 GHz */
    2434000000   /* 2.434 GHz */
};

static uint8_t current_channel = 0;
static uint32_t packet_count = 0;
static uint32_t start_time_ms = 0;

/* Function Prototypes */
void sx1280_spi_init(void);
void sx1280_reset(void);
void sx1280_cmd_write(uint8_t cmd, const uint8_t *buffer, uint16_t size);
void sx1280_cmd_read(uint8_t cmd, uint8_t *buffer, uint16_t size);
void sx1280_write_register(uint16_t addr, const uint8_t *buffer, uint16_t size);
void sx1280_read_register(uint16_t addr, uint8_t *buffer, uint16_t size);
void sx1280_write_buffer(uint8_t offset, const uint8_t *buffer, uint16_t size);
void sx1280_init_flrc(void);
void sx1280_set_frequency(uint32_t freq);
void sx1280_transmit(const uint8_t *data, uint16_t size);
void sx1280_wait_busy(void);
uint8_t sx1280_get_status(void);

/**
 * Initialize SPI interface
 */
void sx1280_spi_init(void) {
    spi_init(SPI_PORT, SPI_CLOCK_SPEED);

    gpio_set_function(SPI_TX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_RX_PIN, GPIO_FUNC_SPI);
    gpio_set_function(SPI_SCK_PIN, GPIO_FUNC_SPI);

    gpio_init(SPI_CSN_PIN);
    gpio_set_dir(SPI_CSN_PIN, GPIO_OUT);
    gpio_put(SPI_CSN_PIN, 1);

    gpio_init(SPI_RESET_PIN);
    gpio_set_dir(SPI_RESET_PIN, GPIO_OUT);

    gpio_init(SPI_BUSY_PIN);
    gpio_set_dir(SPI_BUSY_PIN, GPIO_IN);
}

/**
 * Reset SX1280
 */
void sx1280_reset(void) {
    gpio_put(SPI_RESET_PIN, 0);
    sleep_ms(10);
    gpio_put(SPI_RESET_PIN, 1);
    sleep_ms(10);
    sx1280_wait_busy();
}

/**
 * Wait for BUSY pin to go low
 */
inline void sx1280_wait_busy(void) {
    while (gpio_get(SPI_BUSY_PIN)) {
        sleep_us(10);
    }
}

/**
 * Send command to SX1280
 */
void sx1280_cmd_write(uint8_t cmd, const uint8_t *buffer, uint16_t size) {
    sx1280_wait_busy();

    gpio_put(SPI_CSN_PIN, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    if (size > 0) {
        spi_write_blocking(SPI_PORT, (uint8_t *)buffer, size);
    }
    gpio_put(SPI_CSN_PIN, 1);

    sx1280_wait_busy();
}

/**
 * Read command response from SX1280
 */
void sx1280_cmd_read(uint8_t cmd, uint8_t *buffer, uint16_t size) {
    sx1280_wait_busy();

    gpio_put(SPI_CSN_PIN, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    spi_read_blocking(SPI_PORT, 0x00, buffer, size);
    gpio_put(SPI_CSN_PIN, 1);

    sx1280_wait_busy();
}

/**
 * Get device status
 */
uint8_t sx1280_get_status(void) {
    uint8_t status;
    sx1280_cmd_read(SX1280_CMD_GET_STATUS, &status, 1);
    return status;
}

/**
 * Initialize SX1280 for FLRC mode
 */
void sx1280_init_flrc(void) {
    uint8_t buf[8];

    /* Set to standby mode */
    buf[0] = 0x00;  /* STDBY_RC */
    sx1280_cmd_write(SX1280_CMD_SET_STANDBY, buf, 1);
    sleep_ms(5);

    /* Set packet type to FLRC */
    buf[0] = SX1280_PACKET_TYPE_FLRC;
    sx1280_cmd_write(SX1280_CMD_SET_PACKETTYPE, buf, 1);
    sleep_ms(5);

    /* Set modulation parameters for FLRC */
    /* BR=1300kbps, BW=1.2MHz, CR=1/1, BT=1.0 */
    buf[0] = 0x0F;  /* Bitrate: 1300 kbps */
    buf[1] = 0x07;  /* Bandwidth: 1.2 MHz */
    buf[2] = 0x01;  /* Coding rate 1/1 */
    buf[3] = 0x00;  /* BT (shaping) */
    sx1280_cmd_write(SX1280_CMD_SET_MODPARAMS, buf, 4);
    sleep_ms(5);

    /* Set packet parameters */
    /* PreambleLength=4, SyncWordLength=4, HeaderType=Fixed, PayloadLength=200, CRC=Enabled, Whitening=Enabled */
    buf[0] = 0x10;  /* Preamble length: 16 bits */
    buf[1] = 0x00;  /* Sync word length: 32 bits */
    buf[2] = 0x00;  /* Header type: Fixed length */
    buf[3] = PAYLOAD_SIZE;  /* Payload length: 200 bytes */
    buf[4] = 0x01;  /* CRC enabled */
    buf[5] = 0x00;  /* No invert IQ */
    sx1280_cmd_write(SX1280_CMD_SET_PACKETPARAMS, buf, 6);
    sleep_ms(5);

    /* Set DIO IRQ */
    buf[0] = 0x00;
    buf[1] = SX1280_IRQ_TX_DONE;  /* IRQ on TX done */
    buf[2] = SX1280_IRQ_TX_DONE;  /* TX done to DIO1 */
    buf[3] = 0x00;
    sx1280_cmd_write(SX1280_CMD_SET_DIOIRQPARAMS, buf, 4);
    sleep_ms(5);

    /* Set initial frequency */
    sx1280_set_frequency(channels[0]);

    /* Set TX power */
    buf[0] = TX_POWER;
    buf[1] = 0x00;  /* RAMP time */
    sx1280_cmd_write(SX1280_CMD_SET_TXPOWER, buf, 2);
    sleep_ms(5);

    /* Set buffer base address */
    buf[0] = 0x00;  /* TX base address */
    buf[1] = 0x00;  /* RX base address */
    sx1280_cmd_write(SX1280_CMD_SET_BUFFERBASEADDRESS, buf, 2);
    sleep_ms(5);
}

/**
 * Set RF frequency
 */
void sx1280_set_frequency(uint32_t freq) {
    uint8_t buf[3];

    /* Frequency = (Frf * 32MHz) / (2^25) */
    uint32_t frf = (freq << 10) / 31250;

    buf[0] = (frf >> 16) & 0xFF;
    buf[1] = (frf >> 8) & 0xFF;
    buf[2] = frf & 0xFF;

    sx1280_cmd_write(SX1280_CMD_SET_RFFREQUENCY, buf, 3);
}

/**
 * Write data to TX buffer and transmit
 */
void sx1280_transmit(const uint8_t *data, uint16_t size) {
    uint8_t buf[3];

    /* Write payload to buffer */
    sx1280_write_buffer(0x00, data, size);

    /* Set TX mode with no timeout (0xFFFFFF) */
    buf[0] = 0xFF;
    buf[1] = 0xFF;
    buf[2] = 0xFF;
    sx1280_cmd_write(SX1280_CMD_SET_TX, buf, 3);
}

/**
 * Write to buffer
 */
void sx1280_write_buffer(uint8_t offset, const uint8_t *buffer, uint16_t size) {
    sx1280_wait_busy();

    gpio_put(SPI_CSN_PIN, 0);
    uint8_t cmd = SX1280_CMD_WRITE_BUFFER;
    spi_write_blocking(SPI_PORT, &cmd, 1);
    spi_write_blocking(SPI_PORT, &offset, 1);
    spi_write_blocking(SPI_PORT, (uint8_t *)buffer, size);
    gpio_put(SPI_CSN_PIN, 1);

    sx1280_wait_busy();
}

/**
 * Main transmission loop
 */
int main(void) {
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

    // RX EN
    gpio_init(SX1280_RX_EN_PIN);
    gpio_set_dir(SX1280_RX_EN_PIN, GPIO_OUT);

    // TX EN
    gpio_init(SX1280_TX_EN_PIN);
    gpio_set_dir(SX1280_TX_EN_PIN, GPIO_OUT);

    gpio_init(18);
    gpio_set_dir(18, GPIO_OUT);

    sleep_ms(2000);

    printf("SX1280 High-Speed FLRC Transmitter\n");
    printf("Payload: %d bytes\n", PAYLOAD_SIZE);
    printf("Channels: %d\n\n", NUM_CHANNELS);

    /* Initialize SPI and SX1280 */
    sx1280_spi_init();
    sx1280_reset();
    sx1280_init_flrc();

    /* Create test packet */
    uint8_t payload[PAYLOAD_SIZE];
    memset(payload, 0xAA, PAYLOAD_SIZE);

    /* Timestamp start */
    start_time_ms = to_ms_since_boot(get_absolute_time());

    printf("Starting transmission...\n");

    while (true) {
        tud_task();

        webServer.cyclicTask(); // Make sure this is on core0 since it
                                // WILL halt core1 when writing to the flash!
                                // Handles USB-ETH traffic

        /* Change channel */
        current_channel = (current_channel + 1) % NUM_CHANNELS;
        sx1280_set_frequency(channels[current_channel]);

        gpio_put(18, 1);
        gpio_put(SX1280_TX_EN_PIN, 1);

        /* Transmit packet */
        sx1280_transmit(payload, PAYLOAD_SIZE);

        /* Wait for TX complete (poll BUSY pin) */
        sx1280_wait_busy();

        gpio_put(SX1280_TX_EN_PIN, 0);
        gpio_put(18, 0);

        /* Return to standby */
        uint8_t buf[1] = {0x00};
        sx1280_cmd_write(SX1280_CMD_SET_STANDBY, buf, 1);

        packet_count++;

        /* Print statistics every 100 packets */
        if ((packet_count % 100) == 0) {
            uint32_t elapsed_ms = to_ms_since_boot(get_absolute_time()) - start_time_ms;
            float packet_rate = (float)packet_count * 1000.0 / elapsed_ms;
            printf("Packets: %lu, Elapsed: %lu ms, Rate: %.1f pkt/s\n", 
                   packet_count, elapsed_ms, packet_rate);
        }
    }

    return 0;
}
