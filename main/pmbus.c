#include "esp_log.h"
#include "i2c.h"
#include "esp_log.h"
#include "cJSON.h"
#include "main.h"
#include "pmbus.h"
#include "debug.h"
#include "http.h"
#include "smbus.h"
#include "tasks.h"
#include "tasks_notifications.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h" 
#include "scpi-def.h"
#include "gpio.h"
#include "soc/gpio_reg.h"
#include <string.h>


static const char *TAG = "PMBUS";

extern EventGroupHandle_t SMBUS_events, smbus_init_events;
extern TaskHandle_t tasksHandlers[];
extern i2c_master_bus_handle_t bus_handle;
extern scpi_choice_def_t GPIO_options[];

smbus_info_t smbus_info[MAX_N_DEVICES];
smbus_info_t smbus_info_gencall;
pmbus_data_t pmbus_channels[MAX_N_DEVICES];
pmbus_device_t pmbus_device[MAX_N_DEVICES];
QueueHandle_t pmbusRxQueue;

//bool pmbusbusy = false;
static SemaphoreHandle_t pmbus_mutex = NULL; // Initialize to NULL
bool pmbus_devs_in_bus[I2C_MAX_ADDRESS];
uint8_t pmbuspostreturn[40];

extern bool ws_settings_flag;


pmbus_cmd_def_t pmbus_cmd_def[] =
{
	{ .command = OPERATION, .command_str = "OPERATION", .scpi_str = "OUTP:STAT ", .bytes = 1, .format = UNSIGNED, .scale = 128, .mask = 128, .offset = offsetof(pmbus_data_t, operation), .zone = true},
	{ .command = ON_OFF_CONFIG, .command_str = "ON_OFF_CONFIG", .bytes = 1,.format = UNSIGNED, .scale = 1, .mask = 1, .offset =  0},
	{ .command = CLEAR_FAULTS, .command_str = "CLEAR_FAULTS", .bytes = 0,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = ZONE_CONFIG, .command_str = "ZONE_CONFIG", .scpi_str = "INST:COUP ", .bytes = 2,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = offsetof(pmbus_data_t, zone)},
	{ .command = ZONE_ACTIVE, .command_str = "ZONE_ACTIVE", .bytes = 2,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = STORE_DEFAULT_ALL, .command_str = "STORE_DEFAULT_ALL", .scpi_str = "PMBUS 17,", .bytes = 0,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = RESTORE_DEFAULT_ALL, .command_str = "RESTORE_DEFAULT_ALL", .bytes = 0,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = STORE_DEFAULT_CODE, .command_str = "STORE_DEFAULT_CODE", .bytes = 1,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = RESTORE_DEFAULT_CODE, .command_str = "RESTORE_DEFAULT_CODE", .bytes = 1,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = CAPABILITY, .command_str = "CAPABILITY", .bytes = 1,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = VOUT_COMMAND, .command_str = "VOUT_COMMAND", .scpi_str = "VOLT ", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = offsetof(pmbus_data_t, vout), .zone = true},
	{ .command = VOUT_DROOP, .command_str = "VOUT_DROOP", .scpi_str = "VOLT:DRO ", .bytes = 2,.format = SIGNED, .scale = 1, .mask = 255, .offset = offsetof(pmbus_data_t, droop), .zone = true},
	{ .command = VOUT_OV_WARN_LIMIT, .command_str = "VOUT_OV_WARN_LIMIT", .scpi_str = "VOLT:LIM:HIGH ", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = offsetof(pmbus_data_t, overvoltage), .zone = true},
	{ .command = VOUT_UV_WARN_LIMIT, .command_str = "VOUT_UV_WARN_LIMIT", .scpi_str = "VOLT:LIM:LOW ", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = offsetof(pmbus_data_t, undervoltage), .zone = true},
	{ .command = IOUT_OC_FAULT_LIMIT, .command_str = "IOUT_OC_FAULT_LIMIT", .scpi_str = "CURR ", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = offsetof(pmbus_data_t, ilimit), .zone = true},
	{ .command = TON_MAX_FLT, .command_str = "TON_MAX_FLT", .scpi_str = "OUTP:STAT:TONM ", .bytes = 2,.format = SIGNED, .scale = 10, .mask = 255, .offset = offsetof(pmbus_data_t, ton), .zone = true},
	{ .command = STATUS_BYTE, .command_str = "STATUS_BYTE", .bytes = 1,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = STATUS_WORD, .command_str = "STATUS_WORD", .bytes = 2,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = STATUS_VOUT, .command_str = "STATUS_VOUT", .bytes = 1,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = STATUS_IOUT, .command_str = "STATUS_IOUT", .bytes = 1,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = STATUS_TEMP, .command_str = "STATUS_TEMP", .bytes = 1,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = STATUS_CML, .command_str = "STATUS_CML", .bytes = 1,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = READ_VOUT, .command_str = "READ_VOUT", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = 0},
	{ .command = READ_IOUT, .command_str = "READ_IOUT", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = 0},
	{ .command = READ_TEMP_1, .command_str = "READ_TEMP_1", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = 0},
	{ .command = READ_POUT, .command_str = "READ_POUT", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = 0},
	{ .command = MFR_MODEL, .command_str = "MFR_MODEL", .bytes = 8,.format = STRING, .scale = 1, .mask = 255, .offset = offsetof(pmbus_data_t, model)},
	{ .command = MFR_REVISION, .command_str = "MFR_REVISION", .bytes = 8,.format = STRING, .scale = 1, .mask = 255, .offset = offsetof(pmbus_data_t, hw_version)},
	{ .command = MFR_SERIAL, .command_str = "MFR_SERIAL", .bytes = 11,.format = STRING, .scale = 1, .mask = 255, .offset = offsetof(pmbus_data_t, serial)},
	{ .command = MFR_VOUT_MIN, .command_str = "MFR_VOUT_MIN", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = 0},
	{ .command = MFR_VOUT_MAX, .command_str = "MFR_VOUT_MAX", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = offsetof(pmbus_data_t, max_vout)},
	{ .command = MFR_IOUT_MAX, .command_str = "MFR_IOUT_MAX", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = 0},
	{ .command = MFR_POUT_MAX, .command_str = "MFR_POUT_MAX", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = 0},
	{ .command = MFR_TAMB_MAX, .command_str = "MFR_TAMB_MAX", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = 0},
	{ .command = MFR_TAMB_MIN, .command_str = "MFR_TAMB_MIN", .bytes = 2,.format = SIGNED, .scale = 100, .mask = 255, .offset = 0},
	{ .command = MFR_SMBUS_ADDRESS, .command_str = "MFR_SMBUS_ADDRESS", .scpi_str = "PMBUS 208,", .bytes = 1,.format = UNSIGNED, .scale = 1, .mask = 127, .offset = offsetof(pmbus_data_t, smbus_address)},
	{ .command = MFR_SETTINGS, .command_str = "MFR_SETTINGS", .scpi_str = "VOLT:SENS ", .bytes = 1,.format = UNSIGNED, .scale = 1, .mask = 1, .offset = offsetof(pmbus_data_t, settings), .zone = true},
	{ .command = MFR_FACTORY_RESET, .command_str = "MFR_FACTORY_RESET", .bytes = 0,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
	{ .command = MFR_READ_ALL, .command_str = "MFR_READ_ALL", .bytes = 12,.format = UNSIGNED, .scale = 1, .mask = 255, .offset = 0},
};

module_values_t module_values_def[] =
{
	{ .model_str = "OP1D", .def_vout = 500, .max_vout = 750, .max_overvoltage = 800, .def_overvoltage = 550, .max_undervoltage = 800, .def_undervoltage = 450, .max_ton = 5000, .def_ton = 150, .max_droop = 100, .def_droop = 0, .max_ilimit = 2875, .max_address = 127, .max_operation = 128, .max_settings = 1, .max_zone = 254},
	{ .model_str = "OP2D", .def_vout = 1200, .max_vout = 1500, .max_overvoltage = 1650, .def_overvoltage = 1320, .max_undervoltage = 1650, .def_undervoltage = 1080, .max_ton = 5000, .def_ton = 150, .max_droop = 150, .def_droop = 0, .max_ilimit = 1725, .max_address = 127, .max_operation = 128, .max_settings = 1, .max_zone = 254},
	{ .model_str = "OP3D", .def_vout = 2400, .max_vout = 3000, .max_overvoltage = 3300, .def_overvoltage = 2640, .max_undervoltage = 3300, .def_undervoltage = 2160, .max_ton = 5000, .def_ton = 150, .max_droop = 250, .def_droop = 0, .max_ilimit = 863, .max_address = 127, .max_operation = 128, .max_settings = 1, .max_zone = 254},
	{ .model_str = "OP4D", .def_vout = 4800, .max_vout = 6000, .max_overvoltage = 6300, .def_overvoltage = 5280, .max_undervoltage = 6300, .def_undervoltage = 4320, .max_ton = 5000, .def_ton = 150, .max_droop = 550, .def_droop = 0, .max_ilimit = 431, .max_address = 127, .max_operation = 128, .max_settings = 1, .max_zone = 254},
	{ .model_str = "OPA2D", .def_vout = 1200, .max_vout = 1500, .max_overvoltage = 1650, .def_overvoltage = 1320, .max_undervoltage = 1650, .def_undervoltage = 1080, .max_ton = 5000, .def_ton = 150, .max_droop = 100, .def_droop = 0, .max_ilimit = 2875, .max_address = 127, .max_operation = 128, .max_settings = 1, .max_zone = 254},
	{ .model_str = "OPA3D", .def_vout = 2400, .max_vout = 3000, .max_overvoltage = 3300, .def_overvoltage = 2640, .max_undervoltage = 3300, .def_undervoltage = 2160, .max_ton = 5000, .def_ton = 150, .max_droop = 150, .def_droop = 0, .max_ilimit = 1725, .max_address = 127, .max_operation = 128, .max_settings = 1, .max_zone = 254},
};

/* ********************************** */
/*
 * @brief  check for duplicate I2C address & finds next free address
 *
 * @param
 * uint8_t address, uint8_t indexunit
 * @retval
 *	uint8_t address 
 */
uint8_t check_address(uint8_t address, uint8_t indexunit)
{
    bool match;
    esp_err_t ret;
    
    if (address > 0x7f)	//Invalid address: >0x7f
	{
		#ifdef DEBUG_PMBUS                    
    		ESP_LOGI(TAG, "Address too high %d", address);    
		#endif
		return 0;
	}
	if (address == 0) { //Invalid address: General Call
		#ifdef DEBUG_PMBUS                    
    		ESP_LOGI(TAG, "Cannot assign General Call address", address);    
		#endif
		return 0;
	}
	//Valid address. Check if already used.
	#ifdef DEBUG_PMBUS                    
		ESP_LOGI(TAG, "Valid address. Checking for duplicate");    
	#endif
	for (int k = address; k < 0x80; k++) {	//Check higher addresses	

		for (int j = 0; j < MAX_N_DEVICES; j++)
		{
			if (pmbus_device[j].initialized && k == pmbus_channels[j].smbus_address)
			{
				#ifdef DEBUG_PMBUS                    
					ESP_LOGI(TAG, "Duplicate found in channel %d", j+1);    
				#endif
				match = true;
			}
		}
		if (match == false) //Free address found
		{
			#ifdef DEBUG_PMBUS                    
				ESP_LOGI(TAG, "Free address found at %d", k);    
			#endif
			if (k != address) 
			{
				//Any way to notify user that address has been changed?
				#ifdef DEBUG_PMBUS                    
	        		ESP_LOGI(TAG, "Channel %d address changed from %d to %d", indexunit + 1, address, k);    
				#endif
				//Need to program new address to device here
				ESP_LOGI(TAG, "Writing new address %d", k);
	        	ret = smbus_write_byte(&smbus_info_gencall, MFR_SMBUS_ADDRESS, k);
		        if (ret == ESP_OK) 
		        {
					#ifdef DEBUG_PMBUS             
			            ESP_LOGI(TAG, "I2C Write success to reg 0x%02X", MFR_SMBUS_ADDRESS);
			            ESP_LOG_BUFFER_HEXDUMP(TAG, &k, 1, ESP_LOG_INFO);
					#endif
					//Save new address to NVM?
				            
		        } else {
			        ESP_LOGE(TAG, "I2C write failed, error = 0x%x", ret);
			        //xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_CYCLE_ERROR);
		        }
			}
	        //Readback address
	        ESP_LOGI(TAG, "Reading back address");
	        ret = smbus_read_byte(&smbus_info_gencall, MFR_SMBUS_ADDRESS, &address);
		    if (ret == ESP_OK) {
				#ifdef DEBUG_PMBUS               
		            ESP_LOGI(TAG, "I2C Read success from reg 0x%02X", MFR_SMBUS_ADDRESS);
		            ESP_LOGI(TAG, "Device address is set to %d", address);
		            ESP_LOG_BUFFER_HEXDUMP(TAG, &address, 1, ESP_LOG_INFO);
				#endif    
	        }
	        ESP_LOGI(TAG, "Reading back finished");
			return k;		//return free address
		}
		match = false;	//Address already used. Try next address
	}
	
	for (int k = address-1; k > 0; k--) {	//Check lower addresses	
		for (int j = 0; j < MAX_N_DEVICES; j++)
		{
			if (pmbus_device[j].initialized && k == pmbus_channels[j].smbus_address)
			{
				match = true;
			}
		}
		if (match == false) //Free address found
		{
			//Any way to notify user that address has been changed?
			#ifdef DEBUG_PMBUS                    
        		ESP_LOGI(TAG, "Channel %d address changed from %d to %d", indexunit + 1, address, k);    
			#endif
			//Need to program new address to device here
        	ret = smbus_write_byte(&smbus_info_gencall, MFR_SMBUS_ADDRESS, k);
	        if (ret == ESP_OK) 
	        {
				#ifdef DEBUG_PMBUS             
		            ESP_LOGI(TAG, "I2C Write success to reg 0x%02X", MFR_SMBUS_ADDRESS);
		            ESP_LOG_BUFFER_HEXDUMP(TAG, &k, 1, ESP_LOG_INFO);
				#endif
	        } else {
		        ESP_LOGE(TAG, "I2C write failed, error = 0x%x", ret);
		        //xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_CYCLE_ERROR);
	        }
	        //Readback address
	        ESP_LOGI(TAG, "Reading back address");
	        ret = smbus_read_byte(&smbus_info_gencall, MFR_SMBUS_ADDRESS, &address);
		    if (ret == ESP_OK) {
				#ifdef DEBUG_PMBUS               
		            ESP_LOGI(TAG, "I2C Read success from reg 0x%02X", MFR_SMBUS_ADDRESS);
		            ESP_LOGI(TAG, "Device address is set to %d", address);
		            ESP_LOG_BUFFER_HEXDUMP(TAG, &address, 1, ESP_LOG_INFO);
				#endif    
	        }
	        ESP_LOGI(TAG, "Reading back finished");
	        
	        #ifdef SAVE_AUTOADDRESS
		        //Save new address to NVM? 
		        ret = smbus_write_byte(&smbus_info_gencall, STORE_DEFAULT_CODE, MFR_SMBUS_ADDRESS);
		        if (ret == ESP_OK) 
		        {
					#ifdef DEBUG_PMBUS             
			            ESP_LOGI(TAG, "I2C Write success to reg 0x%02X", MFR_SMBUS_ADDRESS);
			            ESP_LOG_BUFFER_HEXDUMP(TAG, &k, 1, ESP_LOG_INFO);
					#endif
	      
		        } else {
			        ESP_LOGE(TAG, "I2C write failed, error = 0x%x", ret);
			        //xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_CYCLE_ERROR);
		        }
		    #endif
			return k;	//return free address
		}
		match = false;	//Address already used. Try next address
	}
	//Everything else
	return 0;
}


/* ********************************** */
/*
 * @brief  Send data from POST
 *
 * @param
 * @retval
 *
 */
void pmbus_send_cmd(pmbus_cmd_t *cmd)
{
	ESP_LOGI(TAG, "pmbus_send_cmd: %d", cmd->command);

		if (xQueueSend(pmbusRxQueue, cmd, 0) != pdPASS)
	{
		// Log an error if the queue is full and the send failed
		ESP_LOGE(TAG, "pmbusRxQueue is full, failed to send command 0x%lX", cmd->command);
	}
}



/* ********************************** */
/*
 * @brief
 * @param
 * @retval
 */
void pmbus_init_default(void)
{  
    pmbus_mutex = xSemaphoreCreateMutex();
    if (pmbus_mutex == NULL) 
    {
		ESP_LOGE(TAG, "Failed to create pmbus_mutex");
	}
    
    for (int j = 0; j < MAX_N_DEVICES; j++)
    {    
        pmbus_channels[j].channel = j + 1;
        pmbus_device[j].StatByte = 0;
        pmbus_device[j].StatVout = 0;
    }
    smbus_init();
    i2c_add_device(bus_handle, &smbus_info_gencall.dev_handle, 0);
    smbus_info_gencall.init = true;
    xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_INIT_DONE); 
}


    
/* ********************************** */
/*
 * @brief
 * @param
 * @retval
 */
static void smbus_web_services_state_machine(int indexunit)   //only one channel
{
    esp_err_t ret;
    uint8_t datab[11], command = MFR_READ_ALL; //0xD8
    uint8_t len = 11;
    smbus_info_t temp_info;
    int reg;
    uint16_t reg_val = 0 ;
    scpi_bool_t res;
    
    //Create temporary i2c device for specified address
    i2c_add_device(bus_handle, &temp_info.dev_handle, pmbus_channels[indexunit].smbus_address);
    temp_info.init = true;
     
    ret = smbus_read_block(&temp_info, command, datab, &len);
    //Remove temporary i2c device
    i2c_master_bus_rm_device(temp_info.dev_handle);
    
    if (ret == ESP_OK) 
    {
        ESP_LOG_BUFFER_HEXDUMP(TAG, datab, len, ESP_LOG_INFO);
        pmbus_device[indexunit].voltage = ((uint16_t)datab[2] << 8) + (uint16_t)datab[1]; 
        pmbus_device[indexunit].current = ((uint16_t)datab[4] << 8) + (uint16_t)datab[3]; 
        pmbus_device[indexunit].power = ((uint16_t)datab[6] << 8) + (uint16_t)datab[5];
        pmbus_device[indexunit].temp = ((uint16_t)datab[8] << 8) + (uint16_t)datab[7]; 
        pmbus_device[indexunit].StatByte = datab[9];
        pmbus_device[indexunit].StatVout = datab[10];
        
        ESP_LOGI(TAG,"Sbyte: %d, Svout: %d",pmbus_device[indexunit].StatByte,pmbus_device[indexunit].StatVout);
        //Update SCPI ISUM registers
        if ((pmbus_device[indexunit].StatByte & STATBYTE_OCP) == STATBYTE_OCP) {reg_val |= ISUM_CV;	reg_val &= ~ISUM_CC;}
        else {reg_val |= ISUM_CC; reg_val &= ~ISUM_CV;}
        
        if ((pmbus_device[indexunit].StatByte & STATBYTE_OTP) == STATBYTE_OTP) {reg_val |= ISUM_OTW;} 
		else {reg_val &= ~ISUM_OTW;}

        if ((pmbus_device[indexunit].StatVout & STATVOUT_OVP) == STATVOUT_OVP) {reg_val |= ISUM_OVW;}
        else {reg_val &= ~ISUM_OVW;}
        
        if ((pmbus_device[indexunit].StatVout & STATVOUT_UVP) == STATVOUT_UVP) {reg_val |= ISUM_UVW;}
        else {reg_val &= ~ISUM_UVW;}
			
		if ((pmbus_device[indexunit].StatVout & STATVOUT_TON) == STATVOUT_TON) {reg_val |= ISUM_TON;}
		else {reg_val &= ~ISUM_TON;}
        
        if ((pmbus_device[indexunit].StatByte & STATBYTE_INH) == STATBYTE_INH)
			reg_val = ISUM_INH;		//All other bits 0
        
        reg = SCPI_REG_QUES_INST_ISUM1C + indexunit*3;
				
    	/* Set register */
    	ESP_LOGI(TAG,"Set Registers: %d, %d", reg, reg_val);
    	SCPI_RegSet(&scpi_http_context, reg, reg_val);
    	SCPI_RegSet(&scpi_TCP_context, reg, reg_val);
    	SCPI_RegSet(&scpi_usbtmc_context, reg, reg_val);
    } 
    else 
    {
        ESP_LOGE(TAG, "I2C Read failed, error = 0x%x", ret);
    }
}


/* ********************************** */
/*
 * @brief
 * @param
 * @retval
 */
void smbus_init_device(uint8_t indexunit)
{
    esp_err_t ret;
    uint8_t datab[10], command;
        
	REG_WRITE(GPIO_OUT_W1TS_REG, GPIO_options[indexunit].tag);
	ESP_LOGE(TAG, "GPIO %d LEVEL %d",GPIO_options[indexunit].tag, 1);
	command = MFR_SMBUS_ADDRESS;
	ret = smbus_read_byte(&smbus_info_gencall, command, datab);
	//Check for unit off flag here
	
	if (ret == ESP_OK) 
    {
#ifdef DEBUG_PMBUS               
        ESP_LOGI(TAG, "I2C Read success from reg 0x%02X", command);
        ESP_LOG_BUFFER_HEXDUMP(TAG, datab, 1, ESP_LOG_INFO);
#endif            
        ESP_LOGI(TAG, "Checking address");
        datab[0] = check_address(datab[0],indexunit);
		if (!datab[0])
		{
			ESP_LOGI(TAG, "Channel %d device invalid Address: %d", indexunit + 1, datab[0]);
		} else {
	        pmbus_channels[indexunit].smbus_address = datab[0];
	        smbus_read_device(indexunit);

#ifdef DEBUG_PMBUS                    
        	ESP_LOGI(TAG, "Channel %d device detected. Address: %d", indexunit + 1, pmbus_channels[indexunit].smbus_address);    
#endif	
		}			
	} else {
        smbus_info[indexunit].init = false;
        ESP_LOGE(TAG, "I2C Read failed, error = 0x%x", ret);
        ESP_LOGI(TAG, "Channel %d no device detected", indexunit + 1);
        //xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_CYCLE_ERROR);
    }
        
    REG_WRITE(GPIO_OUT_W1TC_REG, GPIO_options[indexunit].tag);	
	ESP_LOGE(TAG, "GPIO %d LEVEL %d",GPIO_options[indexunit].tag, 0);
}

void smbus_read_device(uint8_t indexunit)
{
	esp_err_t ret;
    uint8_t data[16] = {0};
    bool model_flag;
    smbus_cmd_t smbus_cmd;
    smbus_info_t temp_smbus_info; // Local variable
    
    //Create temporary i2c device for specified address
    i2c_add_device(bus_handle, &temp_smbus_info.dev_handle, pmbus_channels[indexunit].smbus_address);
    temp_smbus_info.init = true;
    
    uint8_t cmd_index_list[13] = {0,3,10,11,12,13,14,15,26,28,27,30,36};
    //OPERATION, ZONE_CONFIG, VOUT_COMMAND,VOUT_DROOP, VOUT_OV_WARN_LIMIT, VOUT_UV_WARN_LIMIT,IOUT_OC_FAULT_LIMIT
    //TON_MAX_FLT, MFR_MODEL, MFR_SERIAL, MFR_REVISION, MFR_VOUT_MAX, MFR_SETTINGS

	for (int j = 0; j < 13; j++)
        {
			smbus_cmd.command = pmbus_cmd_def[cmd_index_list[j]].command;
			smbus_cmd.format = pmbus_cmd_def[cmd_index_list[j]].format;
			smbus_cmd.data = data;
			smbus_cmd.len = pmbus_cmd_def[cmd_index_list[j]].bytes;
						
			ret = smbus_read(&temp_smbus_info, &smbus_cmd);
			
			if (ret == ESP_OK) 
			    {
					#ifdef DEBUG_PMBUS               
			        ESP_LOGI(TAG, "I2C Read success from reg 0x%02X", smbus_cmd.command);
			        ESP_LOG_BUFFER_HEXDUMP(TAG, smbus_cmd.data, smbus_cmd.len, ESP_LOG_INFO);
					#endif
					
					//Update pmbus_channels[ch]
					if (pmbus_cmd_def[cmd_index_list[j]].offset)		//Offset must be non-zero
					{
						pmbus_data_t *target_struct = &pmbus_channels[indexunit];
						char *base_byte_ptr = (char *)target_struct;
						char *target_member_ptr = base_byte_ptr + pmbus_cmd_def[cmd_index_list[j]].offset;
						
						if (smbus_cmd.format > UNSIGNED)		//STRING or BLOCK type. include LEN byte
						{
							memcpy(target_member_ptr, smbus_cmd.data+1, smbus_cmd.len-1);            
				        } else {
							memcpy(target_member_ptr, smbus_cmd.data, smbus_cmd.len);
				        }
				        
				        //check for valid model
				        if (smbus_cmd.command == MFR_MODEL)
				        {
					        model_flag = false;
					        for (int j = 0; j < NEVO_MODEL_N; j++)
							{
								if (!strcmp(module_values_def[j].model_str, pmbus_channels[indexunit].model))
								{
									pmbus_channels[indexunit].model_index =j;
									model_flag = true;
									break;
								}
							}
							if (!model_flag)
							{
						        ESP_LOGE(TAG, "Error: Model not recognised");
						        xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_CYCLE_ERROR);
						        break;
							}
						}
				        
						#ifdef DEBUG_PMBUS
						ESP_LOGI(TAG, "Channel %d Command : %s Data: ", indexunit + 1, pmbus_cmd_def[cmd_index_list[j]].command_str);   
						ESP_LOG_BUFFER_HEXDUMP(TAG, target_member_ptr, smbus_cmd.len, ESP_LOG_INFO);                    
						#endif 
					}
			    } else {
			        ESP_LOGE(TAG, "I2C Read failed, error = 0x%x", ret);
			        break;
			    }	
		}
	
	//Remove temporary i2c device
	i2c_master_bus_rm_device(temp_smbus_info.dev_handle);
	
    if (ret == ESP_OK) 
    {            
        ESP_LOGI(TAG, "DEVICE INITIALIZATION SUCCESS");       
		xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_CYCLE_COMPLETE);
		pmbus_device[indexunit].initialized = true;
    } else {
        ESP_LOGE(TAG, "DEVICE INITIALIZATION FAILED");
        xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_CYCLE_ERROR);   
    }
}



/* ********************************** */
 /*
  * @brief 
  * @param
  * @retval
  */
void pmbus_reset_state_machine() 
{
    for (int j = 0; j < MAX_N_DEVICES; j++)
    {
        pmbus_device[j].initialized = false;
    }
}


/* ********************************** */
 /*
  * @brief 
  * @param
  * @retval
  */

 esp_err_t pmbus_execute_command(pmbus_cmd_t *cmd) 
 {
	int16_t value_16 = *((int16_t *)cmd->value);
	#ifdef DEBUG_PMBUS 
	    ESP_LOGI(TAG, "CMD: %d", cmd->command);
	    ESP_LOGI(TAG, "Value: %d", value_16);
	    ESP_LOGI(TAG, "Address: %d", cmd->address);
		ESP_LOGI(TAG, "type: %d", cmd->r_w);
		ESP_LOGI(TAG, "format: %d", cmd->format);
		ESP_LOGI(TAG, "length: %d", cmd->len);
	#endif 
    xTaskNotifyGive(tasksHandlers[TASK_ID_WEB_SOCKET]);
    
    //Check for zone != 254
    //Set cmd->address = ZONE_WRITE_ADDR
    //Set zone active to Zone address
    
    smbus_info_t temp_smbus_info; // Local variable
    //Create temporary i2c device for specified address
    i2c_add_device(bus_handle, &temp_smbus_info.dev_handle, cmd->address);
    temp_smbus_info.init = true;    
    esp_err_t ret = ESP_OK;
	bool flagsign = UNSIGNED;		//Default unsigned

	smbus_cmd_t smbus_cmd;
	smbus_cmd.command = cmd->command;
	smbus_cmd.data = cmd->value;
	smbus_cmd.format = cmd->format;
	smbus_cmd.len = cmd->len;
	
	switch (cmd->r_w)
	{
		case SMB_MSG_TYPE_READ:
		{
            
            ret = smbus_read(&temp_smbus_info, &smbus_cmd);

            if (ret == ESP_OK) 
            {
				#ifdef  DEBUG_PMBUS
					ESP_LOGI(TAG, "smbus_read success");                   
	                ESP_LOG_BUFFER_HEXDUMP(TAG, smbus_cmd.data, smbus_cmd.len, ESP_LOG_INFO);
				#endif
				
				//Copy data to return variable                
                memset(pmbuspostreturn, '\0', sizeof(pmbuspostreturn));
                if (cmd->format == STRING) //String
                {
					memcpy(pmbuspostreturn, smbus_cmd.data+1, smbus_cmd.len-1);
				} else {
					memcpy(pmbuspostreturn, smbus_cmd.data, smbus_cmd.len);
				}
                
                xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_READ_COMPLETE);
            }
            else
            {	
				#ifdef  DEBUG_PMBUS
					ESP_LOGI(TAG, "smbus_read error");           
				#endif
                xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_READ_ERROR);
            }
            break;
		}
		case SMB_MSG_TYPE_WRITE:
		{
			ret = smbus_write(&temp_smbus_info, &smbus_cmd);
            if (ret == ESP_OK) 
            {
                ESP_LOGI(TAG, "smbus_write success");
                xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE);
    
            }
            else
            {
                ESP_LOGI(TAG, "smbus_write error");
                xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_ERROR);
            }
            break;
        }

        default:
            ret = ESP_ERR_NOT_SUPPORTED;
            break;
	}
    
	//Update pmbus_channels[ch]
	if (ret ==  ESP_OK && cmd->update.offset && cmd->update.update)		//R/W success & Offset non-zero & update true
	{

		char *base_byte_ptr = (char *)cmd->update.target_struct;
		char *target_member_ptr = base_byte_ptr + cmd->update.offset;
		
		if (smbus_cmd.format > UNSIGNED)		//STRING or BLOCK type. include LEN byte
		{
			memcpy(target_member_ptr, smbus_cmd.data+1, smbus_cmd.len-1);            
        } else {
			memcpy(target_member_ptr, smbus_cmd.data, smbus_cmd.len);
        } 

		//Trigger websocket send settings
		ws_settings_flag = true;
	}	   
				
    i2c_master_bus_rm_device(temp_smbus_info.dev_handle);
    return ret;
}


/* ********************************** */
 /*
  * @brief 
  * @param
  * @retval
  */
void pmbus_manager_task(void *pvParameter) 
{

    uint16_t dataw[5];
    uint8_t datab[5];
    uint32_t tam_rx;
	uint32_t index = 0;
	uint8_t car;
	EventBits_t uxBits;
	esp_err_t ret = ESP_OK;
	//static SemaphoreHandle_t pmbus_mutex;

    pmbusRxQueue = xQueueCreate (PMBUS_RX_DATA_LONG, sizeof (pmbus_cmd_t));

   	while (1) 
   	{	
		uxBits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE, pdFALSE, pdTRUE, 0); 

		if (((uxBits & SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE) == 0)) {		//Only scan if triggered by clearing READLL_COMPLETE
			if (xSemaphoreTake(pmbus_mutex, 0) == pdTRUE) 
        	{
				for (int j = 0; j < MAX_N_DEVICES; j++)
		        { 
		            if ((pmbus_device[j].initialized))
		            {
		                ESP_LOGI(TAG, "Channel :%d", j);
		                smbus_web_services_state_machine(j);
		            }
		        }
            xSemaphoreGive(pmbus_mutex);    
		    }
	        xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE);
		}
		
		uxBits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READDEVICES_COMPLETE, pdFALSE, pdTRUE, 0); 

		if (((uxBits & SMBUS_PROC_NOTIF_SMBUS_READDEVICES_COMPLETE) == 0)) {		//Only scan if triggered by clearing READDEVCES_COMPLETE
			ESP_LOGI(TAG, "READ ALL DEVICES START");
			if (xSemaphoreTake(pmbus_mutex, 0) == pdTRUE) 
    		{
				for (int j = 0; j < MAX_N_DEVICES; j++)
		        {
		            if ((pmbus_device[j].initialized))
		            {
						#ifdef  DEBUG_PMBUS                 
			                ESP_LOGI(TAG, "Channel :%d", j);
						#endif
		                smbus_read_device(j);
		            }
		        }
		    ESP_LOGI(TAG, "READ ALL DEVICES FINISH");
            xSemaphoreGive(pmbus_mutex);
	        }
	        ws_settings_flag = true;
	        xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READDEVICES_COMPLETE);
		}
        
		pmbus_cmd_t cmd_rx; 

	    if (xQueueReceive(pmbusRxQueue, &cmd_rx, 1) == pdPASS)
	    {
	        //pmbusbusy = true;
	        if (xSemaphoreTake(pmbus_mutex, pdMS_TO_TICKS(200)) == pdTRUE) 
    		{ 
		        #ifdef  DEBUG_PMBUS                 
	                ESP_LOGI(TAG, "Received PMBus Command: 0x%lX, Address: 0x%lX, Type: %d", 
		            cmd_rx.command, cmd_rx.address, cmd_rx.r_w);
				#endif
	
		        ret = pmbus_execute_command(&cmd_rx); // **Function call must be changed!**
		        
		        //pmbusbusy = false;
		        xSemaphoreGive(pmbus_mutex);
	        }
		}

        vTaskDelay(pdMS_TO_TICKS(SMBUS_TIME_THREAD));
   	}
   	return;
}
 
