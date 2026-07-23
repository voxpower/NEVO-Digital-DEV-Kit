#ifndef SMBUS_MANAGER_H
#define SMBUS_MANAGER_H
//#include "esp_system.h"
//#include "psa/crypto_se_driver.h"
#include "smbus.h"


#define PMBUS_RX_DATA_LONG  256
#define SMBUS_TIME_THREAD	10
#define SMBUS_WAIT_TIME 5000


#define STATBYTE_CML 0x02
#define STATBYTE_OTP 0x04
#define STATBYTE_OCP 0x10
#define STATBYTE_INH 0x40

#define STATVOUT_TON 0x04
#define STATVOUT_UVP 0x20
#define STATVOUT_OVP 0x40

enum 
{
    NO_DEVICE = false,
    DETECTED = true,
};

#define SMB_CMD_N	                    39

#define OPERATION	                    0x01   //unsigned byte
#define ON_OFF_CONFIG	                0x02   //unsigned byte
#define CLEAR_FAULTS	                0x03
#define ZONE_CONFIG	                    0x07   //unsigned word
#define ZONE_ACTIVE	                    0x08   //unsigned word
#define STORE_DEFAULT_ALL	            0x11
#define RESTORE_DEFAULT_ALL	            0x12
#define STORE_DEFAULT_CODE	            0x13
#define RESTORE_DEFAULT_CODE	        0x14
#define CAPABILITY	                    0x19  //unsigned byte
#define VOUT_COMMAND	                0x21
#define VOUT_DROOP	                    0x28
#define VOUT_OV_WARN_LIMIT	            0x42
#define VOUT_UV_WARN_LIMIT	            0x44
#define IOUT_OC_FAULT_LIMIT	            0x46
#define TON_MAX_FLT	            		0x62
#define STATUS_BYTE	                    0x78  //unsigned byte
#define STATUS_WORD	                    0x79  //unsigned word
#define STATUS_VOUT	                    0x7A  //unsigned byte
#define STATUS_IOUT	                    0x7B  //unsigned byte
#define STATUS_TEMP	                    0x7D  //unsigned byte
#define STATUS_CML	                    0x7E  //unsigned byte
#define READ_VOUT	                    0x8B
#define READ_IOUT	                    0x8C
#define READ_TEMP_1	                    0x8D
#define READ_POUT	                    0x96
#define MFR_MODEL	                    0x9A
#define MFR_REVISION	                0x9B
#define MFR_SERIAL	                    0x9E
#define MFR_VOUT_MIN	                0xA4
#define MFR_VOUT_MAX	                0xA5
#define MFR_IOUT_MAX	                0xA6
#define MFR_POUT_MAX	                0xA7
#define MFR_TAMB_MAX	                0xA8
#define MFR_TAMB_MIN	                0xA9
#define MFR_SMBUS_ADDRESS 	            0xD0
#define MFR_SETTINGS	                0xD1  //unsigned byte
#define MFR_FACTORY_RESET	            0xD2
#define MFR_READ_ALL	                0xD3

#define ZONE_WRITE_ADDR	                0x37

#define NEVO_MODEL_N	    6		//Number of known OP modules types (module_values_t)

typedef	struct
	{
		char model_str[9];
		int16_t def_vout;
		int16_t max_vout;
		int16_t max_overvoltage;
		int16_t def_overvoltage;
		int16_t max_undervoltage;
		int16_t def_undervoltage;
		int16_t max_ton;
		int16_t def_ton;
		uint16_t max_droop;
		uint16_t def_droop;
		int16_t max_ilimit;
		uint8_t max_address;
		uint16_t max_operation;
		uint16_t max_settings;
		uint16_t max_zone;
	} module_values_t;		//Known OP modules parameters

typedef enum {
    SMB_MSG_TYPE_READ,
	SMB_MSG_TYPE_WRITE,
} smb_msg_type_t;

typedef struct
{
	uint8_t channel;
	char model[9];
	char serial[12];
	char hw_version[9];
	uint8_t operation;
	uint16_t zone;
	int16_t vout;
	int16_t overvoltage;
	int16_t undervoltage;
	int16_t ton;
	int16_t droop;
	int16_t ilimit;
	uint8_t smbus_address;
	uint8_t settings;
	uint8_t model_index;
	int16_t max_vout;
} pmbus_data_t;    //Device settings parameters

typedef struct
{
	uint8_t channel;
	int16_t voltage;
	int16_t current;
	int16_t power;
	int16_t temp;
	uint8_t StatByte;
	uint8_t StatVout;
	bool initialized;
} pmbus_device_t;   //Device real time parameters


typedef struct
{
	uint8_t command;
	char * command_str;
	char * scpi_str;
	uint8_t bytes;
	smb_msg_type_t read;
	smb_msg_type_t write;
	uint8_t scale;
	uint16_t mask;
	uint8_t format;
	size_t offset;
	bool zone;
} pmbus_cmd_def_t;   //PMBus known commands

typedef struct
{
	bool update;
	size_t offset;
	pmbus_data_t *target_struct
} update_t;

typedef struct
{
	uint32_t command;
	uint32_t address;
	smb_msg_type_t r_w;
	uint8_t format;
	uint8_t value[32];
	uint8_t len;
	update_t update;
} pmbus_cmd_t;   //PMBus command

void pmbus_manager_task(void *pvParameter);
void pmbus_reset_state_machine(void);
void pmbus_send_cmd(pmbus_cmd_t *pmbus_cmd);
void pmbus_init_default(void);
void smbus_init_device(uint8_t indexunit);
void smbus_read_device(uint8_t indexunit);
uint8_t check_address(uint8_t address, uint8_t indexunit);

#endif
