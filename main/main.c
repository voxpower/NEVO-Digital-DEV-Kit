
/**
 * @file main.c
 * @brief Main application file for the PMBUS DevKit.
 *
 * This file contains the main entry point of the application,
 * initialization routines, task creation, and USB TinyMCL callbacks.
 */
#include "main.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <nvs_flash.h>
#include "scpi/ieee488.h"
#include "scpi_socket.h"
#include "soc/gpio_reg.h"
#include "softap.h"
#include "board_api.h"
#include "board_api.h"
#include "tusb.h"
#include "usbtmc_app.h"
#include "tusb_config.h"
#include "esp_log.h"
#include <sys/param.h>
#include <esp_wifi.h>
#include "esp_netif.h"
#include "tasks.h"
#include "pmbus.h"
#include <lwip/netdb.h>
#include "../common/scpi-def.h"
#include "http.h"
#include "tasks_notifications.h"
#include "freertos/event_groups.h"
#include "LED.h"
#include "config.h"


 static const char *TAG = "MAIN";

 TaskHandle_t tasksHandlers[NUM_TASKS];

 xQueueHandle usbtmcTxQueue;

 EventGroupHandle_t SMBUS_events, smbus_init_events;
 
 extern pmbus_device_t pmbus_device[];
 extern pmbus_data_t pmbus_channels[];
 extern led_strip_handle_t led_strip;

 char string[4];
 char IDN_address[40];
 char IDN_serials[100];
 char IDN_models[80];
 char IDN_HWrev[80];
 
SemaphoreHandle_t scpi_channel_mutex;
uint8_t SCPI_channel = 255;	//Default channel is 255

void init_devices()
{
	for (int j = 0; j < MAX_N_DEVICES; j++)
    {
		smbus_init_device(j);
		ESP_LOGI(TAG, "smbus_init_device %d", j);
		
		if (pmbus_device[j].initialized) {
			xSemaphoreTake(scpi_channel_mutex, portMAX_DELAY);
			if (SCPI_channel == 255)	//Set SCPI channel to first initialised channel
				SCPI_channel = j;
			xSemaphoreGive(scpi_channel_mutex);
				
			itoa(pmbus_channels[j].smbus_address,string,10);
			strncat(IDN_address, string,3);
			strncat(IDN_serials, pmbus_channels[j].serial,11);
			strncat(IDN_models, pmbus_channels[j].model,8);
			strncat(IDN_HWrev, pmbus_channels[j].hw_version,8);
		}
		strncat(IDN_address, "|",1);
		strncat(IDN_serials, "|",1);
		strncat(IDN_models, "|",1);
		strncat(IDN_HWrev, "|",1);
		
		ESP_LOGI(TAG, "IDN_address %s", IDN_address);
		ESP_LOGI(TAG, "IDN_serials %s", IDN_serials);
		ESP_LOGI(TAG, "IDN_models %s", IDN_models);
		ESP_LOGI(TAG, "IDN_HWrev %s", IDN_HWrev);		
	}
    
	//remove last "|"
	IDN_address[strlen(IDN_address)-1] = '\0';
	IDN_serials[strlen(IDN_serials)-1] = '\0';	//Checked for 8 module serials. -> OK
	IDN_models[strlen(IDN_models)-1] = '\0';
	IDN_HWrev[strlen(IDN_HWrev)-1] = '\0';
		
    xEventGroupSetBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READDEVICES_COMPLETE);
}


/**
 * @brief Custom debug log function for TinyUSB.
 *
 * This function redirects TinyUSB debug messages to ESP-IDF's logging system.
 *
 * @param format Format string for the log message.
 * @param ... Variable arguments for the format string.
 */
 void tud_debug_log(const char* format, ...) {
   va_list args;
   va_start(args, format);
   esp_log_writev(ESP_LOG_DEBUG, "TinyUSB", format, args);
   va_end(args);
 }
 

/**
 * @brief FreeRTOS task for handling USBTMC operations.
 *
 * This task waits for the SMBus initialization to complete and then
 * continuously processes USBTMC application events.
 *
 * @param pvParameter Pointer to task parameters (not used).
 */
static void usbtmc_task(void *pvParameter) 
{ 
   	xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_INIT_DONE, pdFALSE, pdTRUE, pdMS_TO_TICKS(5000));
   	while (1) 
   	{
    	usbtmc_app_task_iter();
     	vTaskDelay(pdMS_TO_TICKS(50));
   	}
   	return;
 }
 
 
 /*------------- MAIN -------------*/
 //HW will hold device in RESET until NEVO 5Vbias is present.
 //This will ensure correct initialization when USB power is applied.
 
int app_main(void)
{
   board_init();
   scpi_channel_mutex = xSemaphoreCreateMutex();
   esp_err_t ret = nvs_flash_init();

	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	
	smbus_init_events = xEventGroupCreate();
	SMBUS_events = xEventGroupCreate();
	
	ESP_LOGI(TAG, "pmbus_cmd size %d", sizeof(pmbus_cmd_t));
	//LED	
	
    configure_led_strip();			//Initialize the LED strip

    // Set LED color RED
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 100, 0, 0));
    // Flush the buffer to the strip
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
	
	#ifdef EXT_ANTENNA
		REG_WRITE(GPIO_OUT1_W1TS_REG, (1ULL<<(ANTENNA_ENABLE-32)));	//Enable External Antenna
	#else
		REG_WRITE(GPIO_OUT1_W1TC_REG, (1ULL<<(ANTENNA_ENABLE-32)));	//Enable Onboard Antenna
	#endif
	
	pmbus_init_default();
	//Error handling here
	
	init_devices();
	
	ESP_ERROR_CHECK(ret);
	ESP_LOGI(TAG, "SOFTAP");
	ESP_ERROR_CHECK(softap_init());  //Softap initialization
	ESP_LOGI(TAG, "HTTP");
	ESP_ERROR_CHECK(http_server_init());
	ESP_LOGI(TAG, "ESP NETIF");
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_LOGI(TAG, "SCPI");
	scpi_TCP_context.user_context = NULL;

	SCPI_Init(&scpi_http_context,
          scpi_commands,
          &scpi_http_interface,
          scpi_units_def,
         SCPI_IDN1, IDN_models, IDN_serials, IDN_HWrev,
          scpi_http_input_buffer, SCPI_INPUT_BUFFER_LENGTH,
          scpi_error_queue_data, SCPI_ERROR_QUEUE_SIZE);
  	SCPI_RegSetBits(&scpi_http_context,SCPI_REG_ESR,ESR_PON);
  	SCPI_Init(&scpi_TCP_context,
          scpi_commands,
          &scpi_socket_interface,
          scpi_units_def,
         SCPI_IDN1, IDN_models, IDN_serials, IDN_HWrev,
          scpi_tcp_input_buffer, SCPI_INPUT_BUFFER_LENGTH,
          scpi_error_queue_data, SCPI_ERROR_QUEUE_SIZE);
	SCPI_RegSetBits(&scpi_TCP_context,SCPI_REG_ESR,ESR_PON);
  	SCPI_Init(&scpi_usbtmc_context,
          scpi_commands,
          &scpi_usbtmc_interface,
          scpi_units_def,
          SCPI_IDN1, IDN_models, IDN_serials, IDN_HWrev,
          scpi_usbtmc_input_buffer, SCPI_INPUT_BUFFER_LENGTH,
          scpi_error_queue_data, SCPI_ERROR_QUEUE_SIZE);  
  	scpi_usbtmc_context.user_context = &TAG;
  	SCPI_RegSetBits(&scpi_usbtmc_context,SCPI_REG_ESR,ESR_PON);  
          
	ESP_LOGI(TAG, "USBTMC");

  	usbtmcTxQueue = xQueueCreate (SCPI_OUTPUT_BUFFER_LENGTH, sizeof (char));

  
   	// init device stack on configured roothub port
  	tusb_rhport_init_t dev_init = {
    	.role = TUSB_ROLE_DEVICE,
    	.speed = TUSB_SPEED_AUTO
  	};
   
   	tusb_init(BOARD_TUD_RHPORT, &dev_init);

	ESP_LOGI(TAG, "PMBUS MANAGER");
    
    if (xTaskCreate(&pmbus_manager_task, "pmbus_manager_task", 1024 * 25, NULL, 5, &tasksHandlers[TASK_ID_PMBUS]) == pdPASS) 
    	ESP_LOGI(TAG, "Starting pmbus_manager_task task...");
  	else
    	ESP_LOGE(TAG, "Starting pmbus_manager_task...couldn't allocate memory");   

    if (xTaskCreate(&scpi_socket_server_task, "scpi_socket_server_task", 1024 * 6, (void*)AF_INET, 6, &tasksHandlers[TASK_ID_SCPI_SOCKET]) == pdPASS) 
        ESP_LOGI(TAG, "Starting scpi_socket_server_task task...");
    else
        ESP_LOGE(TAG, "Starting scpi_socket_server_task...couldn't allocate memory");
   
   
  	if (xTaskCreate(&usbtmc_task, "usbtmc_task", 1024 * 10, NULL, 7, &tasksHandlers[TASK_ID_USBTMC]) == pdPASS) 
    	ESP_LOGI(TAG, "Starting tud_task_th State Machine task...");
  	else
    	ESP_LOGE(TAG, "Starting usbtmc_task...couldn't allocate memory"); 
    
       	
    if (xTaskCreate(&websocket_task, "websocket_task", 1024 * 4, NULL, 8, &tasksHandlers[TASK_ID_WEB_SOCKET]) == pdPASS) 
      	ESP_LOGI(TAG, "Starting websocket_task State Machine task...");
  	else
    	ESP_LOGE(TAG, "Starting websocket_task...couldn't allocate memory"); 
    	 
    xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_INIT_DONE, pdFALSE, pdTRUE, pdMS_TO_TICKS(5000));
 	
 	//Trigger PMBus READALL by clearing READALL_COMPLETE
	xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE);
    //Wait for result
    xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE, pdFALSE, pdTRUE, pdMS_TO_TICKS(SMBUS_WAIT_TIME));

    // Set LED color GREEN
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 0, 100, 0));
    // Flush the buffer to the strip
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
    
 	while (1)
  	{
    	tud_task(); // tinyusb device task
        vTaskDelay(pdMS_TO_TICKS(20));
  	}
}
 
 //--------------------------------------------------------------------+
 // Device callbacks
 //--------------------------------------------------------------------+
 
 // Invoked when device is mounted
 void tud_mount_cb(void)
 {

 }
 
 // Invoked when device is unmounted
 void tud_umount_cb(void)
 {

 }
 
 // Invoked when usb bus is suspended
 // remote_wakeup_en : if host allow us  to perform remote wakeup
 // Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
   (void) remote_wakeup_en;

}
 
 // Invoked when usb bus is resumed
void tud_resume_cb(void)
{

}
 