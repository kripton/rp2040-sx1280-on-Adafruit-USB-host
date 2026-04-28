
/**
 * SX1280 High-Speed FLRC Receiver - Pico SDK
 * 
 * Receives 200-byte fixed-size frames as fast as possible with channel hopping
 * Counts packets/second without processing payload content
 * 
 * Pin Configuration: IDENTICAL to transmitter
 * - SPI0_TX (GP19) -> SX1280 MOSI
 * - SPI0_RX (GP16) -> SX1280 MISO
 * - SPI0_SCK (GP18) -> SX1280 CLK
 * - GP17 -> SX1280 NSS (CS)
 * - GP20 -> SX1280 RESET
 * - GP21 -> SX1280 BUSY
 * - GP22 -> SX1280 DIO1 (optional IRQ)
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

/* SX1280 Register Definitions - SAME as transmitter */
#define SX1280_CMD_SET_SLEEP              0x84
#define SX1280_CMD_SET_STANDBY            0x80
#define SX1280_CMD_SET_FS                 0x81
#define SX1280_CMD_SET_TX                 0x83
#define SX1280_CMD_SET_RX                 0x82
#define SX1280_CMD_SET_RXDUTYCYCLE        0x94
#define SX1280_CMD_GET_IRQSTATUS          0x12
#define SX1280_CMD_CLR_IRQSTATUS          0x97
#define SX1280_CMD_SET_RFFREQUENCY        0x86
#define SX1280_CMD_SET_PACKETTYPE         0x8A
#define SX1280_CMD_SET_MODPARAMS          0x8B
#define SX1280_CMD_SET_PACKETPARAMS       0x8C
#define SX1280_CMD_SET_DIOIRQPARAMS       0x8D
#define SX1280_CMD_WRITE_REGISTER         0x18
#define SX1280_CMD_READ_REGISTER          0x1D
#define SX1280_CMD_GET_STATUS             0xC0
#define SX1280_CMD_GET_RXBUFFERSTATUS     0x13
#define SX1280_CMD_GET_PACKETSTATUS       0x14

/* Pin Definitions - MATCH TRANSMITTER */
#define SPI_PORT           spi1
#define SPI_TX_PIN         15
#define SPI_RX_PIN         8
#define SPI_SCK_PIN        14
#define SPI_CSN_PIN        0
#define SPI_RESET_PIN      12
#define SPI_BUSY_PIN       5

/* 18 MHz SPI - MAX SX1280 SPEED */
#define SPI_CLOCK_SPEED    18000000

/* Receiver Configuration */
#define PAYLOAD_SIZE       200
#define NUM_CHANNELS       5
#define STATS_INTERVAL     100
#define DUTY_CYCLE_RX      1    // 1ms RX, 1ms standby per cycle
#define DUTY_CYCLE_STDBY   1

/* Channel table - EXACTLY matches transmitter */
static uint32_t channels[NUM_CHANNELS] = {
    2402000000UL, 2410000000UL, 2418000000UL, 
    2426000000UL, 2434000000UL
};

static uint8_t current_channel = 0;
static uint32_t packet_count = 0;
static uint32_t lost_packets = 0;
static uint32_t start_time_ms = 0;
static uint32_t channel_stats[NUM_CHANNELS] = {0};

/* Function prototypes */
void sx1280_spi_init(void);
void sx1280_reset(void);
void sx1280_cmd_write(uint8_t cmd, const uint8_t *buffer, uint16_t size);
void sx1280_cmd_read(uint8_t cmd, uint8_t *buffer, uint16_t size);
void sx1280_wait_busy(void);
void sx1280_init_flrc_rx(void);
void sx1280_set_frequency(uint32_t freq);
void sx1280_set_rx_mode(void);
uint32_t get_packet_rate(void);

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

void sx1280_reset(void) {
    gpio_put(SPI_RESET_PIN, 0);
    sleep_ms(10);
    gpio_put(SPI_RESET_PIN, 1);
    sleep_ms(10);
}

void sx1280_wait_busy(void) {
    uint32_t timeout = 10000;  // 10ms max
    while (gpio_get(SPI_BUSY_PIN) && timeout--) {
        sleep_us(1);
    }
}

void sx1280_cmd_write(uint8_t cmd, const uint8_t *buffer, uint16_t size) {
    sx1280_wait_busy();
    gpio_put(SPI_CSN_PIN, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    if (buffer && size) spi_write_blocking(SPI_PORT, (uint8_t*)buffer, size);
    gpio_put(SPI_CSN_PIN, 1);
    sx1280_wait_busy();
}

void sx1280_cmd_read(uint8_t cmd, uint8_t *buffer, uint16_t size) {
    sx1280_wait_busy();
    gpio_put(SPI_CSN_PIN, 0);
    spi_write_blocking(SPI_PORT, &cmd, 1);
    if (buffer && size) spi_read_blocking(SPI_PORT, 0, buffer, size);
    gpio_put(SPI_CSN_PIN, 1);
    sx1280_wait_busy();
}

void sx1280_init_flrc_rx(void) {
    uint8_t buf[8];

    /* Reset sequence */
    sx1280_reset();

    /* Standby RC */
    buf[0] = 0x00;
    sx1280_cmd_write(0x80, buf, 1);

    /* FLRC packet type */
    buf[0] = 0x04;
    sx1280_cmd_write(0x8A, buf, 1);

    /* FLRC params: 1300kbps, 1.2MHz BW */
    buf[0] = 0x0F; buf[1] = 0x07; buf[2] = 0x01; buf[3] = 0x00;
    sx1280_cmd_write(0x8B, buf, 4);

    /* Packet params: 200 bytes fixed */
    buf[0] = 0x10; buf[1] = 0x00; buf[2] = 0x00; buf[3] = PAYLOAD_SIZE;
    buf[4] = 0x01; buf[5] = 0x00;
    sx1280_cmd_write(0x8C, buf, 6);

    /* Initial frequency */
    sx1280_set_frequency(channels[0]);

    printf("SX1280 FLRC RX initialized - 18MHz SPI, %d bytes\n", PAYLOAD_SIZE);
}

void sx1280_set_frequency(uint32_t freq) {
    uint32_t frf = (freq * 67108864UL) / 1000000000UL;
    uint8_t buf[3] = {(frf>>16)&0xFF, (frf>>8)&0xFF, frf&0xFF};
    sx1280_cmd_write(0x86, buf, 3);
}

void sx1280_set_rx_mode(void) {
    /* Continuous RX */
    uint8_t buf[3] = {0xFF, 0xFF, 0xFF};
    sx1280_cmd_write(0x82, buf, 3);
}

int main(void) {
    stdio_init_all();
    sleep_ms(2000);

    printf("SX1280 FLRC Receiver - 18 MHz SPI\n");
    printf("Payload: %d bytes, Channels: %d\n\n", PAYLOAD_SIZE, NUM_CHANNELS);

    sx1280_spi_init();
    sx1280_init_flrc_rx();

    start_time_ms = to_ms_since_boot(get_absolute_time());
    printf("Starting reception...\n\n");

    uint32_t last_stats = 0;

    while (true) {
        /* Channel hop - sync with transmitter */
        current_channel = (current_channel + 1) % NUM_CHANNELS;
        sx1280_set_frequency(channels[current_channel]);

        /* RX mode */
        sx1280_set_rx_mode();

        /* Wait for packet */
        sx1280_wait_busy();

        /* Packet received - count only */
        channel_stats[current_channel]++;
        packet_count++;

        /* Standby for next hop */
        uint8_t buf[1] = {0x00};
        sx1280_cmd_write(0x80, buf, 1);

        /* Stats */
        if ((packet_count % STATS_INTERVAL) == 0) {
            uint32_t elapsed = to_ms_since_boot(get_absolute_time()) - start_time_ms;
            float rate = (float)packet_count * 1000.0f / elapsed;

            printf("Packets: %lu, Elapsed: %.0fms, Rate: %.1f pkt/s\n", 
                   packet_count, (float)elapsed, rate);

            /* Channel stats */
            if (packet_count % (STATS_INTERVAL * 5) == 0) {
                printf("Channels: [");
                for (int i = 0; i < NUM_CHANNELS; i++) {
                    printf("%lu", channel_stats[i]);
                    if (i < NUM_CHANNELS-1) printf(", ");
                }
                printf("]\n\n");
            }
        }
    }

    return 0;
}
