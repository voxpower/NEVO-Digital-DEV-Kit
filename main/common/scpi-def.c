/*-
 * BSD 2-Clause License
 *
 * Copyright (c) 2012-2018, Jan Breuer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file   scpi-def.c
 * @date   Thu Nov 15 10:58:45 UTC 2012
 *
 * @brief  SCPI parser test
 *
 *
 */

//#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/projdefs.h"
#include "main.h"
#include "scpi/error.h"
#include "scpi/parser.h"
#include "scpi-def.h"
#include "esp_log.h"
#include "debug.h"
#include "pmbus.h"
#include "GPIO.h"
#include "scpi/types.h"
#include "smbus.h"
#include "soc/gpio_reg.h"
#include "soc/soc.h"
#include "tasks_notifications.h"

static const char *TAG = "SCPI-DEF";

extern pmbus_device_t pmbus_device[];
extern pmbus_data_t pmbus_channels[];
extern module_values_t module_values_def[];
extern pmbus_cmd_def_t pmbus_cmd_def[];
extern EventGroupHandle_t SMBUS_events, smbus_init_events;
extern uint8_t pmbuspostreturn[40];

extern uint8_t SCPI_channel;
extern bool ws_settings_flag;


scpi_choice_def_t state_options[] = {
	    {"OFF", 0},
	    {"ON", 1},
	    SCPI_CHOICE_LIST_END /* termination of option list */
	};
	
scpi_choice_def_t level_options[] = {
	    {"LOW", 0},
	    {"HIGH", 1},
	    SCPI_CHOICE_LIST_END /* termination of option list */
	};

scpi_choice_def_t sense_options[] = {
	    {"EXT", 0},
	    {"INT", 1},
	    SCPI_CHOICE_LIST_END /* termination of option list */
	};

scpi_choice_def_t trigger_source[] = {
    {"BUS", 5},
    {"IMMediate", 6},
    {"EXTernal", 7},
    SCPI_CHOICE_LIST_END /* termination of option list */
};

scpi_choice_def_t channel_options[] = {
	    {"CH1", 0},
	    {"CH2", 1},
    	{"CH3", 2},
	    {"CH4", 3},
	    {"CH5", 4},
	    {"CH6", 5},
	    {"CH7", 6},
	    {"CH8", 7},
	    SCPI_CHOICE_LIST_END /* termination of option list */
	};

scpi_choice_def_t chan_special_options[] = {
	    {"CH1", 0},
	    {"CH2", 1},
    	{"CH3", 2},
	    {"CH4", 3},
	    {"CH5", 4},
	    {"CH6", 5},
	    {"CH7", 6},
	    {"CH8", 7},
	    {"MIN", 8},
	    {"MAX", 9},
	    {"DEF", 10},
	    SCPI_CHOICE_LIST_END /* termination of option list */
	};
	
scpi_choice_def_t GPIO_options[] = {
	    {"CH1", (1ULL<<INH1)},
	    {"CH2", (1ULL<<INH2)},
    	{"CH3", (1ULL<<INH3)},
	    {"CH4", (1ULL<<INH4)},
	    {"CH5", (1ULL<<INH5)},
	    {"CH6", (1ULL<<INH6)},
	    {"CH7", (1ULL<<INH7)},
	    {"CH8", (1ULL<<INH8)},
	    {"ALL", GPIO_ALL},
	    SCPI_CHOICE_LIST_END /* termination of option list */
	};

scpi_choice_def_t PMBUS_options[] = {
	    {"CH1", 0},
	    {"CH2", 1},
    	{"CH3", 2},
	    {"CH4", 3},
	    {"CH5", 4},
	    {"CH6", 5},
	    {"CH7", 6},
	    {"CH8", 7},
	    {"GC", 8},
	    {"DIR", 9},
	    {"ZONE", 10},
	    {"GRP", 11},
	    SCPI_CHOICE_LIST_END /* termination of option list */
	};
	
scpi_choice_def_t type_options[] = {
	    {"read_byte", 0},
	    {"read_word", 1},
	    {"read_block", 2},
	    {"read_string", 3},
	    {"send_byte", 4},
	    {"write_byte", 5},
	    {"write_word", 6},
	    {"write_block", 7},
	    SCPI_CHOICE_LIST_END /* termination of option list */
	};	

/* Helper Functions */

static inline uint8_t get_safe_SCPI_channel() {
    uint8_t channel;
    xSemaphoreTake(scpi_channel_mutex, portMAX_DELAY);
    channel = SCPI_channel;
    xSemaphoreGive(scpi_channel_mutex);
    return channel;
}

	
static scpi_result_t set_type_params(uint8_t type, pmbus_cmd_t *temp_cmd)
{
	switch (type)
	{
		case 0:	//read_byte
		{
			temp_cmd->r_w = SMB_MSG_TYPE_READ;
			temp_cmd->format = UNSIGNED;
			temp_cmd->len = 1;
			break;
		}
		case 1:	//read_word
		{
			temp_cmd->r_w = SMB_MSG_TYPE_READ;
			temp_cmd->format = UNSIGNED;
			temp_cmd->len = 2;
			break;
		}
		case 2:	//read_block
		{
			temp_cmd->r_w = SMB_MSG_TYPE_READ;
			temp_cmd->format = BLOCK;
			temp_cmd->len = 3;
			break;
		}
		case 3:	//read_string
		{
			temp_cmd->r_w = SMB_MSG_TYPE_READ;
			temp_cmd->format = STRING;
			temp_cmd->len = 3;
			break;
		}
		case 4:	//send_byte
		{
			temp_cmd->r_w = SMB_MSG_TYPE_WRITE;
			temp_cmd->format = UNSIGNED;
			temp_cmd->len = 0;
			break;
		}
		case 5:	//write_byte
		{
			temp_cmd->r_w = SMB_MSG_TYPE_WRITE;
			temp_cmd->format = UNSIGNED;
			temp_cmd->len = 1;
			break;
		}
		case 6:	//write_word
		{
			temp_cmd->r_w = SMB_MSG_TYPE_WRITE;
			temp_cmd->format = UNSIGNED;
			temp_cmd->len = 2;
			break;
		}
		case 7:	//write_block
		{
			temp_cmd->r_w = SMB_MSG_TYPE_WRITE;
			temp_cmd->format = BLOCK;
			temp_cmd->len = 3;
			break;
		}
		default:
		{
			return SCPI_RES_ERR;
			break;
		}
	}
	return SCPI_RES_OK;
} 

static scpi_result_t set_active_zone (scpi_t *context, uint8_t zone)
{
		pmbus_cmd_t pmbus_cmd_struct;
		pmbus_cmd_struct.address = ZONE_WRITE_ADDR;
		//Send ZONE_ACTIVE PMbus command here
	    pmbus_cmd_struct.command = ZONE_ACTIVE;
	    pmbus_cmd_struct.r_w = SMB_MSG_TYPE_WRITE;
	    pmbus_cmd_struct.format = UNSIGNED;
	   	pmbus_cmd_struct.value[0] = zone;		//Zone write address.
	    pmbus_cmd_struct.value[1] = 0;			//Zone read address fixed.                   
	    pmbus_cmd_struct.len = 2;
		pmbus_cmd_struct.update.update = false;
	
		xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR);        
	
		pmbus_send_cmd(&pmbus_cmd_struct);
	
		EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));
		//wait the answer from pmbus
		
		if (bits & (SMBUS_PROC_NOTIF_WRITE_ERROR | SMBUS_PROC_NOTIF_READ_ERROR))
		{
			//return ESP_FAIL & push error;
			ESP_LOGI(TAG, "PMBus execute error: %d", bits);
			SCPI_ErrorPush(context, SCPI_ERROR_COMMUNICATION_ERROR);
		}
} 

static scpi_result_t scpi_set_handler(
	scpi_t *context,
    int pmbus_enum,
    scpi_unit_t unit,
    size_t max_offset,
    size_t def_offset,
    scpi_choice_def_t *choice)
{
    int16_t param;
    scpi_number_t param_var;
    scpi_bool_t res;
    scpi_parameter_t param_choice;
    int32_t value = 0;
    uint8_t channel;
    
    channel = get_safe_SCPI_channel();		//Default is INST:SEL channel
    
    //Array Indexing Lookup (Static Global Reference)
    const pmbus_cmd_def_t *cmd_def = &pmbus_cmd_def[pmbus_enum];
    const module_values_t *mod_vals = &module_values_def[pmbus_channels[channel].model_index];
    
    //Get scale factor
    double scale_factor = cmd_def->scale;
    
    //Get MAX and DEF values
    int16_t max_value,def_value = 0;
    memcpy(&max_value,((char*)mod_vals+max_offset),cmd_def->bytes);
    memcpy(&def_value,((char*)mod_vals+def_offset),cmd_def->bytes);
    
    //Check for Channel.eg. VOLT CH1,.....
	if (context->parser_state.numberOfParameters > 1)
	{
		res = SCPI_Parameter(context, &param_choice, TRUE);
	
		if (SCPI_ParamToChoice(context, &param_choice, channel_options, &value))
	    {
	    	if (value < 0 || value > 7) 
		    {
	            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
	            return SCPI_RES_ERR;
	        } else {
				if (pmbus_device[value].initialized)
				{
					channel = value;		//Set active channel
				} else {
					SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
					return SCPI_RES_ERR;
				}	
			}
	    }
    }
		
    if (!choice) 
    {
		res = SCPI_ParamNumber(context, scpi_special_numbers_def, &param_var, TRUE);
		if (!res) { SCPI_ErrorPush(context, SCPI_ERROR_MISSING_PARAMETER); return SCPI_RES_ERR; }
		
	    //Handle Special Numbers (MAX/DEF/MIN) 
		if (param_var.special) {
		    switch (param_var.content.tag) {
		    case SCPI_NUM_MAX: param = max_value; break;
		    case SCPI_NUM_MIN: param = 0; break;
		    case SCPI_NUM_DEF: param = def_value; break;
		    default: SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE); return SCPI_RES_ERR;
		    }
		} else {
			param = (param_var.content.value*scale_factor)+0.5;		//+0.5 to eliminate rounding error
			if (cmd_def->mask != 255)
			{
				param = param & cmd_def->mask;
			}
		    //Handle Numeric Value and Unit/Range Check
		    if (param_var.unit == SCPI_UNIT_NONE || param_var.unit == SCPI_UNIT_UNITLESS || param_var.unit == unit) {
				if (param_var.content.value < 0.0 || param > max_value) {
		            SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
		            //Trigger websocket send settings
					ws_settings_flag = true;
		            return SCPI_RES_ERR;
		        }
		    } else {
		        SCPI_ErrorPush(context, SCPI_ERROR_INVALID_SUFFIX);
		        return SCPI_RES_ERR;
		    }
		}
	
	} else {
		res = SCPI_Parameter(context, &param_choice, TRUE);
	
		if (res && param_choice.type == SCPI_TOKEN_PROGRAM_MNEMONIC) {
		    SCPI_ParamToChoice(context, &param_choice, choice, &value);
		} else if (res && param_choice.type == SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA){
			SCPI_ParamToInt32(context,&param_choice,&value);
			
			if (value < 0 || value > max_value) 
			{
				SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
	            return SCPI_RES_ERR;
	        } 
		} else {
			SCPI_ErrorPush(context, SCPI_ERROR_MISSING_PARAMETER);
			return SCPI_RES_ERR;
		}
	    if (SCPI_ParamErrorOccurred(context)) 
	    {
			SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
			return SCPI_RES_ERR;
		} else {
			param = (value*scale_factor)+0.5;		//+0.5 to eliminate rounding error;
		}
	}
    
    //Send PMBus Command using cmd_def reference
    pmbus_cmd_t pmbus_cmd_struct;
    pmbus_cmd_struct.command = cmd_def->command;
    pmbus_cmd_struct.r_w = SMB_MSG_TYPE_WRITE;
    pmbus_cmd_struct.format = cmd_def->format;
    value = (int16_t)param;
    memcpy(pmbus_cmd_struct.value, &value, cmd_def->bytes);
    pmbus_cmd_struct.len = cmd_def->bytes;
    
    //Zone write if zone != 254 and control command is zone type.
    if (((pmbus_channels[channel].zone & 255) != 254) && cmd_def->zone)
    {
		set_active_zone(context,pmbus_channels[channel].zone);
		pmbus_cmd_struct.address = ZONE_WRITE_ADDR;
		pmbus_cmd_struct.update.update = false;
	} else {
		pmbus_cmd_struct.address = pmbus_channels[channel].smbus_address;
    	pmbus_cmd_struct.update.update = true;
    	pmbus_cmd_struct.update.target_struct = &pmbus_channels[channel];
    	pmbus_cmd_struct.update.offset = cmd_def->offset;
	}
    
    xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE | SMBUS_PROC_NOTIF_SMBUS_READDEVICES_COMPLETE, pdFALSE, pdTRUE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));
    
    xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR);
    pmbus_send_cmd(&pmbus_cmd_struct);
	EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));
	//wait the answer from pmbus
	
	if (bits & SMBUS_PROC_NOTIF_WRITE_COMPLETE)
	{
		//Send response if requested from http
		if (context == &scpi_http_context)
			SCPI_ResultMnemonic(context, "OK\r\n");
			
		if (!pmbus_cmd_struct.update.update)	//Was zone command?
		{
			//Readback? all channels for specific command?
			for (int j = 0; j < MAX_N_DEVICES; j++)
			{
				if (!pmbus_device[j].initialized)
					continue;
					
				pmbus_cmd_struct.address = pmbus_channels[j].smbus_address;
				pmbus_cmd_struct.r_w = SMB_MSG_TYPE_READ;
				pmbus_cmd_struct.update.update = true;
				pmbus_cmd_struct.update.offset = cmd_def->offset;
				pmbus_cmd_struct.update.target_struct = &pmbus_channels[j];
				
				xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_READ_COMPLETE | SMBUS_PROC_NOTIF_READ_ERROR);

				pmbus_send_cmd(&pmbus_cmd_struct);

				EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_READ_COMPLETE | SMBUS_PROC_NOTIF_READ_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));
				//Wait
				
				if (bits & (SMBUS_PROC_NOTIF_WRITE_ERROR | SMBUS_PROC_NOTIF_READ_ERROR))  
				{			
					ESP_LOGE(TAG, "PMBus execute error: %d", bits);
				}
			}
		}
	}
	
	if (bits & (SMBUS_PROC_NOTIF_WRITE_ERROR | SMBUS_PROC_NOTIF_READ_ERROR))
	{
		ESP_LOGE(TAG, "PMBus execute error: %d", bits);
		SCPI_ErrorPush(context, SCPI_ERROR_COMMUNICATION_ERROR);
	}
    return SCPI_RES_OK;
}

static uint8_t scpi_get_channel_handler(scpi_t * context,uint8_t * channel) {
    
    scpi_bool_t res;
	scpi_parameter_t param;
	int32_t value = 0;
	
	res = SCPI_Parameter(context, &param, FALSE);
	
	if (res && param.type == SCPI_TOKEN_PROGRAM_MNEMONIC) 
	{
	    SCPI_ParamToChoice(context, &param, channel_options, &value);
	    
	    if (value < 0 || value > 7) 
	    {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        } else {
			if (pmbus_device[value].initialized)
			{
				channel[0] = value;		//Set active channel
			} else {
				SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
				return SCPI_RES_ERR;
			}	
		}
        
	} else {
		channel[0] = get_safe_SCPI_channel();		//Default to INST:SEL channel
	}
	
    if (SCPI_ParamErrorOccurred(context)) 
    {
		SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
		return SCPI_RES_ERR;
	}
	return SCPI_RES_OK;
}

static scpi_result_t scpi_get_handler(
	scpi_t * context,
	int pmbus_enum,
	size_t max_offset,
    size_t def_offset,
    scpi_choice_def_t *options)
{
    int32_t param;
    scpi_bool_t res;
	int32_t value = 0;
	uint8_t channel = get_safe_SCPI_channel();
	bool val_special = false;
	
	//Array Indexing Lookup (Static Global Reference)
    const pmbus_cmd_def_t *cmd_def = &pmbus_cmd_def[pmbus_enum];
    const module_values_t *mod_vals = &module_values_def[pmbus_channels[channel].model_index];
    
    //Get scale factor
    float scale_factor = (float)cmd_def->scale;
    
    //Get MAX and DEF values
    float max_value = (float)(*(uint16_t*)((char*)mod_vals + max_offset));
    float def_value = (float)(*(uint16_t*)((char*)mod_vals + def_offset));
    
	if (context->parser_state.numberOfParameters > context->input_count)
	{

	    if(SCPI_ParamChoice(context, chan_special_options,&param ,FALSE))
	    {
			if (param < 7)	//Chx
			{
				//Select channel
				if (pmbus_device[param].initialized)
				{
					channel = param;		//Set active channel
				} else {
					SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
					return SCPI_RES_ERR;
				}
				
				//Get next parameter if it exists.		
				if (context->parser_state.numberOfParameters > context->input_count)
				{
		    		if(SCPI_ParamChoice(context, scpi_special_numbers_def,&param ,FALSE))
					{
						val_special = true;
						//Check for MIN/MAX/DEF
		    			switch (param)
					    {
							case SCPI_NUM_MIN: value = 0; break;
					    	case SCPI_NUM_MAX: value = (int32_t)max_value; break;
					    	case SCPI_NUM_DEF: value = (int32_t)def_value; break;
					    	default: 
					        	SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
					        	return SCPI_RES_ERR;
					    }
				    }    
		    	}
				
			} else {	//Check for MIN/MAX/DEF
				val_special = true;
				param -= 7;
			    switch (param)
			    {
					case SCPI_NUM_MIN: value = 0; break;
			    	case SCPI_NUM_MAX: value = (int32_t)max_value; break;
			    	case SCPI_NUM_DEF: value = (int32_t)def_value; break;
			    	default: 
			        	SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
			        	return SCPI_RES_ERR;
			    }
			}
		}
	}

	if(!val_special)
	{
		//Get value from pmbus_channels[ch]
		if (cmd_def->offset)		//Offset must be non-zero
		{
			pmbus_data_t *target_struct = &pmbus_channels[channel];
			char *base_byte_ptr = (char *)target_struct;
			char *target_member_ptr = base_byte_ptr + cmd_def->offset;
			value = *((uint16_t *)target_member_ptr);
			
			if (cmd_def->mask != 255)
			{
				value = value & cmd_def->mask;
			}
		} else {	//No stored return value for this pmbus command. 
			SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
	        return SCPI_RES_ERR;
		}
	}
	
	if (!options)
	{
		SCPI_ResultFloat(context, (float)(value/scale_factor));	
	} else {
		const char * name;
		SCPI_ChoiceToName(options, (int32_t)(value/scale_factor), &name);
    	SCPI_ResultMnemonic(context, name);
	}

	return SCPI_RES_OK;
}

/* Command Callbacks */
	
static scpi_result_t MeasureVoltageQ(scpi_t * context) {
    
	uint8_t channel = 0;
	
	if (scpi_get_channel_handler (context,&channel) == SCPI_RES_OK)
	{
		//Trigger PMBus READALL by clearing READALL_COMPLETE
		xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE);
	    //Wait for result
	    xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE, pdFALSE, pdTRUE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));
	    //Send result
		SCPI_ResultFloat(context, (pmbus_device[channel].voltage/100.0));
	    return SCPI_RES_OK;
	} else {
		return SCPI_RES_ERR;
	}
}

static scpi_result_t MeasureCurrentQ(scpi_t * context) {
    
    uint8_t channel = 0;
	
	if (scpi_get_channel_handler (context,&channel) == SCPI_RES_OK)
	{
		//Trigger PMBus READALL by clearing READALL_COMPLETE
		xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE);
	    //Wait for result
	    xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE, pdFALSE, pdTRUE, pdMS_TO_TICKS(SMBUS_WAIT_TIME)); 
		//Send result
		SCPI_ResultFloat(context, (pmbus_device[channel].current/100.0));
	    return SCPI_RES_OK;
    } else {
		return SCPI_RES_ERR;
	}
}

static scpi_result_t MeasurePowerQ(scpi_t * context) {
    
    uint8_t channel = 0;

	if (scpi_get_channel_handler (context,&channel) == SCPI_RES_OK)
	{
		//Trigger PMBus READALL by clearing READALL_COMPLETE
		xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE);
	    //Wait for result
	    xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE, pdFALSE, pdTRUE, pdMS_TO_TICKS(SMBUS_WAIT_TIME)); 
		//Send result
		SCPI_ResultFloat(context, (pmbus_device[channel].power/10.0));
	    return SCPI_RES_OK;
	} else {
		return SCPI_RES_ERR;
	}
}

static scpi_result_t MeasureTempQ(scpi_t * context) {
    
    uint8_t channel = 0;

	if (scpi_get_channel_handler (context,&channel) == SCPI_RES_OK)
	{
		//Trigger PMBus READALL by clearing READALL_COMPLETE
		xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE);
	    //Wait for result
	    xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE, pdFALSE, pdTRUE, pdMS_TO_TICKS(SMBUS_WAIT_TIME)); 
		//Send result
		SCPI_ResultFloat(context, (pmbus_device[channel].temp/100.0));
	    return SCPI_RES_OK;
	} else {
		return SCPI_RES_ERR;
	}
}

static scpi_result_t SetVoltage(scpi_t * context) {

	//Call helper function here.
	return scpi_set_handler(context, 10, SCPI_UNIT_VOLT, offsetof(module_values_t, max_vout), offsetof(module_values_t, def_vout),NULL);	
}

static scpi_result_t SetVoltageQ(scpi_t * context) {
    
	//Call Get helper function here.
	return scpi_get_handler(context, 10, offsetof(module_values_t, max_vout), offsetof(module_values_t, def_vout),NULL);
}

static scpi_result_t SetVdroop(scpi_t * context) {

	//Call helper function here.
	return scpi_set_handler(context, 11, SCPI_UNIT_MILLIOHM, offsetof(module_values_t, max_droop), offsetof(module_values_t, def_droop),NULL);
}

static scpi_result_t SetVdroopQ(scpi_t * context) {
	
	//Call helper function here.
	return scpi_get_handler(context, 11, offsetof(module_values_t, max_droop), offsetof(module_values_t, def_droop),NULL);
}

static scpi_result_t SetCurrent(scpi_t * context) {

	//Call helper function here.
	return scpi_set_handler(context, 14, SCPI_UNIT_AMPER, offsetof(module_values_t, max_ilimit), offsetof(module_values_t, max_ilimit),NULL);
}

static scpi_result_t SetCurrentQ(scpi_t * context) {
	
	//Call helper function here.
	return scpi_get_handler(context, 14, offsetof(module_values_t, max_ilimit), offsetof(module_values_t, max_ilimit),NULL);
}

static scpi_result_t SetUV(scpi_t * context) {

	//Call helper function here.
	return scpi_set_handler(context, 13, SCPI_UNIT_VOLT, offsetof(module_values_t, max_undervoltage), offsetof(module_values_t, def_undervoltage),NULL);
}

static scpi_result_t SetUVQ(scpi_t * context) {
	
	//Call helper function here.
	return scpi_get_handler(context, 13, offsetof(module_values_t, max_undervoltage), offsetof(module_values_t, def_undervoltage),NULL);
}

static scpi_result_t SetOV(scpi_t * context) {
	
	//Call helper function here.
	return scpi_set_handler(context, 12, SCPI_UNIT_VOLT, offsetof(module_values_t, max_overvoltage), offsetof(module_values_t, def_overvoltage),NULL);
}

static scpi_result_t SetOVQ(scpi_t * context) {
	
	//Call helper function here.
	return scpi_get_handler(context, 12, offsetof(module_values_t, max_overvoltage), offsetof(module_values_t, max_overvoltage),NULL);
}

static scpi_result_t SetTon(scpi_t * context) {
    
	//Call helper function here.
	return scpi_set_handler(context, 15, SCPI_UNIT_SECOND, offsetof(module_values_t, max_ton), offsetof(module_values_t, def_ton),NULL);
}

static scpi_result_t SetTonQ(scpi_t * context) {
    
    //Call helper function here.
	return scpi_get_handler(context, 15, offsetof(module_values_t, max_ton), offsetof(module_values_t, def_ton),NULL);
}

/* ******************************************************************************************** */
/*
 * @brief
 * OUTP:COUP [CH],ZONE
 * Assign channel to zone.
 * @param
 *CH = channel CH1-CH8 (invalid parameter if channel not present)
 *ZONE = 0-254, 254 = No Zone
 *Sets "ZONE_CONFIG" on the channel
 * @retval
 */		
static scpi_result_t CHcouple(scpi_t * context) {

	//Call helper function here.
	return scpi_set_handler(context, 3, SCPI_UNIT_NONE, offsetof(module_values_t, max_zone), offsetof(module_values_t, max_zone),NULL);
}

static scpi_result_t CHcoupleQ(scpi_t * context) {
    
    //Call helper function here.
	return scpi_get_handler(context, 3, offsetof(module_values_t, max_zone), offsetof(module_values_t, max_zone),NULL);
}
	
static scpi_result_t SetState(scpi_t * context) {

	//Call helper function here.
	return scpi_set_handler(context, 0, SCPI_UNIT_NONE, offsetof(module_values_t, max_operation), offsetof(module_values_t, max_operation),state_options);
}

static scpi_result_t SetStateQ(scpi_t * context) {

	//Call helper function here.
	return scpi_get_handler(context, 0, offsetof(module_values_t, max_operation), offsetof(module_values_t, max_operation),state_options);
}
	
static scpi_result_t SnsSelect(scpi_t * context) {

	//Call helper function here.
	return scpi_set_handler(context, 36, SCPI_UNIT_NONE, offsetof(module_values_t, max_settings), offsetof(module_values_t, max_settings),sense_options);
}

static scpi_result_t SnsSelectQ(scpi_t * context) {

	//Call helper function here.
	return scpi_get_handler(context, 36, offsetof(module_values_t, max_settings), offsetof(module_values_t, max_settings),sense_options);
}




/* ******************************************************************************************** */
/*
 * @brief
 * INST:SEL CH
 * Select active channel.
 * @param
 *CH = channel CH1-CH8 (invalid parameter if channel not present)
 * @retval
 */	
static scpi_result_t CHselect(scpi_t * context) {
   
    scpi_bool_t res;
	scpi_parameter_t param;
	int32_t value = 0;
	
	res = SCPI_Parameter(context, &param, TRUE);
	
	if (res && param.type == SCPI_TOKEN_PROGRAM_MNEMONIC) 
	{
	    SCPI_ParamToChoice(context, &param, channel_options, &value);

	    if (value < 0 || value > 7) 
	    {
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;
        } else {
			if (pmbus_device[value].initialized)
			{
                xSemaphoreTake(scpi_channel_mutex, portMAX_DELAY);
				SCPI_channel = value;		//Set active channel
                xSemaphoreGive(scpi_channel_mutex);
			} else {
				SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
				return SCPI_RES_ERR;
			}	
    	
	    	//Send response if requested from http
			if (context == &scpi_http_context)
				SCPI_ResultMnemonic(context, "OK\r\n");
		}
        
	} else {
		SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
		return SCPI_RES_ERR;
	}
    if (SCPI_ParamErrorOccurred(context)) 
    {
		SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
		return SCPI_RES_ERR;
	}
	
    return SCPI_RES_OK;
}

static scpi_result_t CHselectQ(scpi_t * context) {
	
    uint8_t current_scpi_channel = get_safe_SCPI_channel();
	SCPI_ResultMnemonic(context, channel_options[current_scpi_channel].name);

    return SCPI_RES_OK;
}






/* ******************************************************************************************** */
/*
 * @brief
 * PMBUS [CH],CMD,DATA
 * Write to PMBUS. DATA is array for write_block type.
 * @param
 *[CH] channel CH1-CH8, invalid parameter if channel not present
 *channel defaults to INST:SEL channel if [CH] not present
 *GC - general call address, ZONE - Zone write address
 *
 *DIR - Direct: Breakout for any address or format. Format: ADD,CMD,TYPE,DATA. eg. PMBUS DIR,100,33,"write_word",500
 *		ADD - address, CMD - command, TYPEs - "send_byte","write_byte","write_word","write_block", DATA - data
 *
 *ZONE - zone command: sends command on zone write address. Format: ZONE,CMD,TYPE,DATA eg. PMBUS ZONE,1,33,"write_word",500
 *		ZONE - zone address, CMD - command, TYPEs - "send_byte","write_byte","write_word","write_block", DATA - data
 *
 *GRP - Group address protocol - Format: N, CMD, ADD1,ADD2,...ADDN, DATA1, DATA2,...DATAN. eg. PMBUS GRP,3,33,100,101,102,200,300,400 
 * @retval
 */
static scpi_result_t PMBus_CommandHandler(scpi_t *context) 
{
	
    int32_t value,chan,type;
    pmbus_cmd_t pmbus_cmd;
    scpi_bool_t res;
	scpi_parameter_t param;
	bool cmd_found;
	uint8_t current_scpi_channel = get_safe_SCPI_channel();
    
    #ifdef DEBUG_SCPI
		ESP_LOGI(TAG, "PMBus_CommandHandler\n");
	#endif
	pmbus_cmd.r_w = SMB_MSG_TYPE_WRITE;	//Default
	pmbus_cmd.value[0] = 0;				//Default
	pmbus_cmd.value[1] = 0;				//Default
	pmbus_cmd.len = 1;					//Default
	pmbus_cmd.format = UNSIGNED;		//Default.

	//Address
	res = SCPI_Parameter(context, &param, FALSE);

	if (res && param.type == SCPI_TOKEN_PROGRAM_MNEMONIC) 		//CHx,GC,DIR,ZONE,GRP
	{
	    SCPI_ParamToChoice(context, &param, PMBUS_options, &chan);

	    if (chan < 8)
	    {
			if (pmbus_device[chan].initialized)
				pmbus_cmd.address =  pmbus_channels[chan].smbus_address;
			else {
				SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
				return SCPI_RES_ERR;
			}	
		} else if (chan == 11) {	//Group command protocol
		//Not yet implemented
		//GRP – Group command protocol
		//Parameters: N, CMD, ADD1,ADD2,...ADDN, DATA1, DATA2,...DATAN. eg.
		//Examples:
		//PMBUS GRP,3,33,100,101,102,200,300,400
		//Writes the value 200 to register 33 (VOUT_COMMAND) on the device with SMBus address 100,
		//300 to register 33 on the device with SMBus address 101
		//and 400 to register 33 on the device with SMBus address 102.
		}		
	    	
	    else if (chan == 8)		//General Call
			pmbus_cmd.address = 0;	
	    else if (chan == 10)	//Zone
	    	pmbus_cmd.address = 55;
    	else if (chan == 9)	{	//DIRect
			//address
		    if (!SCPI_ParamInt(context, &pmbus_cmd.address, TRUE)) 
		    {
				SCPI_ErrorPush(context, SCPI_ERROR_MISSING_PARAMETER);
		    	return SCPI_RES_ERR;
		    }
		}
	    
	    //Command
	    if (!SCPI_ParamInt(context, &pmbus_cmd.command, TRUE)) 
	    {	
			SCPI_ErrorPush(context, SCPI_ERROR_MISSING_PARAMETER);
	    	return SCPI_RES_ERR;
	    }
	    
	    if (chan == 9)	{	//DIRect
	        //Type
	        
	        //Type
			SCPI_ParamChoice(context, type_options, &type, TRUE);
			
			if (set_type_params(type, &pmbus_cmd) == SCPI_RES_ERR)
			{
				SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_ERROR);
    			return SCPI_RES_ERR;
			}
			
			//Check for read type
			if (pmbus_cmd.r_w == SMB_MSG_TYPE_READ) {
				SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
    			return SCPI_RES_ERR;
			} 	
	    }

	} else if (res && param.type == SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA) {		//PMBUS CMD
		pmbus_cmd.address = pmbus_channels[current_scpi_channel].smbus_address;		//Default to INST:SEL channel.
		//Command
		SCPI_ParamToInt(context,&param,&pmbus_cmd.command);
	    
	} else {
		SCPI_ErrorPush(context, SCPI_ERROR_MISSING_PARAMETER);
    	return SCPI_RES_ERR;
	}
	
    if (SCPI_ParamErrorOccurred(context)) 
    {
		SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
		return SCPI_RES_ERR;
	}

	//Find command
	int cmd_index;
	cmd_found = false;
	
	for (cmd_index = 0; cmd_index < SMB_CMD_N; cmd_index++)
	{
		if (!(pmbus_cmd_def[cmd_index].command == pmbus_cmd.command))
			continue;
		
		if (chan != 9)	//Not DIRect. Use found command type
		{
			pmbus_cmd.r_w = SMB_MSG_TYPE_WRITE;
			pmbus_cmd.format = pmbus_cmd_def[cmd_index].format;
			pmbus_cmd.len = pmbus_cmd_def[cmd_index].bytes;
		}
		cmd_found=true;		
		break;
	}	
	

	//Value
	ESP_LOGI(TAG, "Value\n");
	//Array of uint8 for "block_write""	
	if (pmbus_cmd.format == BLOCK)
	{
		uint8_t value_array[32];
		uint8_t count = 0;
	    
	    // A loop to iterate through all value parameters in the list
	    while (count < 32) {
			if (!SCPI_ParamInt(context, &value, FALSE)) 
					break;

	    	value_array[count++] = (uint8_t)value;
		}
		       
		memcpy(pmbus_cmd.value,value_array,count);
		pmbus_cmd.len = count;
	
	} else {	//int16 for write_word
		
		SCPI_ParamInt(context, &value, FALSE); 
    	memcpy(pmbus_cmd.value, &value, 2);
	}
				
	//Send PMbus command here.
	uint32_t gpio_status;
	uint8_t address;
	address = pmbus_cmd.address;	//Record channel address for readback.
	pmbus_cmd.update.update = false;

	#ifdef DISABLE_4_SMBUSADDRESS
		if (pmbus_cmd.command == 208)
		{
			gpio_status = REG_READ(GPIO_OUT_REG) & GPIO_OUTPUT_PIN_SEL;		//Save GPIO status.
			REG_WRITE(GPIO_OUT_W1TC_REG, GPIO_options[8].tag);				//All units ON
			REG_WRITE(GPIO_OUT_W1TS_REG, GPIO_options[current_scpi_channel].tag);	//Specific unit OFF
			address = 0;													//Write on GenCall address.
		}
	#endif
	  
    xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR);        
    
    pmbus_send_cmd(&pmbus_cmd);
    
	EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));
	//wait the answer from pmbus
	
	if (bits & SMBUS_PROC_NOTIF_WRITE_COMPLETE) 
	{
		//Send response if requested from http
		if (context == &scpi_http_context)
			SCPI_ResultMnemonic(context, "OK\r\n");
			
		if (pmbus_cmd.command == 20)	//RESTORE_DEFAULT_CODE
		{
			//Readback CODE on all active channels.
	    	
	    	//Find command
	    	cmd_found=FALSE;
			for (cmd_index = 0; cmd_index < SMB_CMD_N; cmd_index++)
			{
				if (!(pmbus_cmd_def[cmd_index].command == pmbus_cmd.value[0]))
					continue;
				
				cmd_found=true;	
				break;
			}		
		}
		
		if ((pmbus_cmd.command == 18) | (pmbus_cmd.command == 210))	//RESTORE_DEFAULT_ALL or FACTORY RESET
		{
			//Readback all CMDs on all active channels.
			xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READDEVICES_COMPLETE);
			
		}
		
		//Update device regs. by readback on each channel
		pmbus_cmd.value[0] = 0;
		pmbus_cmd.value[1] = 0;
		
		//Was command MFR_SMBUS_ADDRESS?
		uint8_t op_status[MAX_N_DEVICES];
		
		if (pmbus_cmd.command == 208)
		{
			#ifndef DISABLE_4_SMBUSADDRESS
				gpio_status = REG_READ(GPIO_OUT_REG) & GPIO_OUTPUT_PIN_SEL;		//Save GPIO status.
			#endif
			
			for (int j = 0; j < MAX_N_DEVICES; j++)							//Save operation status
				{
					if (!pmbus_device[j].initialized)
						continue;
						
					op_status[j] = pmbus_channels[j].operation;
				}
			//Turn off all outputs.
			pmbus_cmd.address = 0;
			pmbus_cmd.command = OPERATION;
			pmbus_cmd.r_w = SMB_MSG_TYPE_WRITE;
			pmbus_cmd.format = UNSIGNED;
			pmbus_cmd.value[0] = 0; 
			pmbus_cmd.len = 1;
			pmbus_cmd.update.update = false;
			
			xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_ERROR | SMBUS_PROC_NOTIF_WRITE_COMPLETE);
			
			pmbus_send_cmd(&pmbus_cmd);

			EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));
			//Wait
			
			//Error Check??
			if (bits & SMBUS_PROC_NOTIF_WRITE_ERROR)  
			{			
				//return ESP_FAIL & push error;
				ESP_LOGE(TAG, "PMBus execute error: %d", bits);
				SCPI_ErrorPush(context, SCPI_ERROR_COMMUNICATION_ERROR);
			}
		}
			
		//Readback specific command on each initialized channel
		for (int j = 0; j < MAX_N_DEVICES; j++)
		{
			if (!pmbus_device[j].initialized)	//No channel present
				continue;
			
			if (!cmd_found)	//CMD wasn't found.
				continue;								
			
			if (!pmbus_cmd_def[cmd_index].offset)		//CMD does not need result
				continue;
			
			//Was command MFR_SMBUS_ADDRESS?
			if (pmbus_cmd_def[cmd_index].command == 208)
			{
				REG_WRITE(GPIO_OUT_W1TC_REG, GPIO_ALL);	//All outputs ON
				REG_WRITE(GPIO_OUT_W1TS_REG, GPIO_options[j].tag);	//Specific output OFF				
				address = 0;	//Readback on GenCall address.
			} else {
				address = pmbus_channels[j].smbus_address;
			}
			
			pmbus_cmd.address = address;
			pmbus_cmd.command = pmbus_cmd_def[cmd_index].command;
			pmbus_cmd.r_w = SMB_MSG_TYPE_READ;
			pmbus_cmd.format = UNSIGNED;
			pmbus_cmd.len = pmbus_cmd_def[cmd_index].bytes;
			
			pmbus_cmd.update.update = true;
			pmbus_cmd.update.offset = pmbus_cmd_def[cmd_index].offset;
			pmbus_cmd.update.target_struct = &pmbus_channels[j];
			
			xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_READ_ERROR | SMBUS_PROC_NOTIF_READ_COMPLETE);

			pmbus_send_cmd(&pmbus_cmd);
			
			EventBits_t bits = xEventGroupWaitBits(SMBUS_events,  SMBUS_PROC_NOTIF_READ_COMPLETE | SMBUS_PROC_NOTIF_READ_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));
			//Wait
			
			if (bits & SMBUS_PROC_NOTIF_READ_ERROR)  
			{			
				//return ESP_FAIL;
				ESP_LOGE(TAG, "PMBus execute error: %d", bits);
			}
		}
			
		//Restore GPIO & OPERATION states
		if (pmbus_cmd.command == 208)
		{
			REG_WRITE(GPIO_OUT_W1TC_REG, GPIO_ALL);		//All outputs ON
			REG_WRITE(GPIO_OUT_W1TS_REG, gpio_status);	//Recall GPIO status	
		
			for (int j = 0; j < MAX_N_DEVICES; j++)				//Recall operation status
			{
				if (!pmbus_device[j].initialized)
					continue; 
				
				pmbus_cmd.address = pmbus_channels[j].smbus_address;
				pmbus_cmd.command = OPERATION;
				pmbus_cmd.r_w = SMB_MSG_TYPE_WRITE;
				pmbus_cmd.format = UNSIGNED;
				pmbus_cmd.value[0] = op_status[j];
				pmbus_cmd.len = 1;
				pmbus_cmd.update.update = false;
				
				xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR);

				pmbus_send_cmd(&pmbus_cmd);

				EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE |  SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));
				//Wait
				
				//Error Check??
				if (bits & SMBUS_PROC_NOTIF_WRITE_ERROR)  
				{			
					//return ESP_FAIL & push error;
					ESP_LOGE(TAG, "PMBus execute error: %d", bits);
					SCPI_ErrorPush(context, SCPI_ERROR_COMMUNICATION_ERROR);
				}  
			}
		}
	}
		
	if (bits & SMBUS_PROC_NOTIF_WRITE_ERROR)  
	{			
		//return ESP_FAIL & push error;
		ESP_LOGE(TAG, "PMBus execute error: %d", bits);
		SCPI_ErrorPush(context, SCPI_ERROR_COMMUNICATION_ERROR);
	}  

	ESP_LOGI(TAG, "End PMbus handler: return OK");
	return SCPI_RES_OK;
}

/* ******************************************************************************************** */
/*
 * @brief
 * PMBUS? [CH],[CMD]
 * Read from PMBUS
 *
 * @param
*[CH] channel CH1-CH8, invalid parameter if channel not present
 *channel defaults to INST:SEL channel if [CH] not present
 *DIR - Direct: Breakout for any address or format. Format: ADD,CMD,TYPE[,BYTES]. eg. PMBUS ADD,100,33,"read_block",5
 *		[BYTES] - optional. Number of bytes to read for read_block/read_string.
 *TYPEs - "read_byte","read_word","read_string","read_block"
 *GC - Allowed but likely to return corrupted data.
 *ZONE and GRP not allowed for read 
 * @retval
 * if CH & CMD omitted. returns current INST:SEL SMBUS address
 */
static scpi_result_t PMBusQ(scpi_t *context) 
{
	
    int32_t value,chan,type;
    //const char *type_ptr = NULL;
    //size_t type_len;
    pmbus_cmd_t pmbus_cmd;
    scpi_bool_t res;
	scpi_parameter_t param;
	uint8_t current_scpi_channel = get_safe_SCPI_channel();
	
	pmbus_cmd.r_w = SMB_MSG_TYPE_READ;	//Default
	pmbus_cmd.format = UNSIGNED;		//Default
	pmbus_cmd.len = 1;					//Default
	pmbus_cmd.update.update = false;	//Default
	pmbus_cmd.value[0] = 0;				//Default
	pmbus_cmd.value[1] = 0;				//Default
	
	
	//Address
	res = SCPI_Parameter(context, &param, FALSE);
	
	if (res && param.type == SCPI_TOKEN_PROGRAM_MNEMONIC) 
	{
	    SCPI_ParamToChoice(context, &param, PMBUS_options, &chan);

	    if (chan < 8)
	    {
			if (pmbus_device[chan].initialized)
				pmbus_cmd.address =  pmbus_channels[chan].smbus_address;
			else {
				SCPI_ErrorPush(context, SCPI_ERROR_HARDWARE_MISSING);
				return SCPI_RES_ERR;
			}
		} else if (chan == 8)		//General Call
	    	pmbus_cmd.address = 0;	
		
		else if (chan == 9) {		//Need to handle DIRect
			
			//address
		    if (!SCPI_ParamInt(context, &pmbus_cmd.address, TRUE)) 
		    {
				SCPI_ErrorPush(context, SCPI_ERROR_MISSING_PARAMETER);
		    	return SCPI_RES_ERR;
		    }
	    
		} else {	//Read not allowed on ZONE, GC, GRP
            SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
            return SCPI_RES_ERR;			
		}
	    
	    //Command
	    if (!SCPI_ParamInt(context, &pmbus_cmd.command, FALSE))
	    {	
			if (chan < 8) //eg. PMBUS? CH1
			{
				SCPI_ResultInt8(context, pmbus_channels[chan].smbus_address);
    			return SCPI_RES_OK;
			}
			else 
			{
				SCPI_ErrorPush(context, SCPI_ERROR_MISSING_PARAMETER);
	    		return SCPI_RES_ERR;
			}
	    }

		if (chan == 9)	{	//DIRect
	        //Type
			SCPI_ParamChoice(context, type_options, &type, TRUE);
			
			if (set_type_params(type, &pmbus_cmd) == SCPI_RES_ERR)
			{
				SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_ERROR);
    			return SCPI_RES_ERR;
			}
			
			//Check for write type
			if (pmbus_cmd.r_w == SMB_MSG_TYPE_WRITE) {
				SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
    			return SCPI_RES_ERR;
    		}
			
			//Bytes
			if (pmbus_cmd.len > 2)	//block or string transaction? Get bytes.
			{
				if (SCPI_ParamInt(context, &value, TRUE)) 
				{
					memcpy(pmbus_cmd.value, &value, 2);
					pmbus_cmd.len = value;
				}	
			}	
	    }

	} else if (res && param.type == SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA) {
		pmbus_cmd.address = pmbus_channels[current_scpi_channel].smbus_address;		//Default to INST:SEL channel.
		//Command. eg. PMBUS? 33
		SCPI_ParamToInt(context,&param,&pmbus_cmd.command);
	    
	} else {	//eg. PMBUS?
		SCPI_ResultInt8(context, pmbus_channels[current_scpi_channel].smbus_address);
    	return SCPI_RES_OK;
	}
	
    if (SCPI_ParamErrorOccurred(context)) 
    {
		SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
		return SCPI_RES_ERR;
	}
	
	//Find command
	for (int i = 0; i < SMB_CMD_N; i++)
	{
		if (!(pmbus_cmd_def[i].command == pmbus_cmd.command))
			continue;
		
		pmbus_cmd.r_w = SMB_MSG_TYPE_READ;
		pmbus_cmd.format = pmbus_cmd_def[i].format;
		pmbus_cmd.len = pmbus_cmd_def[i].bytes;
	    break;
	}
	
    xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_READ_COMPLETE | SMBUS_PROC_NOTIF_READ_ERROR);        

    pmbus_send_cmd(&pmbus_cmd);
	EventBits_t bits = xEventGroupWaitBits(SMBUS_events,  SMBUS_PROC_NOTIF_READ_COMPLETE | SMBUS_PROC_NOTIF_READ_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));
	//wait the answer from pmbus
	
	if (bits & SMBUS_PROC_NOTIF_READ_COMPLETE) 
	{
		switch (pmbus_cmd.format)
		{
			case SIGNED:
			{
				//Send response
				int16_t result = *(int16_t *)pmbuspostreturn;
				SCPI_ResultInt16(context, result);
				break;
			}
			case UNSIGNED:
			{
				//Send response
				uint16_t result = *(uint16_t *)pmbuspostreturn;
				SCPI_ResultUInt16(context, result);
				break;
			}
			case STRING:
			{
				//Send response
				SCPI_ResultMnemonic(context, pmbuspostreturn);
				break;
			}
			case BLOCK:
			{
				//Send response
				SCPI_ResultArrayUInt8(context,pmbuspostreturn+1,pmbus_cmd.len,SCPI_FORMAT_ASCII);
			    break;
			}
		}
	
	}
		
	if (bits & SMBUS_PROC_NOTIF_READ_ERROR)  
	{			
		//return ESP_FAIL & push error;
		ESP_LOGE(TAG, "PMBus execute error: %d", bits);
		SCPI_ErrorPush(context, SCPI_ERROR_COMMUNICATION_ERROR);
	}  

	ESP_LOGI(TAG, "End PMbus handler: return OK");
	return SCPI_RES_OK;
}
	
    
/* ******************************************************************************************** */
/*
 * @brief
 * GPIO <CH1-CH8|ALL>,<HIGH|LOW,1|0>
 * @param
 * @retval
 */
static scpi_result_t GPIO_CommandHandler(scpi_t *context) 
{
    
    scpi_bool_t res;
	scpi_parameter_t param;
	int32_t value = 0,gpio = 0;
	uint32_t gpio_status; 
	
	//First parameter: Channel
	res = SCPI_Parameter(context, &param, TRUE);
	
	if (res && param.type == SCPI_TOKEN_PROGRAM_MNEMONIC) {
	    SCPI_ParamToChoice(context, &param, GPIO_options, &gpio);

	} else {
		SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
		return SCPI_RES_ERR;
	}
	
    if (SCPI_ParamErrorOccurred(context)) {
		SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
		return SCPI_RES_ERR;
	}
    
    //Second parameter: State
    res = SCPI_Parameter(context, &param, TRUE);
	
	if (res && param.type == SCPI_TOKEN_PROGRAM_MNEMONIC) {
	    SCPI_ParamToChoice(context, &param, level_options, &value);
	} else if (res && param.type == SCPI_TOKEN_DECIMAL_NUMERIC_PROGRAM_DATA) {
		SCPI_ParamToInt32(context,&param,&value);

		if (value < 0 || value > 1) 
		{
			SCPI_ErrorPush(context, SCPI_ERROR_DATA_OUT_OF_RANGE);
	        return SCPI_RES_ERR;
	    }
	} else {
		SCPI_ErrorPush(context, SCPI_ERROR_ILLEGAL_PARAMETER_VALUE);
		return SCPI_RES_ERR;
	}
    if (SCPI_ParamErrorOccurred(context)) {
		SCPI_ErrorPush(context, SCPI_ERROR_COMMAND);
		return SCPI_RES_ERR;
	}

	if (value)
	{
		REG_WRITE(GPIO_OUT_W1TS_REG, gpio);
	}	else {
		REG_WRITE(GPIO_OUT_W1TC_REG, gpio);
	}
    
    if (context == &scpi_http_context) 
    {
        SCPI_ResultMnemonic(context, "OK\r\n");
    }
    
    return SCPI_RES_OK;
}

/* ******************************************************************************************** */
/*
 * @brief
 * GPIO?
 * @param
 * @retval
 */
static scpi_result_t GPIO_CommandHandlerQ(scpi_t *context) 
{
    uint32_t gpioValue;
    uint8_t current_scpi_channel = get_safe_SCPI_channel();

	gpioValue = (REG_READ(GPIO_OUT_REG) & GPIO_options[current_scpi_channel].tag);
	
	if (gpioValue) {
		SCPI_ResultMnemonic(context, "HIGH\r\n");	
	} else {
		SCPI_ResultMnemonic(context, "LOW\r\n");
	}	
	
    return SCPI_RES_OK;
}



/* ******************************************************************************************** */
/* TEST callbacks */
static scpi_result_t TEST_ChoiceQ(scpi_t * context) {

    int32_t param;
    const char * name;

    if (!SCPI_ParamChoice(context, trigger_source, &param, TRUE)) {
        return SCPI_RES_ERR;
    }

    SCPI_ChoiceToName(trigger_source, param, &name);
    
	#ifdef DEBUG_SCPI
		ESP_LOGI(TAG, "\tP1=%s (%ld)\r\n", name, (long int) param);
	#endif
    SCPI_ResultInt32(context, param);

    return SCPI_RES_OK;
}

static scpi_result_t TEST_Numbers(scpi_t * context) {
    int32_t numbers[2];

    SCPI_CommandNumbers(context, numbers, 2, 1);

	#ifdef DEBUG_SCPI
		ESP_LOGI(TAG, "TEST numbers %ld %ld\r\n", numbers[0], numbers[1]);
	#endif
    return SCPI_RES_OK;
}

static scpi_result_t TEST_Text(scpi_t * context) {
    char buffer[100];
    size_t copy_len;

    if (!SCPI_ParamCopyText(context, buffer, sizeof (buffer), &copy_len, FALSE)) {
        buffer[0] = '\0';
    }

	#ifdef DEBUG_SCPI
		ESP_LOGI(TAG, "TEXT: ***%s***\r\n", buffer);
	#endif
    return SCPI_RES_OK;
}

static scpi_result_t TEST_ArbQ(scpi_t * context) {
    const char * data;
    size_t len;

    if (SCPI_ParamArbitraryBlock(context, &data, &len, FALSE)) {
        SCPI_ResultArbitraryBlock(context, data, len);
    }

    return SCPI_RES_OK;
}

struct _scpi_channel_value_t {
    int32_t row;
    int32_t col;
};
typedef struct _scpi_channel_value_t scpi_channel_value_t;

/**
 * @brief
 * parses lists
 * channel numbers > 0.
 * no checks yet.
 * valid: (@1), (@3!1:1!3), ...
 * (@1!1:3!2) would be 1!1, 1!2, 2!1, 2!2, 3!1, 3!2.
 * (@3!1:1!3) would be 3!1, 3!2, 3!3, 2!1, 2!2, 2!3, ... 1!3.
 *
 * @param channel_list channel list, compare to SCPI99 Vol 1 Ch. 8.3.2
 */
static scpi_result_t TEST_Chanlst(scpi_t *context) {
    scpi_parameter_t channel_list_param;
#define MAXROW 2    /* maximum number of rows */
#define MAXCOL 6    /* maximum number of columns */
#define MAXDIM 2    /* maximum number of dimensions */
    scpi_channel_value_t array[MAXROW * MAXCOL]; /* array which holds values in order (2D) */
    size_t chanlst_idx; /* index for channel list */
    size_t arr_idx = 0; /* index for array */
    size_t n, m = 1; /* counters for row (n) and columns (m) */

    /* get channel list */
    if (SCPI_Parameter(context, &channel_list_param, TRUE)) {
        scpi_expr_result_t res;
        scpi_bool_t is_range;
        int32_t values_from[MAXDIM];
        int32_t values_to[MAXDIM];
        size_t dimensions;

        bool for_stop_row = FALSE; /* true if iteration for rows has to stop */
        bool for_stop_col = FALSE; /* true if iteration for columns has to stop */
        int32_t dir_row = 1; /* direction of counter for rows, +/-1 */
        int32_t dir_col = 1; /* direction of counter for columns, +/-1 */

        /* the next statement is valid usage and it gets only real number of dimensions for the first item (index 0) */
        if (!SCPI_ExprChannelListEntry(context, &channel_list_param, 0, &is_range, NULL, NULL, 0, &dimensions)) {
            chanlst_idx = 0; /* call first index */
            arr_idx = 0; /* set arr_idx to 0 */
            do { /* if valid, iterate over channel_list_param index while res == valid (do-while cause we have to do it once) */
                res = SCPI_ExprChannelListEntry(context, &channel_list_param, chanlst_idx, &is_range, values_from, values_to, 4, &dimensions);
                if (is_range == FALSE) { /* still can have multiple dimensions */
                    if (dimensions == 1) {
                        /* here we have our values
                         * row == values_from[0]
                         * col == 0 (fixed number)
                         * call a function or something */
                        array[arr_idx].row = values_from[0];
                        array[arr_idx].col = 0;
                    } else if (dimensions == 2) {
                        /* here we have our values
                         * row == values_fom[0]
                         * col == values_from[1]
                         * call a function or something */
                        array[arr_idx].row = values_from[0];
                        array[arr_idx].col = values_from[1];
                    } else {
                        return SCPI_RES_ERR;
                    }
                    arr_idx++; /* inkrement array where we want to save our values to, not neccessary otherwise */
                    if (arr_idx >= MAXROW * MAXCOL) {
                        return SCPI_RES_ERR;
                    }
                } else if (is_range == TRUE) {
                    if (values_from[0] > values_to[0]) {
                        dir_row = -1; /* we have to decrement from values_from */
                    } else { /* if (values_from[0] < values_to[0]) */
                        dir_row = +1; /* default, we increment from values_from */
                    }

                    /* iterating over rows, do it once -> set for_stop_row = false
                     * needed if there is channel list index isn't at end yet */
                    for_stop_row = FALSE;
                    for (n = values_from[0]; for_stop_row == FALSE; n += dir_row) {
                        /* usual case for ranges, 2 dimensions */
                        if (dimensions == 2) {
                            if (values_from[1] > values_to[1]) {
                                dir_col = -1;
                            } else if (values_from[1] < values_to[1]) {
                                dir_col = +1;
                            }
                            /* iterating over columns, do it at least once -> set for_stop_col = false
                             * needed if there is channel list index isn't at end yet */
                            for_stop_col = FALSE;
                            for (m = values_from[1]; for_stop_col == FALSE; m += dir_col) {
                                /* here we have our values
                                 * row == n
                                 * col == m
                                 * call a function or something */
                                array[arr_idx].row = n;
                                array[arr_idx].col = m;
                                arr_idx++;
                                if (arr_idx >= MAXROW * MAXCOL) {
                                    return SCPI_RES_ERR;
                                }
                                if (m == (size_t)values_to[1]) {
                                    /* endpoint reached, stop column for-loop */
                                    for_stop_col = TRUE;
                                }
                            }
                            /* special case for range, example: (@2!1) */
                        } else if (dimensions == 1) {
                            /* here we have values
                             * row == n
                             * col == 0 (fixed number)
                             * call function or sth. */
                            array[arr_idx].row = n;
                            array[arr_idx].col = 0;
                            arr_idx++;
                            if (arr_idx >= MAXROW * MAXCOL) {
                                return SCPI_RES_ERR;
                            }
                        }
                        if (n == (size_t)values_to[0]) {
                            /* endpoint reached, stop row for-loop */
                            for_stop_row = TRUE;
                        }
                    }


                } else {
                    return SCPI_RES_ERR;
                }
                /* increase index */
                chanlst_idx++;
            } while (SCPI_EXPR_OK == SCPI_ExprChannelListEntry(context, &channel_list_param, chanlst_idx, &is_range, values_from, values_to, 4, &dimensions));
            /* while checks, whether incremented index is valid */
        }
        /* do something at the end if needed */
        /* array[arr_idx].row = 0; */
        /* array[arr_idx].col = 0; */
    }

    {
        size_t i;

        #ifdef DEBUG_SCPI
			ESP_LOGI(TAG, "TEST_Chanlst: ");
		#endif
        for (i = 0; i< arr_idx; i++) {
           
            #ifdef DEBUG_SCPI
				ESP_LOGI(TAG, "%ld!%ld, ", array[i].row, array[i].col);
			#endif
        }
        #ifdef DEBUG_SCPI
			ESP_LOGI(TAG, "\r\n");
		#endif
    }
    return SCPI_RES_OK;
}



/**
 * Reimplement IEEE488.2 *TST?
 *
 * Result should be 0 if everything is ok
 * Result should be 1 if something goes wrong
 *
 * Return SCPI_RES_OK
 */
 
static scpi_result_t My_CoreTstQ(scpi_t * context) {
    SCPI_ResultInt32(context, 0);
    return SCPI_RES_OK;
}

/**
 * STATus:QUEStionable:INSTrument[:EVENt]?
 * @param context
 * @return
 */
scpi_result_t SCPI_StatusQuestionableInstrumetEventQ(scpi_t * context) {
 
    /* return value */
    SCPI_ResultInt32(context, SCPI_RegGet(context, SCPI_REG_QUES_INST));

    /* clear register */
    SCPI_RegSet(context, SCPI_REG_QUES_INST, 0);

    return SCPI_RES_OK;
}

/**
 * STATus:QUEStionable:INSTrument:ENABle
 * @param context
 * @return
 */
scpi_result_t SCPI_StatusQuestionableInstrumetEnable(scpi_t * context) {
    int32_t new_INSTE;
	
    if (SCPI_ParamInt32(context, &new_INSTE, TRUE)) {
        SCPI_RegSet(context, SCPI_REG_QUES_INSTE, (scpi_reg_val_t) new_INSTE);
    }
    return SCPI_RES_OK;
}

/**
 * STATus:QUEStionable:INSTrument:ENABle?
 * @param context
 * @return
 */
scpi_result_t SCPI_StatusQuestionableInstrumetEnableQ(scpi_t * context) {
	
	/* return value */
    SCPI_ResultInt32(context, SCPI_RegGet(context, SCPI_REG_QUES_INSTE));

    return SCPI_RES_OK;
}

/**
 * STATus:QUEStionable:INSTrument:ISUMmary#:ENABle
 * @param context
 * @return
 */
scpi_result_t SCPI_StatusQuestionableInstrumetIsummaryEnable(scpi_t * context) {
    int32_t new_INSTISUME;
    int32_t numbers[1];
    int reg;

    SCPI_CommandNumbers(context, numbers, 1, 0);
    
    if ((numbers[0] <1) | (numbers[0]>8))
    {
		SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
	    return SCPI_RES_ERR;
	}
	
	reg = SCPI_REG_QUES_INST_ISUM1E + (numbers[0]-1)*3;
	
    if (SCPI_ParamInt32(context, &new_INSTISUME, TRUE)) {
        SCPI_RegSet(context, reg, (scpi_reg_val_t) new_INSTISUME);
    }
    return SCPI_RES_OK;
}

/**
 * STATus:QUEStionable:INSTrument:ISUMmary#:ENABle?
 * @param context
 * @return
 */
scpi_result_t SCPI_StatusQuestionableInstrumetIsummaryEnableQ(scpi_t * context) {
    int32_t numbers[1];
    int reg;

    SCPI_CommandNumbers(context, numbers, 1, 0);
    
    if ((numbers[0] <1) | (numbers[0]>8))
    {
		SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
	    return SCPI_RES_ERR;
	}
	
	reg = SCPI_REG_QUES_INST_ISUM1E + (numbers[0]-1)*3;
	
	/* return value */
    SCPI_ResultInt32(context, SCPI_RegGet(context, reg));

    return SCPI_RES_OK;
}


/**
 * STATus:QUEStionable:INSTrument:ISUMmary#[:EVENt]?
 * @param context
 * @return
 */
scpi_result_t SCPI_StatusQuestionableInstrumetIsummaryEventQ(scpi_t * context) {
     int32_t numbers[1];
     int reg;

    SCPI_CommandNumbers(context, numbers, 1, 0);
    
    if ((numbers[0] <1) | (numbers[0]>8))
    {
		SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
	    return SCPI_RES_ERR;
	}
	
	 //Trigger PMBus READALL by clearing READALL_COMPLETE
	xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE);
    //Wait for result
    xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE, pdFALSE, pdTRUE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));

	reg = SCPI_REG_QUES_INST_ISUM1 + (numbers[0]-1)*3;
    /* return value */
    SCPI_ResultInt32(context, SCPI_RegGet(context, reg));

    /* clear register */
    SCPI_RegSet(context, reg, 0);

    return SCPI_RES_OK;
}

/**
 * STATus:QUEStionable:INSTrument:ISUMmary#:CONDition?
 * @param context
 * @return
 */
scpi_result_t SCPI_StatusQuestionableInstrumetIsummaryConditionQ(scpi_t * context) {
     int32_t numbers[1];
     int reg;

    SCPI_CommandNumbers(context, numbers, 1, 0);
    
    if ((numbers[0] <1) | (numbers[0]>8))
    {
		SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
	    return SCPI_RES_ERR;
	}
	
	 //Trigger PMBus READALL by clearing READALL_COMPLETE
	xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE);
    //Wait for result
    xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE, pdFALSE, pdTRUE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));

	reg = SCPI_REG_QUES_INST_ISUM1C + (numbers[0]-1)*3;
    /* return value */
    SCPI_ResultInt32(context, SCPI_RegGet(context, reg));

    return SCPI_RES_OK;
}



const scpi_command_t scpi_commands[] = {
    /* IEEE Mandated Commands (SCPI std V1999.0 4.1.1) */
    { .pattern = "*CLS", .callback = SCPI_CoreCls,},
    { .pattern = "*ESE", .callback = SCPI_CoreEse,},
    { .pattern = "*ESE?", .callback = SCPI_CoreEseQ,},
    { .pattern = "*ESR?", .callback = SCPI_CoreEsrQ,},
    { .pattern = "*IDN?", .callback = SCPI_CoreIdnQ,},
    { .pattern = "*OPC", .callback = SCPI_CoreOpc,},
    { .pattern = "*OPC?", .callback = SCPI_CoreOpcQ,},
    { .pattern = "*RST", .callback = SCPI_CoreRst,},
    { .pattern = "*SRE", .callback = SCPI_CoreSre,},
    { .pattern = "*SRE?", .callback = SCPI_CoreSreQ,},
    { .pattern = "*STB?", .callback = SCPI_CoreStbQ,},
    { .pattern = "*TST?", .callback = My_CoreTstQ,},
    { .pattern = "*WAI", .callback = SCPI_CoreWai,},

    /* Required SCPI commands (SCPI std V1999.0 4.2.1) */
    {.pattern = "SYSTem:ERRor[:NEXT]?", .callback = SCPI_SystemErrorNextQ,},
    {.pattern = "SYSTem:ERRor:COUNt?", .callback = SCPI_SystemErrorCountQ,},
    {.pattern = "SYSTem:VERSion?", .callback = SCPI_SystemVersionQ,},
    {.pattern = "STATus:OPERation[:EVENt]?", .callback = SCPI_StatusOperationEventQ,},
    {.pattern = "STATus:OPERation:CONDition?", .callback = SCPI_StatusOperationConditionQ,},
    {.pattern = "STATus:OPERation:ENABle", .callback = SCPI_StatusOperationEnable,},
    {.pattern = "STATus:OPERation:ENABle?", .callback = SCPI_StatusOperationEnableQ,},
    {.pattern = "STATus:QUEStionable[:EVENt]?", .callback = SCPI_StatusQuestionableEventQ,},
    {.pattern = "STATus:QUEStionable:CONDition?", .callback = SCPI_StatusQuestionableConditionQ,},
    {.pattern = "STATus:QUEStionable:ENABle", .callback = SCPI_StatusQuestionableEnable,},
    {.pattern = "STATus:QUEStionable:ENABle?", .callback = SCPI_StatusQuestionableEnableQ,},
    {.pattern = "STATus:PRESet", .callback = SCPI_StatusPreset,},
    
    /* Instrument status */
    {.pattern = "STATus:QUEStionable:INSTrument[:EVENt]?", .callback = SCPI_StatusQuestionableInstrumetEventQ,},
	{.pattern = "STATus:QUEStionable:INSTrument:ENABle", .callback = SCPI_StatusQuestionableInstrumetEnable,},
	{.pattern = "STATus:QUEStionable:INSTrument:ENABle?", .callback = SCPI_StatusQuestionableInstrumetEnableQ,},
	
	/* Instrument summary status */
    {.pattern = "STATus:QUEStionable:INSTrument:ISUMmary#[:EVENt]?", .callback = SCPI_StatusQuestionableInstrumetIsummaryEventQ,},
	{.pattern = "STATus:QUEStionable:INSTrument:ISUMmary#:CONDition?", .callback = SCPI_StatusQuestionableInstrumetIsummaryConditionQ,},
	{.pattern = "STATus:QUEStionable:INSTrument:ISUMmary#:ENABle", .callback = SCPI_StatusQuestionableInstrumetIsummaryEnable,},
	{.pattern = "STATus:QUEStionable:INSTrument:ISUMmary#:ENABle?", .callback = SCPI_StatusQuestionableInstrumetIsummaryEnableQ,},

    /* Measurements */
    {.pattern = "MEASure:VOLTage?", .callback = MeasureVoltageQ,},
    {.pattern = "MEASure:TEMPerature?", .callback = MeasureTempQ,},
    {.pattern = "MEASure:POWer?", .callback = MeasurePowerQ,},
    {.pattern = "MEASure:CURRent?", .callback = MeasureCurrentQ,},
    
    /* Settings */
    {.pattern = "VOLTage", .callback = SetVoltage,},
    {.pattern = "VOLTage?", .callback = SetVoltageQ,},
    {.pattern = "VOLTage:DROop", .callback = SetVdroop,},
    {.pattern = "VOLTage:DROop?", .callback = SetVdroopQ,},
    {.pattern = "CURRent[:PROTection]", .callback = SetCurrent,},
    {.pattern = "CURRent[:PROTection]?", .callback = SetCurrentQ,},
    {.pattern = "OUTPut:STATe", .callback = SetState,},
    {.pattern = "OUTPut:STATe?", .callback = SetStateQ,},
    {.pattern = "OUTPut:STATe:TONMax", .callback = SetTon,},
    {.pattern = "OUTPut:STATe:TONMax?", .callback = SetTonQ,},
    {.pattern = "VOLTage:LIMit:LOW", .callback = SetUV,},
    {.pattern = "VOLTage:LIMit:LOW?", .callback = SetUVQ,},
    {.pattern = "VOLTage:LIMit:HIGH", .callback = SetOV,},
    {.pattern = "VOLTage:LIMit:HIGH?", .callback = SetOVQ,},
    {.pattern = "VOLTage:SENSe[:SOURce]", .callback = SnsSelect,},
    {.pattern = "VOLTage:SENSe[:SOURce]?", .callback = SnsSelectQ,},
    {.pattern = "INSTrument:SELect", .callback = CHselect,},
    {.pattern = "INSTrument:SELect?", .callback = CHselectQ,},
    {.pattern = "INSTrument:COUPle", .callback = CHcouple,},
    {.pattern = "INSTrument:COUPle?", .callback = CHcoupleQ,},
    
    /* Advanced */
    {.pattern = "PMBUS", .callback = PMBus_CommandHandler,},
    {.pattern = "PMBUS?", .callback = PMBusQ,},
    {.pattern = "GPIO", .callback = GPIO_CommandHandler,},
    {.pattern = "GPIO?", .callback = GPIO_CommandHandlerQ,},

	/* Test */
    /*{.pattern = "TEST:BOOL", .callback = TEST_Bool,},*/
    /*{.pattern = "TEST:CHOice?", .callback = TEST_ChoiceQ,},*/
    /*{.pattern = "TEST#:NUMbers#", .callback = TEST_Numbers,},*/
    /*{.pattern = "TEST:TEXT", .callback = TEST_Text,},*/
    /*{.pattern = "TEST:ARBitrary?", .callback = TEST_ArbQ,},*/
    /*{.pattern = "TEST:CHANnellist", .callback = TEST_Chanlst,},*/

    SCPI_CMD_LIST_END
};

scpi_interface_t scpi_http_interface = {
    .error = SCPI_http_Error,
    .write = SCPI_http_Write,
    .control = SCPI_http_Control,
    .flush = SCPI_http_Flush,
    .reset = SCPI_http_Reset,
};

scpi_interface_t scpi_socket_interface = {
    .error = SCPI_socket_Error,
    .write = SCPI_socket_Write,
    .control = SCPI_socket_Control,
    .flush = SCPI_socket_Flush,
    .reset = SCPI_socket_Reset,
};

scpi_interface_t scpi_usbtmc_interface = {
    .error = SCPI_USBTMC_Error,
    .write = SCPI_USBTMC_Write,
    .control = SCPI_USBTMC_Control,
    .flush = SCPI_USBTMC_Flush,
    .reset = SCPI_USBTMC_Reset,
};

char scpi_http_input_buffer[SCPI_INPUT_BUFFER_LENGTH];
char scpi_tcp_input_buffer[SCPI_INPUT_BUFFER_LENGTH];
char scpi_usbtmc_input_buffer[SCPI_INPUT_BUFFER_LENGTH];
scpi_error_t scpi_error_queue_data[SCPI_ERROR_QUEUE_SIZE];

scpi_t scpi_TCP_context, scpi_usbtmc_context, scpi_http_context;
