/*
 * MIT License
 *
 * Copyright (c) 2017 David Antliff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file smbus.h
 * @brief Interface definitions for the ESP32-compatible SMBus Protocol component.
 *
 * This component provides structures and functions that are useful for communicating
 * with SMBus-compatible I2C slave devices.
 */

 #ifndef SMBUS_H
 #define SMBUS_H
 
#include <stdbool.h>
#include "i2c.h"
#include "freertos/FreeRTOS.h"
 
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
 
 #define I2C_MASTER_PORT	            I2C_NUM_0
 #define CONFIG_I2C_MASTER_SDA         GPIO_NUM_5
 #define CONFIG_I2C_MASTER_SCL         GPIO_NUM_4
 #define I2C_MAX_ADDRESS	            0x7F
 
 #define SMBUS_TIMEOUT_MS       1000
 #define SMBUS_TIME_RETRIES_MS     200
 #define SMBUS_RETRIES           3
 
 
 #define WRITE_BIT      I2C_MASTER_WRITE
 #define READ_BIT       I2C_MASTER_READ
 #define ACK_CHECK      true
 #define NO_ACK_CHECK   false
 #define ACK_VALUE      0x0
 #define NACK_VALUE     0x1
 #define MAX_BLOCK_LEN  255  // SMBus v3.0 increases this from 32 to 255
 //#define MEASURE             // enable measurement and reporting of I2C transaction duration
 
 #define I2C_MASTER_FREQ_HZ       100000
 
 #define SMBUS_DEFAULT_TIMEOUT pdMS_TO_TICKS(1000)  ///< Default transaction timeout in ticks
 /**
  * @brief 7-bit or 10-bit I2C slave address.
  */
 typedef uint16_t i2c_address_t;
 
 /**
  * @brief Structure containing information related to the SMBus protocol.
  */
 typedef struct {
    bool init;                     ///< True if struct has been initialised  
    i2c_master_dev_handle_t dev_handle;

 } smbus_info_t;

typedef struct
{
	uint8_t command;
	uint8_t * data;
	uint8_t format;
	uint8_t len;
} smbus_cmd_t;   //smbus command

 enum 
{
    SIGNED = 0,
    UNSIGNED = 1,
    STRING = 2,
    BLOCK = 3
};

 void smbus_init(void);
 esp_err_t smbus_add_device(smbus_info_t * smbus_info, i2c_address_t address);
 
 /**
  * @brief Set the I2C timeout.
  *        I2C transactions that do not complete within this period are considered an error.
  * @param[in] smbus_info Pointer to initialised SMBus info instance.
  * @param[in] timeout Number of ticks to wait until the transaction is considered in error.
  * @return ESP_OK if successful, ESP_FAIL or ESP_ERR_* if an error occurred.
  */
 esp_err_t smbus_set_timeout(smbus_info_t * smbus_info, TickType_t  timeout);
/**
  * @brief Send a single byte to a slave device.
  * @param[in] smbus_info Pointer to initialised SMBus info instance.
  * @param[in] data Data byte to send to slave.
  * @return ESP_OK if successful, ESP_FAIL or ESP_ERR_* if an error occurred.
  */
 esp_err_t smbus_send_byte(const smbus_info_t * smbus_info, uint8_t data);
 
 /**
  * @brief Receive a single byte from a slave device.
  * @param[in] smbus_info Pointer to initialised SMBus info instance.
  * @param[out] data Data byte received from slave.
  * @return ESP_OK if successful, ESP_FAIL or ESP_ERR_* if an error occurred.
  */
 esp_err_t smbus_receive_byte(const smbus_info_t * smbus_info, uint8_t * data);
 
 /**
  * @brief Write a single byte to a slave device with a command code.
  * @param[in] smbus_info Pointer to initialised SMBus info instance.
  * @param[in] command Device-specific command byte.
  * @param[in] data Data byte to send to slave.
  * @return ESP_OK if successful, ESP_FAIL or ESP_ERR_* if an error occurred.
  */
 esp_err_t smbus_write_byte(const smbus_info_t * smbus_info, uint8_t command, uint8_t data);
 
 /**
  * @brief Write a single word (two bytes) to a slave device with a command code.
  *        The least significant byte is transmitted first.
  * @param[in] smbus_info Pointer to initialised SMBus info instance.
  * @param[in] command Device-specific command byte.
  * @param[in] data Data word to send to slave.
  * @return ESP_OK if successful, ESP_FAIL or ESP_ERR_* if an error occurred.
  */
 esp_err_t smbus_write_word(const smbus_info_t * smbus_info, uint8_t command, uint16_t data);
 
 /**
  * @brief Read a single byte from a slave device with a command code.
  * @param[in] smbus_info Pointer to initialised SMBus info instance.
  * @param[in] command Device-specific command byte.
  * @param[out] data Data byte received from slave.
  * @return ESP_OK if successful, ESP_FAIL or ESP_ERR_* if an error occurred.
  */
 esp_err_t smbus_read_byte(const smbus_info_t * smbus_info, uint8_t command, uint8_t * data);
 
 /**
  * @brief Read a single word (two bytes) from a slave device with a command code.
  *        The first byte received is the least significant byte.
  * @param[in] smbus_info Pointer to initialised SMBus info instance.
  * @param[in] command Device-specific command byte.
  * @param[out] data Data byte received from slave.
  * @return ESP_OK if successful, ESP_FAIL or ESP_ERR_* if an error occurred.
  */
 esp_err_t smbus_read_word(const smbus_info_t * smbus_info, uint8_t command, uint16_t * data);
 
 /**
  * @brief Write up to 255 bytes to a slave device with a command code.
   *        This uses a byte count to negotiate the length of the transaction.
  *        The first byte in the data array is transmitted first.
  * @param[in] smbus_info Pointer to initialised SMBus info instance.
  * @param[in] command Device-specific command byte.
  * @param[in] data Data bytes to send to slave.
  * @param[in] len Number of bytes to send to slave.
  * @return ESP_OK if successful, ESP_FAIL or ESP_ERR_* if an error occurred.
  */
 esp_err_t smbus_write_block(const smbus_info_t * smbus_info, uint8_t command, uint8_t * data, uint8_t len);
 
 
 /**
  * @brief Read up to 255 bytes from a slave device with a command code.
  *        This uses a byte count to negotiate the length of the transaction.
  *        The first byte received is placed in the first array location.
  * @param[in] smbus_info Pointer to initialised SMBus info instance.
  * @param[in] command Device-specific command byte.
  * @param[out] data Data bytes received from slave.
  * @param[in/out] len Size of data array, and number of bytes actually received.
  * @return ESP_OK if successful, ESP_FAIL or ESP_ERR_* if an error occurred.
  */
 esp_err_t smbus_read_block(const smbus_info_t * smbus_info, uint8_t command, uint8_t * data, uint8_t * len);
 
 /**
  * @brief Write bytes to a slave device with a command code.
  *        No byte count is used - the transaction lasts as long as the master requires.
  *        The first byte in the data array is transmitted first.
  *        This operation is not defined by the SMBus specification.
  * @param[in] smbus_info Pointer to initialised SMBus info instance.
  * @param[in] command Device-specific command byte.
  * @param[in] data Data bytes to send to slave.
  * @param[in] len Number of bytes to send to slave.
  * @return ESP_OK if successful, ESP_FAIL or ESP_ERR_* if an error occurred.
  */
 esp_err_t smbus_i2c_write_block(const smbus_info_t * smbus_info, uint8_t command, uint8_t * data, size_t len);
 
 /**
  * @brief Read bytes from a slave device with a command code (combined format).
  *        No byte count is used - the transaction lasts as long as the master requires.
  *        The first byte received is placed in the first array location.
  *        This operation is not defined by the SMBus specification.
  * @param[in] smbus_info Pointer to initialised SMBus info instance.
  * @param[in] command Device-specific command byte.
  * @param[out] data Data bytes received from slave.
  * @param[in/out] len Size of data array. If the slave fails to provide sufficient bytes, ESP_ERR_TIMEOUT will be returned.
  * @return ESP_OK if successful, ESP_FAIL or ESP_ERR_* if an error occurred.
  */
 esp_err_t smbus_i2c_read_block(const smbus_info_t * smbus_info, uint8_t command, uint8_t * data, size_t len);
 
 
 /**
  * @brief Write up to 255 bytes to a slave device with a command code.
   *        This uses a byte count to negotiate the length of the transaction.
  *        The first byte in the data array is transmitted first.
  * @param[in] smbus_info Pointer to initialised SMBus info instance.
  * @param[in] command Device-specific command byte.
  * @param[in] data Data bytes to send to slave.
  * @param[in] len Number of bytes to send to slave.
  * @return ESP_OK if successful, ESP_FAIL or ESP_ERR_* if an error occurred.
  */
 esp_err_t smbus_write(const smbus_info_t * smbus_info, smbus_cmd_t *smbus_cmd);
 
 /**
  * @brief Read up to 255 bytes from a slave device with a command code.
  *        This uses a byte count to negotiate the length of the transaction.
  *        The first byte received is placed in the first array location.
  * @param[in] smbus_info Pointer to initialised SMBus info instance.
  * @param[in] command Device-specific command byte.
  * @param[out] data Data bytes received from slave.
  * @param[in/out] len Size of data array, and number of bytes actually received.
  * @return ESP_OK if successful, ESP_FAIL or ESP_ERR_* if an error occurred.
  */
 esp_err_t smbus_read(const smbus_info_t * smbus_info, smbus_cmd_t *smbus_cmd);
 
 
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif  // SMBUS_H
 