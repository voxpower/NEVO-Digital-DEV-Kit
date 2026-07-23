
#include "board_api.h"
//#include "i2c_master.h"


#include "esp_private/periph_ctrl.h"
#include "esp_private/usb_phy.h"
#include "soc/usb_pins.h"
#include "driver/gpio.h"
#include "gpio.h"


static usb_phy_handle_t phy_hdl;
static const char *TAG = "BOARD";

uint32_t board_millis(void) 
{
  return ( ( ((uint64_t) xTaskGetTickCount()) * 1000) / configTICK_RATE_HZ );
}

// Get USB Serial number string from unique ID if available. Return number of character.
// Input is string descriptor from index 1 (index 0 is type + len)
size_t board_usb_get_serial(uint16_t desc_str1[], size_t max_chars) 
{
  uint8_t uid[16] TU_ATTR_ALIGNED(4);
  size_t uid_len;

  // TODO work with make, but not working with esp32s3 cmake
  if ( board_get_unique_id ) {
    uid_len = board_get_unique_id(uid, sizeof(uid));
  }else {
    // fixed serial string is 01234567889ABCDEF
    uint32_t* uid32 = (uint32_t*) (uintptr_t) uid;
    uid32[0] = 0x67452301;
    uid32[1] = 0xEFCDAB89;
    uid_len = 8;
  }

  if ( uid_len > max_chars / 2 ) uid_len = max_chars / 2;

  for ( size_t i = 0; i < uid_len; i++ ) {
    for ( size_t j = 0; j < 2; j++ ) {
      const char nibble_to_hex[16] = {
          '0', '1', '2', '3', '4', '5', '6', '7',
          '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
      };
      uint8_t const nibble = (uid[i] >> (j * 4)) & 0xf;
      desc_str1[i * 2 + (1 - j)] = nibble_to_hex[nibble]; // UTF-16-LE
    }
  }

  return 2 * uid_len;

//To use vox serialsuse code below. Size is too big.
//	// Instead of a fixed ID, use a variable string
//	size_t IDN_serials_len = strlen(IDN_serials);
//	memcpy(uid, IDN_serials, IDN_serials_len);
//	uid_len = IDN_serials_len;
//	
//	// Ensure the output buffer is large enough
//    if (uid_len > max_chars) {
//        uid_len = max_chars;
//    }
//
//    // Copy characters from the uid array to the uint16_t destination array
//    for (size_t i = 0; i < uid_len; i++) {
//        desc_str1[i] = (uint16_t)uid[i];
//    }
//    
//    // Null-terminate the string
//    desc_str1[uid_len] = 0;
//
//    return uid_len;
}

bool usb_init(void) {
  // Configure USB PHY
  usb_phy_config_t phy_conf = {
    .controller = USB_PHY_CTRL_OTG,
    .target = USB_PHY_TARGET_INT,

    .otg_mode = USB_OTG_MODE_DEVICE,

    .otg_speed = USB_PHY_SPEED_UNDEFINED,
  };

  usb_new_phy(&phy_conf, &phy_hdl);

  return true;
}

bool gpio_init(void) {
		//zero-initialize the config structure.
	    gpio_config_t io_conf = {};
	    //disable interrupt
	    io_conf.intr_type = GPIO_INTR_DISABLE;
	    //set as output mode
	    io_conf.mode = GPIO_MODE_OUTPUT;
	    //bit mask of the pins that you want to set,e.g.GPIO18/19
	    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	    //disable pull-down mode
	    io_conf.pull_down_en = 0;
	    //disable pull-up mode
	    io_conf.pull_up_en = 0;
	    //configure GPIO with the given settings
	    esp_err_t err = gpio_config(&io_conf);
	    if (err != ESP_OK) {
	        ESP_LOGE(TAG, "GPIO Config failed with error: %s", esp_err_to_name(err));
	        return false;
	    }
	    return true;
}

void board_init(void) 
{
	gpio_init();
	usb_init();
  //i2c_init();
}


