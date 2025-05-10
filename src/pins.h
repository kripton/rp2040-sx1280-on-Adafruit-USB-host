// GPIO numbers of the RP2040 chip,
// NOT hardware pin numbers of the pico module!
// All in one file for easy reference

/*
 * GPIO     DefaultFunc OurFunc                     Define
 * GPIO00   TX0         SX1280 CS                   PICO_DEFAULT_UART_TX_PIN
 * GPIO01   RX0         NONE (Silkscreen: D1, BZR)  PICO_DEFAULT_UART_RX_PIN
 * GPIO02   SDA (I2C1)  SDA                         PICO_DEFAULT_I2C_SDA_PIN
 * GPIO03   SCL (I2C1)  SCL                         PICO_DEFAULT_I2C_SCL_PIN
 * GPIO04   D4          SX1280 RX EN
 * GPIO05   D5          SX1280 RF BUSY
 * GPIO06   D6          SX1280 DIO3
 * GPIO07   BOOT BUTTON BOOT BUTTON / NA
 * GPIO08   MISO (SPI1) SX1280 MISO                 PICO_DEFAULT_SPI_RX_PIN
 * GPIO09   D9          SX1280 DIO2
 * GPIO10   D10         NC
 * GPIO11   D11         SX1280 DIO1
 * GPIO12   D12         SX1280 RF RST
 * GPIO13   D13 / LED   SX1280 TX EN                PICO_DEFAULT_LED_PIN
 * GPIO14   SCK (SPI1)  SX1280 SCK                  PICO_DEFAULT_SPI_SCK_PIN
 * GPIO15   MOSI (SPI1) SX1280 MOSI                 PICO_DEFAULT_SPI_TX_PIN
 * GPIO16   USB DATA+   BUZZER IN
 * GPIO17   USB DATA-   LED STRIP DATA OUT
 * GPIO18   USB PWR EN  LED STRIP PWR EN
 * GPIO19   ???         NC
 * GPIO20   NEOPIX PWR  NEOPIX PWR
 * GPIO21   NEOPIX DATA NEOPIX DATA
 * GPIO22   ???         NC
 * GPIO23   ???         NC
 * GPIO24   D24         DMX DATA (SP3485EN: DI / RO)
 * GPIO25   D25         DMX DIR (SP3485EN: 0 = RE, 1 = DE)
 * GPIO26   A0          A0 / BAT: VBAT - 10k - A0 - 10k - GND
 * GPIO27   A1          A1 / NC
 * GPIO28   A2          A2 / NC
 * GPIO29   A3          A3 / NC
 * 
 * On I2C: TI ADS1115IDGS, ADDR = GND = 1001000b
*/