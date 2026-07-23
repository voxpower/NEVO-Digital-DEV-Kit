#include "softap.h"
#include "esp_wifi.h"
#include "main.h"
#include "mdns.h"
#include "esp_log.h"



static const char *TAG = "mDNS";

// Function to initialize and start the mDNS service
esp_err_t initialize_mdns(void) {
    ESP_LOGI(TAG, "Starting mDNS service");

    // Initialize the mDNS service
    esp_err_t err = mdns_init();
    if (err) {
        ESP_LOGE(TAG, "MDNS Init failed: %s", esp_err_to_name(err));
        return err;
    }

    // Set the hostname
    mdns_hostname_set(MDNS_HOSTNAME_PREFIX);

    // Set a user-friendly instance name
    mdns_instance_name_set("NEVO Access Point");

    ESP_LOGI(TAG, "mDNS service started. Hostname: %s",  MDNS_HOSTNAME_PREFIX);
}

/* ******************************************************************************************** */
/*
 * @brief
 * @param
 * @retval
 */
esp_err_t softap_init(void)
{
	esp_err_t res = ESP_OK;

	res |= esp_netif_init();
	res |= esp_event_loop_create_default();
	esp_netif_create_default_wifi_ap();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	res |= esp_wifi_init(&cfg);

	wifi_config_t wifi_config = {
		.ap = {
			.ssid = WIFI_SSID,
			.ssid_len = strlen(WIFI_SSID),
			.channel = 6,
			.authmode = WIFI_AUTH_OPEN,
			.max_connection = 3
		},
	};

	res |= esp_wifi_set_mode(WIFI_MODE_AP);
	res |= esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
	res |= esp_wifi_start();
	res |= initialize_mdns();

	return res;
}