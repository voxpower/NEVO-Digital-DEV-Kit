
/**
 * @file smbus.c
 *
 * SMBus diagrams are represented as a chain of "piped" symbols, using the following symbols:
 *
 *   - ADDR  : the 7-bit I2C address of a bus slave.
 *   - S     : the START condition sent by a bus master.
 *   - Sr    : the REPEATED START condition sent by a master.
 *   - P     : the STOP condition sent by a master.
 *   - Wr    : bit 0 of the address byte indicating a write operation. Value is 0.
 *   - Rd    : bit 0 of the address byte indicating a read operation. Value is 1.
 *   - R/W   : bit 0 of the address byte, indicating a read or write operation.
 *   - A     : ACKnowledge bit sent by a master.
 *   - N     : Not ACKnowledge bit set by a master.
 *   - As    : ACKnowledge bit sent by a slave.
 *   - Ns    : Not ACKnowledge bit set by a slave.
 *   - DATA  : data byte sent by a master.
 *   - DATAs : data byte sent by a slave.
 */

#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "smbus.h"


static const char *TAG = "SMBUS ";

i2c_master_bus_handle_t bus_handle = NULL;

/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
static bool _is_init(const smbus_info_t * smbus_info)
{
    bool ok = false;
    if (smbus_info != NULL) {
        if (smbus_info->init) {
            ok = true;
        } else {
            ESP_LOGE(TAG, "smbus_info is not initialised");
        }
    } else {
        ESP_LOGE(TAG, "smbus_info is NULL");
    }
    return ok;
}
/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
static esp_err_t _check_i2c_error(esp_err_t err)
{
    switch (err)
    {
    case ESP_OK:  // Success
        break;
    case ESP_ERR_INVALID_ARG:  // Parameter error
        ESP_LOGE(TAG, "I2C parameter error");
        break;
    case ESP_FAIL: // Sending command error, slave doesn't ACK the transfer.
        ESP_LOGE(TAG, "I2C no slave ACK");
        break;
    case ESP_ERR_INVALID_STATE:  // I2C driver not installed or not in master mode.
        ESP_LOGE(TAG, "I2C driver not installed or not master");
        break;
    case ESP_ERR_TIMEOUT:  // Operation timeout because the bus is busy.
        ESP_LOGE(TAG, "I2C timeout");
        break;
    default:
        ESP_LOGE(TAG, "I2C error %d", err);
    }
    return err;
}
/* ******************************************************************************************** */
/*
 * @brief We add retries to improve robutness
 * @param
 * @retval
 */
esp_err_t _write_bytes(const smbus_info_t *smbus_info, uint8_t command, uint8_t *data, size_t len)
{
    esp_err_t err = ESP_FAIL;

     if (_is_init(smbus_info) && data)
     {
         for (int attempt = 0; attempt < SMBUS_RETRIES; ++attempt)
        {
            err = i2c_write_bytes(smbus_info->dev_handle, command, data, len);
            if (err == ESP_OK)
                break;
            
            ESP_LOGW(TAG, "I2C write failed (attempt %d): %s", attempt + 1, esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(SMBUS_TIME_RETRIES_MS));         
        }
    }

    return err;
}
/* ******************************************************************************************** */
/*
 * @brief We add retries to improve robutness
 * @param
 * @retval
 */
esp_err_t _read_bytes(const smbus_info_t *smbus_info, uint8_t command, uint8_t *data, size_t len)
{
    esp_err_t err = ESP_FAIL;

    if (_is_init(smbus_info) && data)
    {
        for (int attempt = 0; attempt < SMBUS_RETRIES; ++attempt)
        {
            err = i2c_read_bytes(smbus_info->dev_handle, command, data, len);
            if (err == ESP_OK)
                break;
                
            ESP_LOGW(TAG, "I2C read failed (attempt %d): %s", attempt + 1, esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(SMBUS_TIME_RETRIES_MS));
        }
    }
    return err;
}
/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
void smbus_init(void)
{
    i2c_master_init(&bus_handle);
}
/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t smbus_add_device(smbus_info_t * smbus_info, i2c_address_t address)
{
    if (smbus_info != NULL)
    {
        //i2c_master_init(&bus_handle , &smbus_info->dev_handle, address);
        //i2c_master_init(&bus_handle);
        smbus_info->init = true;
    }
    else
    {
        ESP_LOGE(TAG, "smbus_info is NULL");
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t smbus_send_byte(const smbus_info_t * smbus_info, uint8_t data)
{
    // Protocol: [S | ADDR | Wr | As | DATA | As | P]
    esp_err_t err = ESP_FAIL;
    if (_is_init(smbus_info))
    {
        err = i2c_send_bytes(smbus_info->dev_handle, &data, 1);
    }
    return err;
}
/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t smbus_receive_byte(const smbus_info_t * smbus_info, uint8_t * data)
{
    // Protocol: [S | ADDR | Rd | As | DATAs | N | P]
    esp_err_t err = ESP_FAIL;
    if (_is_init(smbus_info))
    {
        err = i2c_receive_bytes(smbus_info->dev_handle, data, 1);
    }
    return err;
}
/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t smbus_write_byte(const smbus_info_t * smbus_info, uint8_t command, uint8_t data)
{
    // Protocol: [S | ADDR | Wr | As | COMMAND | As | DATA | As | P]
    return _write_bytes(smbus_info, command, &data, 1);
}
/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t smbus_write_word(const smbus_info_t * smbus_info, uint8_t command, uint16_t data)
{
    // Protocol: [S | ADDR | Wr | As | COMMAND | As | DATA-LOW | As | DATA-HIGH | As | P]
    uint8_t temp[2] = { data & 0xff, (data >> 8) & 0xff };
    return _write_bytes(smbus_info, command, temp, 2);
}
/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t smbus_read_byte(const smbus_info_t * smbus_info, uint8_t command, uint8_t * data)
{
    // Protocol: [S | ADDR | Wr | As | COMMAND | As | Sr | ADDR | Rd | As | DATA | N | P]
    return _read_bytes(smbus_info, command, data, 1);
}
/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t smbus_read_word(const smbus_info_t * smbus_info, uint8_t command, uint16_t * data)
{
    // Protocol: [S | ADDR | Wr | As | COMMAND | As | Sr | ADDR | Rd | As | DATA-LOW | A | DATA-HIGH | N | P]
    esp_err_t err = ESP_FAIL;
    uint8_t temp[2] = { 0 };
    if (data)
    {
        err = _read_bytes(smbus_info, command, temp, 2);
        if (err == ESP_OK)
        {
            *data = (temp[1] << 8) + temp[0];
        }
        else
        {
            *data = 0;
        }
    }
    return err;
}
/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t smbus_write_block(const smbus_info_t * smbus_info, uint8_t command, uint8_t * data, uint8_t len)
{
    // Protocol: [S | ADDR | Wr | As | COMMAND | As | LEN | As | DATA-1 | As | DATA-2 | As ... | DATA-LEN | As | P]
    esp_err_t err = ESP_FAIL;
    if (_is_init(smbus_info) && data)
    {
		uint8_t *write_buf = malloc(len + 1);
		if (write_buf == NULL) {
			return ESP_ERR_NO_MEM;
		}

		// Add the length as the first byte.
		write_buf[0] = len;
		// Copy the data bytes into the buffer after the length byte.
		memcpy(&write_buf[1], data, len);
        err = i2c_write_bytes(smbus_info->dev_handle, command, write_buf, len+1);
        // Free the temporary buffer.
		free(write_buf);
    }
    return err;
}

/* ******************************************************************************************** */
/*
 * @brief
 single write routing to handle cmd/byte/word or block.
 * @param
 smbus_cmd->command: smbus register
 smbus_cmd->data: pointer to data
 smbus_cmd->format: SIGNED = 0, UNSIGNED = 1, STRING = 3, BLOCK = 4
 smbus_cmd->len: number of bytes of data
 * @retval
 */
esp_err_t smbus_write(const smbus_info_t * smbus_info, smbus_cmd_t *smbus_cmd)
{
    // Protocol:
    // block write (len > 2) = [S | ADDR | Wr | As | COMMAND | As | LEN | As | DATA-1 | As | DATA-2 | As ... | DATA-LEN | As | P]
    // cmd/byte/word write (len: 0/1/2) = [S | ADDR | Wr | As | COMMAND | As | DATA-1 | As | DATA-2 | As ... | DATA-LEN | As | P]
    esp_err_t err = ESP_FAIL;
    
    if (_is_init(smbus_info) && smbus_cmd->data)
    {
		        
		uint8_t *write_buf = malloc(smbus_cmd->len + 1);
		if (write_buf == NULL) {
			return ESP_ERR_NO_MEM;
		}

		// Add the length as the first byte.
		write_buf[0] = smbus_cmd->len;
		
		// Copy the data bytes into the buffer after the length byte.
		memcpy(&write_buf[1], smbus_cmd->data, smbus_cmd->len);
		
		for (int attempt = 0; attempt < SMBUS_RETRIES; ++attempt)
        {
			if (smbus_cmd->format > UNSIGNED)		//STRING or BLOCK type. include LEN byte
			{
	        	err = i2c_write_bytes(smbus_info->dev_handle, smbus_cmd->command, write_buf, smbus_cmd->len+1);
			} else {						//SIGNEd or UNSIGNED. exclude LEN byte
	        	err = i2c_write_bytes(smbus_info->dev_handle, smbus_cmd->command, write_buf+1, smbus_cmd->len);
			}
			if (err == ESP_OK)
                break;
            
            ESP_LOGW(TAG, "I2C write failed (attempt %d): %s", attempt + 1, esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(SMBUS_TIME_RETRIES_MS));   
		}
        // Free the temporary buffer.
		free(write_buf);
    }
    return err;
}



/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t smbus_read_block(const smbus_info_t * smbus_info, uint8_t command, uint8_t * data, uint8_t * len)
{
    // Protocol: [S | ADDR | Wr | As | COMMAND | As | Sr | ADDR | Rd | As | LENs | A | DATA-1 | A | DATA-2 | A ... | DATA-LEN | N | P]
    
    esp_err_t err = ESP_FAIL;
    
    if (_is_init(smbus_info) && data && len)
    {
        err = _read_bytes(smbus_info, command, data, *len);
        if (err != ESP_OK)
        {
            *len = 0;
            return err;
        }
    }
    return err;
}

/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t smbus_read(const smbus_info_t * smbus_info, smbus_cmd_t *smbus_cmd)
{
    // Protocol:
    // block read (len > 2) = [S | ADDR | Wr | As | COMMAND | As | Sr | ADDR | Rd | As | LENs | A | DATA-1 | A | DATA-2 | A ... | DATA-LEN | N | P]
    // byte read (len: 1) = [S | ADDR | Wr | As | COMMAND | As | Sr | ADDR | Rd | As | DATA | N | P]
    // word read (len: 2) = [S | ADDR | Wr | As | COMMAND | As | Sr | ADDR | Rd | As | DATA-LOW | A | DATA-HIGH | N | P]
    esp_err_t err = ESP_FAIL;
    
    if (_is_init(smbus_info) && smbus_cmd->data && smbus_cmd->len)
    {
		for (int attempt = 0; attempt < SMBUS_RETRIES; ++attempt)
        {
			if (smbus_cmd->format > UNSIGNED)		//STRING or BLOCK type. include LEN byte
			{
				smbus_cmd->len++;		//add 1 byte for length byte
			}

			err = i2c_read_bytes(smbus_info->dev_handle, smbus_cmd->command, smbus_cmd->data, smbus_cmd->len);
	        
	        if (err == ESP_OK)
                break;
	        
	        ESP_LOGW(TAG, "I2C read failed (attempt %d): %s", attempt + 1, esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(SMBUS_TIME_RETRIES_MS));
        }
    }
    return err;
}


/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t smbus_i2c_write_block(const smbus_info_t * smbus_info, uint8_t command, uint8_t * data, size_t len)
{
    // Protocol: [S | ADDR | Wr | As | COMMAND | As | (DATA | As){*len} | P]
    return _write_bytes(smbus_info, command, data, len);
}
/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t smbus_i2c_read_block(const smbus_info_t * smbus_info, uint8_t command, uint8_t * data, size_t len)
{
    // Protocol: [S | ADDR | Wr | As | COMMAND | As | Sr | ADDR | Rd | As | (DATAs | A){*len-1} | DATAs | N | P]
    return _read_bytes(smbus_info, command, data, len);
}
