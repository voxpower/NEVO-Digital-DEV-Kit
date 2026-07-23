#ifndef SCPI_USBTMC_H
#define SCPI_USBTMC_H
#include "esp_system.h"
#include "scpi/scpi.h"
#include "../common/scpi-def.h"




//size_t SCPI_USBTMC_Write(scpi_t * context, const char * data, size_t len);
//scpi_result_t SCPI_USBTMC_Flush(scpi_t * context);
//int SCPI_USBTMC_Error(scpi_t * context, int_fast16_t err);
//scpi_result_t SCPI_USBTMC_Control(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val);
//scpi_result_t SCPI_USBTMC_Reset(scpi_t * context);

//scpi_result_t SCPI_USBTMC_SystemCommTcpipControlQ(scpi_t * context);
void scpi_usbtmc_send_array (uint8_t *str, uint32_t len);
void scpi_usbtmc_send_word(uint16_t word);
void scpi_usbtmc_send_str(uint8_t *str);
void scpi_usbtmc_send_byte(uint8_t car);

#endif