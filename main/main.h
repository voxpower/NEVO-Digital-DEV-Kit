/**
 * @file main.h
 * @brief Main application header file.
 *
 * Contains global definitions, external variable declarations, and
 * compile-time configurations for the main application.
 */
#ifndef MAIN_H
#define MAIN_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/** @brief Mutex to protect SCPI channel access. */
extern SemaphoreHandle_t scpi_channel_mutex;

/** @brief Default SSID for the SoftAP mode. */
#define WIFI_SSID "VOXPOWER SOFTAP"
/** @brief Maximum number of devices supported. */
#define MAX_N_DEVICES           8

/** @brief MDNS hostname prefix for the device. */
#define MDNS_HOSTNAME_PREFIX "nevo_ap"

#endif
