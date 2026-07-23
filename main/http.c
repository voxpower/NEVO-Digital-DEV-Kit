
#include "http.h"
#include "esp_log.h"
#include "cJSON.h"
#include "pmbus.h"
#include "main.h"
#include "scpi-def.h"
#include "scpi/parser.h"
#include "tasks_notifications.h"
#include "gpio.h"
#include "soc/gpio_reg.h"
#include <stdint.h>
#include "debug.h"

static const char *TAG = "HTTP SERVER";

bool ws_settings_flag = false;
bool ws_send_readings = false;

static httpd_handle_t http_server = NULL;

struct async_resp_arg
{
	httpd_handle_t hd;
	int fd;
};
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t chart_min_js_start[] asm("_binary_chart_min_js_start");
extern const uint8_t chart_min_js_end[]   asm("_binary_chart_min_js_end");
extern const uint8_t script_js_start[] asm("_binary_script_js_start");
extern const uint8_t script_js_end[]   asm("_binary_script_js_end");
extern const uint8_t logo_png_start[] asm("_binary_logo_png_start");
extern const uint8_t logo_png_end[]   asm("_binary_logo_png_end"); 
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[]   asm("_binary_style_css_end"); 

static esp_err_t handle_ws_req(httpd_req_t *req);
static esp_err_t trigger_async_send(httpd_handle_t handle);
static void ws_async_send(void *arg);

extern pmbus_data_t pmbus_channels[];
extern pmbus_device_t pmbus_device[];
extern TaskHandle_t tasksHandlers[];
extern EventGroupHandle_t SMBUS_events, smbus_init_events;
extern pmbus_cmd_def_t pmbus_cmd_def[];
extern scpi_choice_def_t GPIO_options[];
extern uint8_t SCPI_channel;




// *****************************************************************************
// @brief Callback POST "/SCPI"
//        Receive JSON 
// @param httpd_req_t *req
// @return esp_err_t
// *****************************************************************************
esp_err_t scpi_post_handler(httpd_req_t *req)
{
	#ifdef DEBUG_HTTP
		ESP_LOGI(TAG, "Request: POST /SCPI");
	#endif
	
	char content[HTTP_MAX_CONTENT_SIZE];
	const char * cmd;
	char cmd_buffer[WS_BUFFER_SIZE];
	int len;
	
	int ret, remaining = req->content_len;

	if (remaining > sizeof(content))
	{
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too long");
		return ESP_FAIL;
	}

	ret = httpd_req_recv(req, content, remaining);
	if (ret <= 0)
	{
		httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
		return ESP_FAIL;
	}
	content[ret] = '\0';
	
	#ifdef DEBUG_HTTP
		ESP_LOGI(TAG, "http SCPI Received Payload: %s", content);
		ESP_LOGI(TAG, "SCPI cmd %s ,length %d", content,strlen(content));
	#endif
	
	strcpy(cmd_buffer,content);
	len = strlen(cmd_buffer);
	
	#ifdef DEBUG_HTTP
		ESP_LOGI(TAG, "SCPI cmd_buffer %s ,length %d", cmd_buffer,len);
	#endif
	
	cmd_buffer[len] = 10; // NL-terminate
	len++;
	
	#ifdef DEBUG_HTTP
		ESP_LOG_BUFFER_HEXDUMP(TAG, cmd_buffer, len, ESP_LOG_INFO);
	#endif
	
    scpi_http_context.user_context = req;
    
    #ifdef DEBUG_HTTP
    	ESP_LOGI(TAG, "URI %s ", req->uri);
		ESP_LOGI(TAG, "content %s ", content);
	#endif
	
	SCPI_Input(&scpi_http_context, cmd_buffer, len);
		
	#ifdef DEBUG_HTTP
		ESP_LOGI(TAG, "SCPI_post_handler end");
	#endif
	return ESP_OK;
}



// ******************************************
// @brief Callback  GET "/" 
// @param httpd_req_t *req
// @return esp_err_t
// ******************************************
esp_err_t index_get_handler(httpd_req_t *req)
{
	// the response is the web page index.html
	httpd_resp_send(req, (const char *)index_html_start, index_html_end - index_html_start);
	return ESP_OK;
}

// ******************************************
// @brief Callback  GET "/logo.png"
// @param httpd_req_t *req
// @return esp_err_t
// ******************************************
esp_err_t logo_png_get_handler(httpd_req_t *req)
{
	// Set the correct content type for PNG
	httpd_resp_set_type(req, "image/png");
	
	// Send the response with the embedded PNG file content
	httpd_resp_send(req, (const char *)logo_png_start, logo_png_end - logo_png_start);
	
	return ESP_OK;
}

// ******************************************
// @brief Callback  GET "/style.css"
// @param httpd_req_t *req
// @return esp_err_t
// ******************************************
esp_err_t style_css_get_handler(httpd_req_t *req)
{
	// Set the correct content type for PNG
	httpd_resp_set_type(req, "text/css");
	
	// Send the response with the embedded PNG file content
	httpd_resp_send(req, (const char *)style_css_start, style_css_end - style_css_start);
	
	return ESP_OK;
}


// ******************************************
// @brief Callback  GET "/chart.min.js" 
// @param httpd_req_t *req
// @return esp_err_t
// ******************************************
esp_err_t chart_js_get_handler(httpd_req_t *req)
{
	// Set the correct content type for JavaScript
    httpd_resp_set_type(req, "application/javascript");

    // Send the response with the embedded JS file content
    httpd_resp_send(req, (const char *)chart_min_js_start, chart_min_js_end - chart_min_js_start);
    	
	return ESP_OK;
}

// ******************************************
// @brief Callback  GET "/script.js" 
// @param httpd_req_t *req
// @return esp_err_t
// ******************************************
esp_err_t script_js_get_handler(httpd_req_t *req)
{
	// Set the correct content type for JavaScript
    httpd_resp_set_type(req, "application/javascript");

    // Send the response with the embedded JS file content
    httpd_resp_send(req, (const char *)script_js_start, script_js_end - script_js_start);
    	
	return ESP_OK;
}

//****************************************************
// @brief Acces Paths to WebServer
//*****************************************************
// URI handler for index.html
httpd_uri_t index_get = {
	.uri = "/",
	.method = HTTP_GET,
	.handler = index_get_handler,
	.user_ctx = NULL};

// URI handler for style.css
httpd_uri_t style_css_uri = {
	.uri = "/style.css",
	.method = HTTP_GET,
	.handler = style_css_get_handler,
	.user_ctx = NULL};
	
// URI handler for logo.png
httpd_uri_t logo_png_uri = {
	.uri = "/logo.png",
	.method = HTTP_GET,
	.handler = logo_png_get_handler,
	.user_ctx = NULL};
	
// URI handler for chart.min.js
httpd_uri_t chart_js_uri = {
    .uri       = "/chart.min.js",   // The URL to access the file
    .method    = HTTP_GET,
    .handler   = chart_js_get_handler, // The new handler function
    .user_ctx  = NULL};

// URI handler for script.js
httpd_uri_t script_js_uri = {
    .uri       = "/script.js",   // The URL to access the file
    .method    = HTTP_GET,
    .handler   = script_js_get_handler, // The new handler function
    .user_ctx  = NULL};
        
// URI handler for websocket
httpd_uri_t ws = {
	.uri = "/ws",
	.method = HTTP_GET,
	.handler = handle_ws_req,
	.user_ctx = NULL,
	.is_websocket = true};

// URI handler for /SCPI
httpd_uri_t scpi_post = {
	.uri = "/SCPI",
	.method = HTTP_POST,
	.handler = scpi_post_handler,
	.user_ctx = NULL};
	
	
//******************************************
// @brief
// @param
// @return esp_err_t
//******************************************
esp_err_t http_server_init(void)
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	config.stack_size = HTTP_STACK_SIZE;

	if (httpd_start(&http_server, &config) == ESP_OK)
	{
		httpd_register_uri_handler(http_server, &index_get);
		httpd_register_uri_handler(http_server, &chart_js_uri);
		httpd_register_uri_handler(http_server, &script_js_uri);
		httpd_register_uri_handler(http_server, &ws);
		httpd_register_uri_handler(http_server, &scpi_post);
		httpd_register_uri_handler(http_server, &logo_png_uri);
		httpd_register_uri_handler(http_server, &style_css_uri); 
	}

	return http_server == NULL ? ESP_FAIL : ESP_OK;
}

// *************************************************
// @brief Callback WebSocket Request
// @param httpd_req_t *req
// @return esp_err_t
// *************************************************
static esp_err_t handle_ws_req(httpd_req_t *req)
{
	if (req->method == HTTP_GET)
	{
		#ifdef DEBUG_HTTP
			ESP_LOGI(TAG, "Handshake done, new connection");
		#endif
		
		ws_send_readings = true;
		return ESP_OK;
	}

	//
	httpd_ws_frame_t ws_pkt;
	uint8_t *buf = NULL;
	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
	ws_pkt.type = HTTPD_WS_TYPE_TEXT;
	uint8_t cmd_index,channel_index;
	bool cmd_flag;
	int32_t gpio_status;
	pmbus_cmd_t pmbus_cmd;
	char cmd_buffer[WS_BUFFER_SIZE];

	esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
	if (ret != ESP_OK)
	{
		#ifdef DEBUG_HTTP
			ESP_LOGE(TAG, "httpd_ws_recv_frame error when obtaining the 'frame len' with %d", ret);
		#endif
		
		return ret;
	}

	if (ws_pkt.len)
	{
		buf = calloc(1, ws_pkt.len + 1);
		if (buf == NULL)
		{
			#ifdef DEBUG_HTTP
				ESP_LOGE(TAG, "Error reserving memory for the buffer");
			#endif
			
			return ESP_ERR_NO_MEM;
		}

		ws_pkt.payload = buf;
		ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
		if (ret != ESP_OK)
		{
			#ifdef DEBUG_HTTP
				ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
			#endif
			
			free(buf);
			return ret;
		}

		#ifdef DEBUG_HTTP
			ESP_LOGI(TAG, "Received message: %s", ws_pkt.payload);
		#endif

		cJSON *root = cJSON_Parse(ws_pkt.payload);
		if (!root || !cJSON_IsObject(root))
		{
			#ifdef DEBUG_HTTP
				ESP_LOGI(TAG, "Invalid JSON");
			#endif
			return ESP_FAIL;
		}
		
		cJSON *message_obj = cJSON_GetObjectItem(root, "message");
		if (!message_obj || !cJSON_IsString(message_obj)) {
		    #ifdef DEBUG_HTTP
		    	ESP_LOGE(TAG, "Invalid message format");
		    #endif
		    cJSON_Delete(root);
		    free(buf);
		    return ESP_FAIL;
		}
		
		char *message = message_obj->valuestring;
	
		if (!strcmp(message,"Send settings")) 
		{
			ws_send_readings = true;	//Triggers the send
			//Trigger PMBus READDEVICES by clearing READDEVICES_COMPLETE
			xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READDEVICES_COMPLETE);
		}
		
		if (!strcmp(message,"Send readings")) 
		{
			ws_send_readings = true;
			//Trigger PMBus READALL by clearing READALL_COMPLETE
			xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE);
		}
		
		
		if (!strcmp(message,"Import settings")) 
		{	
			//ws_send_readings = false;
			
			cJSON *settings_array = cJSON_GetObjectItem(root, "settings");
			
			int array_size = cJSON_GetArraySize(settings_array);
			#ifdef DEBUG_HTTP
				ESP_LOGI(TAG,"\nChannel Settings:\n");
			#endif
	
	   		for (int i = 0; i < array_size; i++) 
	   		{
		        cJSON *channel_settings_obj = cJSON_GetArrayItem(settings_array, i);
		
		        if (!cJSON_IsObject(channel_settings_obj)) 
		        {
		            #ifdef DEBUG_HTTP
		            	ESP_LOGE(TAG, "Error: Array element at index %d is not an object.\n", i);
		            #endif
		            continue; // Skip to the next element
		        }
			    
		        //Get channel
		        cJSON *channel_val = cJSON_GetObjectItemCaseSensitive(channel_settings_obj, "channel");
		        #ifdef DEBUG_HTTP
		        	ESP_LOGI(TAG,"Channel %d:\n", cJSON_IsNumber(channel_val) ? channel_val->valueint : -1);
		        #endif
		        channel_index = channel_val->valueint - 1;
		        
		        //Check if channel is present
		        if (pmbus_device[channel_index].initialized)
		        {
					// Iterate through all key-value pairs within the current channel's settings object
			    	cJSON *subitem = channel_settings_obj->child;
			    	while (subitem != NULL) 
			    	{
				        #ifdef DEBUG_HTTP
				        	ESP_LOGI(TAG,"Key: %s, Value: %d\n", subitem->string,subitem->valueint);
				        #endif
				
						if (strcmp(subitem->string,"channel")) 
						{
							//Find command
							cmd_flag = 0;
							for (int j = 0; j < SMB_CMD_N; j++)
							{
								if (!strcmp(pmbus_cmd_def[j].command_str, subitem->string))
								{
									#ifdef DEBUG_HTTP
										ESP_LOGI(TAG, "CMD: %d", pmbus_cmd_def[j].command);
									#endif
									cmd_index = j;
									cmd_flag = 1;
									break;
								}
							}
							
							ESP_LOGI(TAG, "cmdIndex: %d, cmdFlag: %d", cmd_index, cmd_flag);
							if (cmd_flag)
							{
								xSemaphoreTake(scpi_channel_mutex, portMAX_DELAY);
								SCPI_channel = channel_index;
								xSemaphoreGive(scpi_channel_mutex);
								strncpy(cmd_buffer,pmbus_cmd_def[cmd_index].scpi_str,20);
								uint16_t len = strlen(cmd_buffer);
								char value_str[8];
								size_t valuestr_len;
								scpi_number_t value;
								value.base = 10;
								value.special = 0;
								value.content.value = subitem->valuedouble;
								ESP_LOGI(TAG, "Value: %f", value.content.value);
								valuestr_len = SCPI_NumberToStr(&scpi_http_context, NULL, &value, value_str, 8);
								ESP_LOGI(TAG, "Valuestr_len: %d", valuestr_len);
								strncat(cmd_buffer,value_str,valuestr_len);
								len += valuestr_len;
								
								#ifdef DEBUG_HTTP
									ESP_LOGI(TAG, "SCPI cmd_buffer %s ,length %d", cmd_buffer,len);
								#endif
								
								cmd_buffer[len] = 10; // NL-terminate
								len++;
								SCPI_Input(&scpi_http_context, cmd_buffer, len);
							}
						}
						subitem = subitem->next;
					}	
				} else 
				{
					#ifdef DEBUG_HTTP
						ESP_LOGI(TAG, "Channel: %d not present", (channel_val->valueint));
					#endif
					SCPI_ErrorPush(&scpi_http_context, SCPI_ERROR_HARDWARE_MISSING);
				}
			}		        
		}
		
		if (!strcmp(message,"Store NVM")) 				//Store NVM all modules
		{
			//Turn OFF all devices.
			gpio_status = REG_READ(GPIO_OUT_REG) & GPIO_OUTPUT_PIN_SEL;		//Save GPIO status.
			REG_WRITE(GPIO_OUT_W1TS_REG, GPIO_options[8].tag);				//All units OFF

			//Write STORE_DEFAULT_ALL CMD to PMBUS
			cmd_index = 5;	//STORE_DEFAULT_ALL
			pmbus_cmd.command = pmbus_cmd_def[cmd_index].command;
			pmbus_cmd.address = 0; 	//General Call address
			pmbus_cmd.r_w = SMB_MSG_TYPE_WRITE;
			pmbus_cmd.format = pmbus_cmd_def[cmd_index].format;
			pmbus_cmd.len = pmbus_cmd_def[cmd_index].bytes;
			pmbus_cmd.update.update = false;
						
			xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR);

			pmbus_send_cmd(&pmbus_cmd);

			EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));
			//Wait
			if (bits & SMBUS_PROC_NOTIF_WRITE_ERROR) 
			{
				#ifdef DEBUG_HTTP
					ESP_LOGE(TAG, "PMBus timeout");
				#endif
			    httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "PMBus timeout");
			    return ESP_FAIL;
			}
			
			//Write RESTORE_DEFAULT_ALL CMD to PMBUS
			cmd_index = 6;	//RESTORE_DEFAULT_ALL
			pmbus_cmd.command = pmbus_cmd_def[cmd_index].command;
			pmbus_cmd.address = 0; 	//General Call address
			pmbus_cmd.r_w = SMB_MSG_TYPE_WRITE;
			pmbus_cmd.format = pmbus_cmd_def[cmd_index].format;
			pmbus_cmd.len = pmbus_cmd_def[cmd_index].bytes;
			pmbus_cmd.update.update = false;
						
			xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR);

			pmbus_send_cmd(&pmbus_cmd);

			bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));
			//Wait
			if (bits & SMBUS_PROC_NOTIF_WRITE_ERROR) 
			{
				#ifdef DEBUG_HTTP
					ESP_LOGE(TAG, "PMBus timeout");
				#endif
			    httpd_resp_send_err(req, HTTPD_408_REQ_TIMEOUT, "PMBus timeout");
			    return ESP_FAIL;
			}
 
			//Set OPERATION to ON	
			REG_WRITE(GPIO_OUT_W1TC_REG, GPIO_ALL);		//All outputs ON
			REG_WRITE(GPIO_OUT_W1TS_REG, gpio_status);	//Recall GPIO status
			
			//Read back parameters to verify.
			ws_send_readings = true;	//Triggers the send
			//Trigger PMBus READDEVICES by clearing READDEVICES_COMPLETE
			xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READDEVICES_COMPLETE);	
		}
		
	// Clean up: Delete the cJSON object and all its children
	cJSON_Delete(root);
	}

	#ifdef DEBUG_HTTP
		ESP_LOGI(TAG, "frame len --> %d", ws_pkt.len);
	#endif

	//Release memory
	free(buf);

	return ESP_OK;
}

//**************************************************************************
// @brief Messages to Send by WebSocket
// @param httpd_handle_t handle
// @return esp_err_t
//***************************************************************************
static esp_err_t trigger_async_send(httpd_handle_t handle)
{
	struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
	resp_arg->hd = handle;
	resp_arg->fd = -1; 
	int ret = httpd_queue_work(handle, ws_async_send, resp_arg);
	#ifdef DEBUG_HTTP
		ESP_LOGI(TAG, "httpd_queue_work %d",ret);
	#endif
	return ret;
}

//***********************************************************************
// @brief 
// @param void
// @return esp_err_t
//***********************************************************************
static void ws_async_send(void *arg)
{
	httpd_ws_frame_t ws_pkt;
	struct async_resp_arg *resp_arg = arg;
	httpd_handle_t hd = resp_arg->hd;
	int fd = resp_arg->fd;

	char buff[1024];
	
	for (int j = 0; j < MAX_N_DEVICES; j++)
	{
		
		if (ws_settings_flag && ws_send_readings)	//send settings while already sending readings?
		{
			j = 0;	//Start again
			ws_send_readings = false;	//Only settings
		}
		
		if (!pmbus_device[j].initialized)
			continue;
					
		memset(buff, 0, sizeof(buff));
		
		if (ws_settings_flag) 
		{
			snprintf(buff, sizeof(buff), "{\"type\":\"settings\",\"ch\":%d, \"model%d\":\"%s\", \
			\"serial%d\":\"%s\", \"hw%d\":\"%s\", \"vset%d\":%d, \"drp%d\":%d, \"ov%d\":%d,	\"uv%d\":%d, \
			\"ton%d\":%d, \"ocp%d\":%d, \"zone%d\":%d, \"addr%d\":%d, \"snsterm%d\":%d,	\"onoff%d\":%d }",
				j+1, j+1,pmbus_channels[j].model, j+1 ,pmbus_channels[j].serial, j+1 ,pmbus_channels[j].hw_version,
				j+1,pmbus_channels[j].vout, j+1, pmbus_channels[j].droop, j+1 ,pmbus_channels[j].overvoltage,
				j+1,pmbus_channels[j].undervoltage, j+1, pmbus_channels[j].ton, j+1, pmbus_channels[j].ilimit, 
				j+1, pmbus_channels[j].zone, j+1, pmbus_channels[j].smbus_address, j+1, pmbus_channels[j].settings,
				j+1, pmbus_channels[j].operation);			
		} else {
				
			snprintf(buff, sizeof(buff), "{\"type\":\"readings\", \"ch\":%d, \"v%d\":%d, \"i%d\":%d, \"p%d\":%d, \"t%d\":%d, \"StatByte%d\":%d, \"StatVout%d\":%d }",
				j+1, j+1, pmbus_device[j].voltage, j+1, pmbus_device[j].current, j+1, pmbus_device[j].power, j+1, pmbus_device[j].temp, 
				j+1, pmbus_device[j].StatByte, j+1, pmbus_device[j].StatVout);
		}
		
		#ifdef DEBUG_HTTP
			ESP_LOGI(TAG, "Contents %s",buff);
		#endif
		memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
		ws_pkt.payload = (uint8_t *)buff;
		ws_pkt.len = strlen(buff);
		ws_pkt.type = HTTPD_WS_TYPE_TEXT;
	
		static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
		size_t fds = max_clients;
		int client_fds[max_clients];

		esp_err_t ret = httpd_get_client_list(http_server, &fds, client_fds);
	
		if (ret != ESP_OK)
		{
			return;
		}
	
		for (int i = 0; i < fds; i++)
		{
			int client_info = httpd_ws_get_fd_info(http_server, client_fds[i]);
			if (client_info == HTTPD_WS_CLIENT_WEBSOCKET)
			{
				httpd_ws_send_frame_async(hd, client_fds[i], &ws_pkt);
			}
		}
		
	}
	
	memset(buff, 0, sizeof(buff));
	snprintf(buff, sizeof(buff), "{\"type\":\"end\"}");
	#ifdef DEBUG_HTTP
		ESP_LOGI(TAG, "Contents %s",buff);
	#endif
		
	memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
	ws_pkt.payload = (uint8_t *)buff;
	ws_pkt.len = strlen(buff);
	ws_pkt.type = HTTPD_WS_TYPE_TEXT;
		
	static size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
	size_t fds = max_clients;
	int client_fds[max_clients];
	
	esp_err_t ret = httpd_get_client_list(http_server, &fds, client_fds);

	if (ret != ESP_OK)
	{
		return;
	}
		
	for (int i = 0; i < fds; i++)
	{
		int client_info = httpd_ws_get_fd_info(http_server, client_fds[i]);
		if (client_info == HTTPD_WS_CLIENT_WEBSOCKET)
		{
			httpd_ws_send_frame_async(hd, client_fds[i], &ws_pkt);
		}
	}

	ws_settings_flag = false;
	ws_send_readings = false;
	
	#ifdef DEBUG_HTTP
		ESP_LOGI(TAG, "WS sent");
	#endif
	free(resp_arg);
}

//**********************************************************
// @brief Task in charge of managing web services
// @param 
// @return 
//***********************************************************
void websocket_task(void *pvParameters)
{
	xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_INIT_DONE, pdFALSE, pdTRUE, pdMS_TO_TICKS(5000)); 

	while (1)
	{
		if (http_server != NULL && ws_send_readings)
		{
			xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READDEVICES_COMPLETE | SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE , pdFALSE, pdTRUE, pdMS_TO_TICKS(5000)); 
			trigger_async_send(http_server);
		}
		ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(HTTP_WEB_SOCKETS_THREAD_TIME));
	}
}