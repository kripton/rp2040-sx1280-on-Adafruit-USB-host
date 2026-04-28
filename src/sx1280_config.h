/**
 * SX1280 Configuration Header
 * 
 * Modify these settings to customize transmitter behavior
 */

#ifndef SX1280_CONFIG_H
#define SX1280_CONFIG_H

/* ===== PIN CONFIGURATION ===== */
//#define SPI_PORT           spi0
//#define SPI_TX_PIN         19        /* Pico GP19 */
//#define SPI_RX_PIN         16        /* Pico GP16 */
//#define SPI_SCK_PIN        18        /* Pico GP18 */
//#define SPI_CSN_PIN        17        /* Pico GP17 */
//#define SPI_RESET_PIN      20        /* Pico GP20 */
//#define SPI_BUSY_PIN       21        /* Pico GP21 */

/* ===== SPI CONFIGURATION ===== */
#define SPI_CLOCK_SPEED    10000000  /* 10 MHz (max 18 MHz) */

/* ===== FLRC MODULATION ===== */
#define FLRC_BANDWIDTH     1200000   /* 1.2 MHz */
#define FLRC_BITRATE       1300000   /* 1300 kbps (max supported) */
#define TX_POWER           13        /* dBm, range: -18 to +13 */

/* ===== PACKET CONFIGURATION ===== */
#define PAYLOAD_SIZE       200       /* bytes */

/* ===== FREQUENCY HOPPING ===== */
#define NUM_CHANNELS       5

/* 2.4 GHz ISM Band Frequencies (Hz) */
#define CHANNEL_0          2402000000  /* 2.402 GHz */
#define CHANNEL_1          2410000000  /* 2.410 GHz */
#define CHANNEL_2          2418000000  /* 2.418 GHz */
#define CHANNEL_3          2426000000  /* 2.426 GHz */
#define CHANNEL_4          2434000000  /* 2.434 GHz */

/* ===== STATISTICS ===== */
#define STATS_UPDATE_INTERVAL  100   /* packets between status updates */

#endif