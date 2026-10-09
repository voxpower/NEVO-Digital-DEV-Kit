#include "http.h"
#include "esp_log.h"
#include "scpi-def.h"
static const char *TAG = "HTTP SCPI";


char scpi_buffer[128];
uint16_t scpi_buffer_len;


/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
size_t SCPI_http_Write(scpi_t * context, const char * data, size_t len) {
    
    
    ESP_LOGI(TAG, "SCPI_http_Write");
    ESP_LOG_BUFFER_HEXDUMP(TAG, data, len, ESP_LOG_INFO);
    
    if (context->user_context == NULL) {
		ESP_LOGW(TAG, "No context");
        return 0;  // No context, nothing to write
    }
    
    httpd_req_t * req = (httpd_req_t *) (context->user_context);
    
    httpd_resp_set_type(req, "text/plain");
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
	
	if (httpd_resp_send_chunk(req, data, len) != ESP_OK) {
        ESP_LOGE(TAG, "Chunk send failed");
        return 0;
    }
    
    if (len >= 2 && data[len - 2] == '\r' && data[len - 1] == '\n') {
        httpd_resp_send_chunk(req, NULL, 0);
    }
   
    ESP_LOGI(TAG, "SCPI_http_Write complete");
	return len;  
}

/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
scpi_result_t SCPI_http_Flush(scpi_t * context) 
{
    if (context->user_context != NULL) {
        int fd = *(int *) (context->user_context);
	//??
    }
    ESP_LOGI(TAG,"**Flush http buffer\r\n");
    scpi_buffer[0] = 0;
    scpi_buffer_len = 0;
    return SCPI_RES_OK;
}

/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
int SCPI_http_Error(scpi_t * context, int_fast16_t err) {
    (void) context;
    /* BEEP */
    ESP_LOGE(TAG, "**ERROR: %d, \"%s\"\r\n", (int16_t) err, SCPI_ErrorTranslate(err));
    SCPI_ResultMnemonic(context, "ERROR\r\n");
    //SCPI_ResultCharacters(context, "ERROR", 6);
    
    return 0;
}

/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
scpi_result_t SCPI_http_Control(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    (void) context;

    if (SCPI_CTRL_SRQ == ctrl) {
        ESP_LOGI(TAG,"**SRQ: 0x%X (%d)\r\n", val, val);
    } else {
        ESP_LOGI(TAG,"**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
    }
    return SCPI_RES_OK;
}

/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
scpi_result_t SCPI_http_Reset(scpi_t * context) {
    (void) context;

    ESP_LOGI(TAG,"**Reset\r\n");
    return SCPI_RES_OK;
}