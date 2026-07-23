/* Include guard ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#ifndef i2c_master_h
#define i2c_master_h

/* Includes ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#include "esp_system.h"
#include "driver/i2c_master.h"



/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Includes */

typedef struct nevo_driver_t {
	//i2c_slave_t		dev;
	bool					initialized;
	bool 					detected;
} nevo_driver_t;

/* Public defines ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#define I2C_MASTER_PORT	I2C_NUM_0
#define I2C_MASTER_TIMEOUT_MS       5000

#define I2C_DEVICE_ADDR	        120
#define I2C_MASTER_FREQ_HZ       100000//   100000 /*!< I2C master clock frequency */
#define I2C_TOOL_TIMEOUT_VALUE_MS (50)

#define I2C_MASTER_TX_BUF 1024 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF 1024 /*!< I2C master doesn't need buffer */
#define WRITE_BIT I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ    /*!< I2C master read */
#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0                 /*!< I2C ack value */
#define NACK_VAL 0x1                /*!< I2C nack value */

#define I2C_MASTER_TX_BUF_DISABLE 0 
#define I2C_MASTER_RX_BUF_DISABLE 0 

#define CONFIG_I2C_MASTER_SDA         GPIO_NUM_5
#define CONFIG_I2C_MASTER_SCL         GPIO_NUM_4
/* Public function prototypes ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
//void i2c_master_init(i2c_master_bus_handle_t *bus_handle, i2c_master_dev_handle_t *dev_handle, uint8_t addr);
void i2c_master_init(i2c_master_bus_handle_t *bus_handle);
esp_err_t i2c_send_bytes(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t len);
esp_err_t i2c_write_bytes(i2c_master_dev_handle_t dev_handle, 	uint8_t reg_addr, uint8_t *data, size_t len);
esp_err_t i2c_read_bytes(i2c_master_dev_handle_t dev_handle, uint8_t reg, uint8_t *data, size_t len);
esp_err_t i2c_receive_bytes(i2c_master_dev_handle_t dev_handle, uint8_t *data, size_t len);
void i2c_add_device(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t *dev_handle, uint8_t addr);

#endif 

