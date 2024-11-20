// GPIO numbers of the RP2040 chip,
// NOT hardware pin numbers of the pico module!
// All in one file for easy reference

// I2C
// TODO: Verify!
#define PIN_I2C_SCL     5
#define PIN_I2C_SDA     4

// On-board LEDs
#define PIN_LED_A        6 // PWM 3A
#define PIN_LED_B        7 // PWM 3B
#define PIN_LED_C        8 // PWM 4A
#define PIN_LED_D        9 // PWM 4B
#define PIN_LED_E       10 // PWM 5A
#define PIN_LED_F       11 // PWM 5B
#define PIN_LED_G       12 // PWM 6A
#define PIN_LED_H       13 // PWM 6B

// Measurement input & output
#define PIN_SIG_IN     15 // PWM 7B
#define PIN_SIG_OUT    16 // PWM 0A

// Pico's on-board, single-color status LED
// If a regular "Pico" is detected at runtime => Port 25
#define PIN_LED_PICO   25 // PWM 4A (same as LED_D)
