#ifndef SCPI_CUSTOM_H
#define SCPI_CUSTOM_H
#include "esp_system.h"
#include "scpi/scpi.h"
#include "../common/scpi-def.h"

#define PORT                        5025
#define KEEPALIVE_IDLE              20
#define KEEPALIVE_INTERVAL          5
#define KEEPALIVE_COUNT             3
#define SCPI_SOCKET_RX_BUFFER_SIZE 128

//scpi_result_t SCPI_SystemCommTcpipControlQ(scpi_t * context);
void scpi_socket_server_task(void *pvParameters);

#endif