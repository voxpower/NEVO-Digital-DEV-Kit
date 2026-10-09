#include "LED.h"
#include "esp_log.h"
#include "soc/gpio_reg.h"

static const char *TAG = "LED";

led_strip_handle_t led_strip;

/**
 * @brief Initialize the addressable LED strip
 */
void configure_led_strip(void) {
    ESP_LOGI(TAG, "Initializing addressable LED strip");
    
    REG_WRITE(GPIO_OUT_W1TS_REG, (1ULL<<LED_ENABLE));	//Enable LED
    
    // LED strip configuration structure
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_NUM,
        .max_leds = LED_STRIP_LED_NUMBERS,
        .led_model = LED_MODEL_WS2812, // For WS2812B
        .flags.invert_out = false, 
    };

    // RMT backend configuration
    led_strip_rmt_config_t rmt_config = {
        //.clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000, // 10MHz resolution (100ns step)
        //.rmt_channel = LED_STRIP_RMT_CHANNEL,
    };

    // Create a new LED strip object
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    
    // Clear the strip
    ESP_ERROR_CHECK(led_strip_clear(led_strip));
    ESP_LOGI(TAG, "LED strip initialization complete");
}