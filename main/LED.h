#include "led_strip.h"
#include "GPIO.h"

// --- Configuration ---
#define LED_STRIP_GPIO_NUM  18          // ESP32 GPIO pin connected to the strip's DIN
#define LED_STRIP_LED_NUMBERS 1        // Number of LEDs in your strip
#define LED_STRIP_RMT_CHANNEL RMT_CHANNEL_0 // RMT channel to use (0-7)

void configure_led_strip(void);