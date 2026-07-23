/* Include guard ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#ifndef scpi_http_h
#define scpi_http_h

/* Includes ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

//#include "esp_system.h"
//#include "esp_http_server.h"
//#include "../common/scpi-def.h"

//#define HTTP_ON_STR				"ON"
//#define HTTP_OFF_STR			"OFF"
//
//
//#define HTTP_WEB_SOCKETS_THREAD_TIME			1000


//typedef union
//{
//  struct
//  {
//	  uint8_t nchannel : 2;   //channel 1-4
//	  uint8_t param : 1;       //it is in PMB_WEB_CMD_STR[SMB_WEB_CMD_N]  
//	  uint8_t value : 1;        //!=NULL
//    uint8_t cmd : 8;
//    uint8_t index : 3;
//  }bit;
//  uint32_t word;
//}http_answ_flags_t;


/* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Includes */
//esp_err_t http_server_init(void);
//esp_err_t index_get_handler(httpd_req_t *req);
//esp_err_t pmbus_get_handler(httpd_req_t *req);
//esp_err_t save_settings_handler(httpd_req_t *req); // Post para Guardar los Settings
//esp_err_t save_param_handler(httpd_req_t *req);	   // Post para Guardar los Settings
//void websocket_task(void *pvParameters);
//void http_set_error_msg(char *msg);

//esp_err_t pmbus_post_handler(httpd_req_t *req);
//esp_err_t scpi_post_handler(httpd_req_t *req);
		   
//void http_set_pmbus_post_return_unsig(uint16_t value);
//void http_set_pmbus_post_return_sig(int16_t value);
//void http_set_pmbus_post_return_dc(uint8_t *cad, uint32_t len);

//size_t SCPI_http_Write(scpi_t * context, const char * data, size_t len);
//scpi_result_t SCPI_http_Flush(scpi_t * context);
//int SCPI_http_Error(scpi_t * context, int_fast16_t err);
//scpi_result_t SCPI_http_Control(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val);
//scpi_result_t SCPI_http_Reset(scpi_t * context);

#endif 

