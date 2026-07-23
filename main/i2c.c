
/* Includes ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#include <esp_log.h>
#include "i2c.h"
#include "freertos/FreeRTOS.h"
#include "main.h"
/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Includes */

/* Private defines ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

static const char *TAG = "I2C-MASTER";


/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */

void i2c_master_init(i2c_master_bus_handle_t *bus_handle)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_MASTER_PORT,
        .sda_io_num = CONFIG_I2C_MASTER_SDA,
        .scl_io_num = CONFIG_I2C_MASTER_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));
}


/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t i2c_read_bytes(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t *data, size_t len)
{
    if (!dev_handle || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_master_transmit_receive(dev_handle,  &reg, 1, data, len, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Data read failed: %s", esp_err_to_name(ret));
    }
	

    return ret;
}



/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t i2c_receive_bytes(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t len)
{
    if (!dev_handle || !data) {
        return ESP_ERR_INVALID_ARG;
    }

    int ret = i2c_master_receive(dev_handle, data, len, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Data read failed: %s", esp_err_to_name(ret));
    }

    return ret;
}


/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t i2c_write_bytes(i2c_master_dev_handle_t dev_handle, 	uint8_t reg_addr, uint8_t *data, 	size_t len)
{
	uint8_t *write_buf = malloc(len + 1);
	if (write_buf == NULL) {
		return ESP_ERR_NO_MEM;
	}

	// The first byte is the register address.
	write_buf[0] = reg_addr;
	// Copy the data bytes into the buffer after the register address.
	memcpy(&write_buf[1], data, len);

	// Transmit the buffer: register address followed by the data.
	esp_err_t ret = i2c_master_transmit(dev_handle, write_buf, len + 1, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));

	// Free the temporary buffer.
	free(write_buf);

	return ret;
}



/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t i2c_send_bytes(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t len)
{
	uint8_t *write_buf = malloc(len + 1);
	if (write_buf == NULL) {
		return ESP_ERR_NO_MEM;
	}
	// Copy the data bytes into the buffer after the register address.
	memcpy(&write_buf[0], data, len);

	// Transmit the buffer: register address followed by the data.
	esp_err_t ret = i2c_master_transmit(dev_handle, write_buf, len, pdMS_TO_TICKS(I2C_MASTER_TIMEOUT_MS));

	// Free the temporary buffer.
	free(write_buf);

	return ret;
}




/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
void i2c_add_device(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t *dev_handle, uint8_t addr) 
{
    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, dev_handle));
}

