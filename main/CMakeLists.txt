idf_component_register(
    SRCS 
        "main.c"  "scpi_socket.c" "smbus.c" "i2c.c" "http.c" "cJSON.c" "pmbus.c" "scpi_http.c"
        "usb_descriptors.c"  
        "usbtmc_app.c" "scpi_usbtmc.c" "LED.c"
		"board_api.c" "softap.c" "common/scpi-def.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/../components/tinyusb/src/class/usbtmc/usbtmc_device.c"  # Updated path
    INCLUDE_DIRS 
        "." "common" 
        "${CMAKE_CURRENT_SOURCE_DIR}/../components/tinyusb/src"       
        "${CMAKE_CURRENT_SOURCE_DIR}/../components/libscpi/src" 
	PRIV_INCLUDE_DIRS ${includes_private}
    REQUIRES  usb nvs_flash esp_wifi libscpi driver esp_timer esp_https_server   
	EMBED_TXTFILES web/index.html web/chart.min.js web/script.js web/style.css
	EMBED_FILES web/logo.png
)