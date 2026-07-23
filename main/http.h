/* Include guard ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
#ifndef http_h
#define http_h

/* Includes ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#include "esp_http_server.h"

//#define HTTP_ON_STR				"ON"
//#define HTTP_OFF_STR			"OFF"

#define HTTP_WEB_SOCKETS_THREAD_TIME			100
#define HTTP_MAX_CONTENT_SIZE 512
#define WS_BUFFER_SIZE 1024
#define HTTP_STACK_SIZE 8192

esp_err_t http_server_init(void);
esp_err_t index_get_handler(httpd_req_t *req);
esp_err_t chart_js_get_handler(httpd_req_t *req);
esp_err_t scpi_post_handler(httpd_req_t *req);

void websocket_task(void *pvParameters);

#endif 

