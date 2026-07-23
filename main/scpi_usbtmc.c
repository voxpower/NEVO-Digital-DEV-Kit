#include "debug.h"
#include "esp_log.h"
#include "scpi_usbtmc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"



static const char *TAG = "SCPI USBTMC";

extern  xQueueHandle usbtmcTxQueue;

/* ********************************** */
/*
 * @brief SCPI answers to  usbtmc
 * @param
 * @retval
 */
void scpi_usbtmc_send_byte(uint8_t car)
{
	xQueueSend (usbtmcTxQueue, &car, 0);
}
/* ********************************** */
/*
 * @brief SCPI answers to  usbtmc
 * @param
 * @retval
 */
void scpi_usbtmc_send_word(uint16_t word) 
{
    uint8_t low = word & 0xFF;
    uint8_t high = (word >> 8) & 0xFF;
    scpi_usbtmc_send_byte(low);
    scpi_usbtmc_send_byte(high);
}
/* ********************************** */
/*
 * @brief  SCPI answers to  usbtmc
 * @param
 * @retval
 */
void scpi_usbtmc_send_array(uint8_t *str, uint32_t len)
{
	while(len--)
	{
		xQueueSend (usbtmcTxQueue, str++, 0);
	}
}
/* ********************************** */
/*
 * @brief  SCPI answers to  usbtmc
 * @param
 * @retval
 */
void scpi_usbtmc_send_str(uint8_t *str)
{
	while (*str != '\0')
	{
		xQueueSend(usbtmcTxQueue, str++, 0);
	}
}


/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
size_t SCPI_USBTMC_Write(scpi_t * context, const char * data, size_t len) {
    if (context->user_context == NULL) {
        return 0;  // No context, nothing to write
    }
    scpi_usbtmc_send_array(data, len);
    size_t total_written = len;
   
    return total_written;  // Return the total number of bytes written
}
/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
scpi_result_t SCPI_USBTMC_Flush(scpi_t * context) 
{
    if (context->user_context != NULL) {
        int fd = *(int *) (context->user_context);

        // Save the original state of TCP_NODELAY
    }
    return SCPI_RES_OK;
}
/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
int SCPI_USBTMC_Error(scpi_t * context, int_fast16_t err) {
    (void) context;
    /* BEEP */
    ESP_LOGE(TAG, "**ERROR: %d, \"%s\"\r\n", (int16_t) err, SCPI_ErrorTranslate(err));
    return 0;
}
/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
scpi_result_t SCPI_USBTMC_Control(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    (void) context;

    if (SCPI_CTRL_SRQ == ctrl) {
        ESP_LOGI(TAG, "**SRQ: 0x%X (%d)\r\n", val, val);
    } else {
        ESP_LOGI(TAG, "**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
    }
    return SCPI_RES_OK;
}

/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
scpi_result_t SCPI_USBTMC_Reset(scpi_t * context) {
    (void) context;
	ESP_LOGI(TAG, "**Reset\r\n");
    return SCPI_RES_OK;
}

