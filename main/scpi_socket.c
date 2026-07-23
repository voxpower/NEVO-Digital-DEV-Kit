#include "esp_log.h"
#include "scpi_socket.h"
#include "tasks_notifications.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "freertos/event_groups.h"
#include "debug.h"

static const char *TAG = "SCPI SOCKET";

extern EventGroupHandle_t SMBUS_events;


/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */

size_t SCPI_socket_Write(scpi_t * context, const char * data, size_t len) {
    if (context->user_context == NULL) {
        return 0;  // No context, nothing to write
    }

    int fd = *(int *) (context->user_context);

    // Save the original state of TCP_NODELAY
    int original_state;
    socklen_t optlen = sizeof(original_state);
    if (getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &original_state, &optlen) == -1) {
        ESP_LOGE(TAG, "getsockopt failed: %d", errno);
        return 0;
    }

    // Disable Nagle algorithm (set TCP_NODELAY to 1)
    int state = 1;
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &state, sizeof(state)) == -1) {
        ESP_LOGE(TAG, "getsockopt failed: %d", errno);
        return 0;
    }

    // Write data to the socket
    ESP_LOGI(TAG, "Writing data to socket %s", data);
    size_t total_written = 0;
    while (total_written < len) {
        ssize_t written = write(fd, data + total_written, len - total_written);
        if (written == -1) {
            if (errno == EINTR) {
                continue;  // Retry if interrupted by a signal
            }
            break;  // Other errors
        }
        total_written += written;
    }

    // Restore the original state of TCP_NODELAY
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &original_state, sizeof(original_state)) == -1) {
        // Handle error (e.g., log or return an error code)
    }

    return total_written;  // Return the total number of bytes written
}

/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */

scpi_result_t SCPI_socket_Flush(scpi_t * context) 
{
    if (context->user_context != NULL) {
        int fd = *(int *) (context->user_context);

        // Save the original state of TCP_NODELAY
        int original_state;
        socklen_t len = sizeof(original_state);
        if(getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &original_state, &len) == -1) {
            ESP_LOGW(TAG, "getsockopt failed: %d", errno);
            return SCPI_RES_ERR;
		}

        // Disable TCP_NODELAY (enable Nagle algorithm)
        int state = 0;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &state, sizeof(state)) == -1) {
            ESP_LOGW(TAG, "getsockopt failed: %d", errno);
            return SCPI_RES_ERR;
        }

        // Restore the original state of TCP_NODELAY
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &original_state, sizeof(original_state)) == -1) 
		{
			ESP_LOGW(TAG, "setsockopt failed: %d", errno);
            return SCPI_RES_ERR;
        }
    }
    return SCPI_RES_OK;
}

/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */

int SCPI_socket_Error(scpi_t * context, int_fast16_t err) {
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

scpi_result_t SCPI_socket_Control(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    (void) context;

    if (SCPI_CTRL_SRQ == ctrl) {
        #ifdef  DEBUG_SCPI
        	ESP_LOGI(TAG, "**SRQ: 0x%X (%d)\r\n", val, val);
        #endif
    } else {
        #ifdef  DEBUG_SCPI
        	ESP_LOGI(TAG, "**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
        #endif
    }
    return SCPI_RES_OK;
}

/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */

scpi_result_t SCPI_socket_Reset(scpi_t * context) {
    (void) context;

    #ifdef  DEBUG_SCPI
    	ESP_LOGI(TAG, "**Reset\r\n");
    #endif
    return SCPI_RES_OK;
}


/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
static void scpi_socket_parser(const int sock)
{
    int len;
    char rx_buffer[SCPI_SOCKET_RX_BUFFER_SIZE];

    do {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
			ESP_LOGE(TAG, "recv() error: %d", errno);
        } else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
        } else {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            #ifdef  DEBUG_SCPI
            	ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);
			#endif            
			SCPI_Input(&scpi_TCP_context, rx_buffer, len);
        }
    } while (len > 0);
}

/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
void scpi_socket_server_task(void *pvParameters)
{

    char addr_str[128];
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;
	bool bootsocket = true;
	struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
	int listen_sock, opt, err; 
    xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_INIT_DONE, pdFALSE, pdTRUE, pdMS_TO_TICKS(5000)); 
    while (1) 
	{
		if (bootsocket)
		{
			bootsocket = false;
			if (addr_family == AF_INET) {
				dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
				dest_addr_ip4->sin_family = AF_INET;
				dest_addr_ip4->sin_port = htons(PORT);
				ip_protocol = IPPROTO_IP;
			}
		
			listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
			if (listen_sock < 0) {
				ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
		    	vTaskDelay(pdMS_TO_TICKS(5000));  // Wait 5 seconds
    			continue;  // Retry
			}
			opt = 1;
			setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		
			#ifdef  DEBUG_SCPI
				ESP_LOGI(TAG, "Socket created");
			#endif
		
			err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
			if (err != 0) {
				ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
				ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
				goto CLEAN_UP;
			}
			#ifdef  DEBUG_SCPI
				ESP_LOGI(TAG, "Socket bound, port %d", PORT);
			#endif
		
			err = listen(listen_sock, 1);
			if (err != 0) {
				ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
				goto CLEAN_UP;
			}
		}

        #ifdef  DEBUG_SCPI
        	ESP_LOGI(TAG, "Socket listening");
        #endif

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) 
		{
				ESP_LOGW(TAG, "Invalid socket, recreating listening socket...\n");

                // Close the invalid socket
                close(listen_sock);
				bootsocket = true;

                // Retry the accept call
                continue;
        }

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        // Convert ip address to string
        if (source_addr.ss_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        }

        #ifdef  DEBUG_SCPI
        	ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);
        #endif
        
		scpi_TCP_context.user_context = &sock;

        scpi_socket_parser(sock);

        if (shutdown(sock, 0) < 0) {
		    ESP_LOGW(TAG, "Shutdown failed: %d", errno);
		}
		if (close(sock) < 0) {
		    ESP_LOGW(TAG, "Close failed: %d", errno);
		}
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}
