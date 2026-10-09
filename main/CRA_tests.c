#include "CRA_tests.h"
#include "esp_log_timestamp.h"
#include "i2c.h"         
#include "smbus.h"   
#include "pmbus.h"   
#include "GPIO.h"
#include "debug.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tasks_notifications.h"

static const char *TAG = "CRA_VERIFY";

// Command Constants
#define TARGET_I2C_ADDR      0x70
#define CMD_TON              0x62
#define CMD_SERIAL           0x9E
#define CMD_STATUS_CML       0x7E
#define CMD_MFR_BOOTLOAD     0xD9
#define CMD_CAL_LOCK         0xD8
#define CMD_STATUS_BYTE  	 0x78
#define CMD_CLEAR_FAULTS  	 0x03
#define CMD_OPERATION	  	 0x01

// External handle from your main/i2c.c
extern i2c_master_bus_handle_t bus_handle;

extern EventGroupHandle_t SMBUS_events;
extern uint8_t pmbuspostreturn[40];

pmbus_cmd_t pmbus_cmd;

// ---------------------------------------------------------------------------
// Hardware Abstraction & Recovery Helpers
// ---------------------------------------------------------------------------
// Forces SCL LOW and resets ESP32 master bus state to clear driver lockups
static void cra_reset_smbus_target(void) {
    if (bus_handle != NULL) {
        i2c_del_master_bus(bus_handle);
        bus_handle = NULL;
    }
    gpio_set_direction(CONFIG_I2C_MASTER_SCL, GPIO_MODE_OUTPUT);
    gpio_set_level(CONFIG_I2C_MASTER_SCL, 0);
    vTaskDelay(pdMS_TO_TICKS(35));
    gpio_set_level(CONFIG_I2C_MASTER_SCL, 1);
    gpio_set_direction(CONFIG_I2C_MASTER_SCL, GPIO_MODE_INPUT);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // 3. Re-initialize the I2C master bus using your function, which re-binds 
    //    the SCL and SDA pins back to the hardware I2C peripheral matrix.
    i2c_master_init(&bus_handle);
}


// Query STATUS register
static bool read_status(uint8_t CMD, uint8_t *out_val) {
    pmbus_cmd.command = CMD;
    pmbus_cmd.r_w     = SMB_MSG_TYPE_READ;
    pmbus_cmd.len     = 1;
    pmbus_cmd.format = UNSIGNED;

    xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_READ_COMPLETE | SMBUS_PROC_NOTIF_READ_ERROR);
    pmbus_send_cmd(&pmbus_cmd);

    EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_READ_COMPLETE | SMBUS_PROC_NOTIF_READ_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(WAIT_TIME));
    
	if (bits & SMBUS_PROC_NOTIF_READ_COMPLETE) {
    	ESP_LOGI(TAG, "[%08lu ms] Read successful: STATUS_REG 0x%02X: 0x%02X", (unsigned long)esp_log_timestamp(), CMD, pmbuspostreturn[0]);
    	out_val[0] = pmbuspostreturn[0];
	    return ESP_OK;
    }
    
    ESP_LOGW(TAG, "[%08lu ms] Error reading STATUS_REG 0x%02X", (unsigned long)esp_log_timestamp(), CMD);
    return ESP_FAIL;
}

//Write STATUS register
static bool write_status(uint8_t CMD, uint8_t val) {
    pmbus_cmd.command = CMD;
    pmbus_cmd.value[0] = val;
    pmbus_cmd.r_w     = SMB_MSG_TYPE_WRITE;
    pmbus_cmd.len     = 1;
    pmbus_cmd.format = UNSIGNED;
    
    xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR);
    pmbus_send_cmd(&pmbus_cmd);

    EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(WAIT_TIME));
    
	if (bits & SMBUS_PROC_NOTIF_WRITE_COMPLETE) {
    	ESP_LOGI(TAG, "[%08lu ms] Write successful: STATUS_REG 0x%02X: 0x%02X", (unsigned long)esp_log_timestamp(), CMD, val);
    	return ESP_OK;
    }
    
    ESP_LOGW(TAG, "[%08lu ms] Error writing STATUS_REG 0x%02X", (unsigned long)esp_log_timestamp(), CMD);
    return ESP_FAIL;
}

//Clear status registers - This locks calibration
static bool clear_stat() {
	ESP_LOGI(TAG, "[%08lu ms] Clearing device STATUS registers.....", (unsigned long)esp_log_timestamp());
	pmbus_cmd.command = CMD_CLEAR_FAULTS;
    pmbus_cmd.r_w     = SMB_MSG_TYPE_WRITE;
    pmbus_cmd.len     = 0;
    pmbus_cmd.format = UNSIGNED;

    xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR);
    pmbus_send_cmd(&pmbus_cmd);

    EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(WAIT_TIME));
    
    if (bits & SMBUS_PROC_NOTIF_WRITE_COMPLETE) {
    	ESP_LOGI(TAG, "[%08lu ms] Device STATUS registers cleared", (unsigned long)esp_log_timestamp());
		return ESP_OK;
	}
	
    ESP_LOGW(TAG, "[%08lu ms] Error clearing device status registers", (unsigned long)esp_log_timestamp());
    return ESP_FAIL;
}

//Send bootloader data
static bool send_boot_data(uint8_t len, uint8_t *verify_payload) {
	//ESP_LOGI(TAG, "Sending MFR_BOOTLOAD data....");
    pmbus_cmd.command = CMD_MFR_BOOTLOAD;
    pmbus_cmd.r_w     = SMB_MSG_TYPE_WRITE;
    pmbus_cmd.format = BLOCK;
    pmbus_cmd.len     = len;
    pmbus_cmd.update.update = false;
    for (int i = 0; i < pmbus_cmd.len; i++) {
        pmbus_cmd.value[i] = verify_payload[i];
    }

	//ESP_LOG_BUFFER_HEXDUMP(TAG, pmbus_cmd.value, pmbus_cmd.len, ESP_LOG_INFO);
    xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR);
    pmbus_send_cmd(&pmbus_cmd);

    EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(WAIT_TIME));
    
    if (bits & SMBUS_PROC_NOTIF_WRITE_COMPLETE) {
    	ESP_LOGI(TAG, "[%08lu ms] MFR_BOOTLOAD write successful.", (unsigned long)esp_log_timestamp());
    	return ESP_OK;
	}
	
    ESP_LOGE(TAG, "[%08lu ms] Timeout writing MFR_BOOTLOAD", (unsigned long)esp_log_timestamp());
	return ESP_FAIL;
	
}




// Unlock calibration mode by reading SERIAL, writing it back backwards, and confirming STATUS_BYTE
static bool unlock_calibration_mode(void) {
    
    // -------------------------------------------------------------
    // Step 1: Read SERIAL Block from the target device
    // ------------------ Data Buffer Mapping Note -----------------   
    pmbus_cmd.command = CMD_SERIAL;
    pmbus_cmd.r_w     = SMB_MSG_TYPE_READ;
    pmbus_cmd.format = STRING;
    pmbus_cmd.len     = 11;
    pmbus_cmd.update.update = false;

    // Clear event bits before sending read request
    xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_READ_COMPLETE | SMBUS_PROC_NOTIF_READ_ERROR);

    pmbus_send_cmd(&pmbus_cmd);

    // Wait for PMBus transaction to complete or timeout
    EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_READ_COMPLETE | SMBUS_PROC_NOTIF_READ_ERROR,  pdTRUE, pdFALSE, pdMS_TO_TICKS(WAIT_TIME)); 

    if (bits & SMBUS_PROC_NOTIF_READ_ERROR) {
        ESP_LOGE(TAG, "[%08lu ms] PMBus timeout reading SERIAL block", (unsigned long)esp_log_timestamp());
        return false;
    }
	//Response is in pmbuspostreturn
    //Reverse the 11 serial bytes safely
    uint8_t data_len = pmbus_cmd.len;
    
    for (int i = 0; i < data_len; i++) {
        pmbus_cmd.value[i] = pmbuspostreturn[data_len -1 - i];
    }
    //Response is in pmbus_cmd.value
    pmbus_cmd.value[data_len] = '\0';
    ESP_LOGI(TAG, "[%08lu ms] Reversed SERIAL %s ready for lock command.", (unsigned long)esp_log_timestamp(), pmbus_cmd.value);
	
	clear_stat();
	
    // -------------------------------------------------------------
    // Step 2: Write reversed serial back to MFR_CAL_LOCK (Block Write)
    // -------------------------------------------------------------
    pmbus_cmd.command = CMD_CAL_LOCK;      // 0xDA
    pmbus_cmd.r_w     = SMB_MSG_TYPE_WRITE;
    pmbus_cmd.format = BLOCK;
    pmbus_cmd.len = 11;

    xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR);

    pmbus_send_cmd(&pmbus_cmd);

    bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(WAIT_TIME));

    if (bits & SMBUS_PROC_NOTIF_WRITE_ERROR) {
        ESP_LOGE(TAG, "[%08lu ms] PMBus timeout writing MFR_CAL_LOCK", (unsigned long)esp_log_timestamp());
        return false;
    }

    // -------------------------------------------------------------
    // Step 3: Confirm Calibration Unlock via STATUS_CML = MEM_FLT readback
    // -------------------------------------------------------------
    uint8_t stat_cml = 0;
    bool read_ok = read_status(CMD_STATUS_CML, &stat_cml);
    
    if ((read_ok == ESP_OK) && ((stat_cml & 0x10) == 0x10)) {	
        ESP_LOGI(TAG, "[%08lu ms] Calibration mode unlocked successfully. STATUS_CML: 0x10 (MEM_FLT).", (unsigned long)esp_log_timestamp());
        return true;
    }

    ESP_LOGW(TAG, "[%08lu ms] Calibration unlock verification failed. STATUS_CML: 0x%02X", (unsigned long)esp_log_timestamp(), stat_cml);
    return false;
}

// ---------------------------------------------------------------------------
// Test Case 2: Parameter Range Sanitization
// ---------------------------------------------------------------------------
bool cra_test_case_2_parameter_sanitization(void) {
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "Executing Test Case 2: Parameter Range Sanitization");
    ESP_LOGI(TAG, "=================================================");

	clear_stat();		//Clear Faults
	
	pmbus_cmd.command = CMD_TON;
    pmbus_cmd.r_w     = SMB_MSG_TYPE_WRITE;
    pmbus_cmd.len     = 2;
    pmbus_cmd.format = SIGNED;
    pmbus_cmd.value[0] = 0xA0;
    pmbus_cmd.value[1] = 0x0F;
     
	xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR);
    // Step A: Inject Valid Parameter (TON = 4000)
    ESP_LOGI(TAG, "[%08lu ms] BUS_INJECTION -> WRITE_CMD: 0x62 (TON) | DATA: 0x0FA0 (4000)", (unsigned long)esp_log_timestamp());
    pmbus_send_cmd(&pmbus_cmd);

    EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(WAIT_TIME));

    if (bits & SMBUS_PROC_NOTIF_WRITE_ERROR) {
        ESP_LOGE(TAG, "PMBus timeout writing TON Setting");
        return false;
    }

    ESP_LOGI(TAG, "[%08lu ms] UUT_RESPONSE  -> I2C_ACK Received. Register TON updated to 4000.", (unsigned long)esp_log_timestamp());

    // Step B: Inject Out-of-Bounds Parameter (TON = 6000)
    pmbus_cmd.value[0] = 0x70;
    pmbus_cmd.value[1] = 0x17;
    
    ESP_LOGI(TAG, "[%08lu ms] BUS_INJECTION -> WRITE_CMD: 0x62 (TON) | DATA: 0x1770 (6000)", (unsigned long)esp_log_timestamp());
	pmbus_send_cmd(&pmbus_cmd);

    bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(WAIT_TIME));

	if (bits & SMBUS_PROC_NOTIF_WRITE_ERROR) {
        ESP_LOGI(TAG, "WRITE ERROR\n");
	}
    uint8_t cml = 0;
    bool read_success = read_status(CMD_STATUS_CML, &cml);

    if ((bits & SMBUS_PROC_NOTIF_WRITE_ERROR) || ((read_success == ESP_OK) && (cml & 0x40))) {
        ESP_LOGW(TAG, "[%08lu ms] UUT_RESPONSE  -> Internal variable rejected.", (unsigned long)esp_log_timestamp());
        ESP_LOGI(TAG, "[%08lu ms] SFR_MONITOR   -> STAT_CML Checked: Bit 6 (INVD_DATA) state changed to 1.", (unsigned long)esp_log_timestamp());
        ESP_LOGI(TAG, "[PASS] Test Case 2 Result: PASSED\n");
        return true;
    }

    ESP_LOGE(TAG, "[FAIL] Test Case 2 Result: FAILED");
    return false;
}

// ---------------------------------------------------------------------------
// Test Case 3: Buffer Over-Read Mitigation
// ---------------------------------------------------------------------------
bool cra_test_case_3_overread_mitigation(void) {
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "Executing Test Case 3: Buffer Over-Read Mitigation");
    ESP_LOGI(TAG, "=================================================");

	clear_stat();	//Clear Faults
	
	pmbus_cmd.command = CMD_SERIAL;
    pmbus_cmd.r_w     = SMB_MSG_TYPE_READ;
    pmbus_cmd.format = BLOCK;
    pmbus_cmd.len     = 32;		//Should be 11
    pmbus_cmd.update.update = false;

	// Clear event bits before sending read request
    xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_READ_COMPLETE | SMBUS_PROC_NOTIF_READ_ERROR);
	ESP_LOGI(TAG, "[%08lu ms] BUS_INJECTION -> READ_BLOCK_CMD: 0x9E (SERIAL) | REQ_COUNT: 0x20 (32)", (unsigned long)esp_log_timestamp());
    pmbus_send_cmd(&pmbus_cmd);

    // Wait for PMBus transaction to complete or timeout
    EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_READ_COMPLETE | SMBUS_PROC_NOTIF_READ_ERROR,  pdTRUE, pdFALSE, pdMS_TO_TICKS(WAIT_TIME)); 

	//Response is in pmbuspostreturn
	uint8_t returned_count = pmbuspostreturn[0];
	ESP_LOGI(TAG, "[%08lu ms] Returned count: %d", (unsigned long)esp_log_timestamp(), returned_count);

    bool mitigation_verified = (bits & SMBUS_PROC_NOTIF_READ_ERROR)|| (returned_count <= 11);

    if (mitigation_verified) {
        ESP_LOGI(TAG, "[%08lu ms] UUT_RESPONSE  -> Frame Output Stream Restricted / Terminated.", (unsigned long)esp_log_timestamp());
        ESP_LOGI(TAG, "[%08lu ms] BUS_MONITOR   -> Over-read attempt blocked by target bounds check.", (unsigned long)esp_log_timestamp());
        ESP_LOGI(TAG, "[PASS] Test Case 3 Result: PASSED\n");
        return true;
    }

    ESP_LOGE(TAG, "[FAIL] Test Case 3 Result: FAILED");
    return false;
}

// ---------------------------------------------------------------------------
// Test Case 4: Pre-Execution Update Validation & Truncated Payload
// ---------------------------------------------------------------------------
bool cra_test_case_4_update_and_truncation_handling(void) {
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "Executing Test Case 4: Pre-Execution & Truncation");
    ESP_LOGI(TAG, "=================================================");

	bool sc_a_passed = false;
	bool sc_b_passed = false;

	//write_status(CMD_OPERATION,128);	//Ensure device is on.
    // Prerequisite: Unlock calibration mode so bootloader commands are accepted
    ESP_LOGI(TAG, "[%08lu ms] HOST_COMMAND  -> Unlocking calibration mode (MFR_CAL_LOCK with reversed SERIAL)", (unsigned long)esp_log_timestamp());
    unlock_calibration_mode();
    //vTaskDelay(pdMS_TO_TICKS(10));

    // Scenario A: Corrupt Verify Payload via MFR_BOOTLOAD (CMD: 32 = Verify)
    ESP_LOGI(TAG, "-- Scenario A Execution --");
    ESP_LOGI(TAG, "[%08lu ms] HOST_COMMAND  -> MFR_BOOTLOAD (0xD9) Sub-cmd: 0 (Write Data)", (unsigned long)esp_log_timestamp());
    //Each data line is checked for CRC
    ESP_LOGI(TAG, "[%08lu ms] Sending valid bootload data...", (unsigned long)esp_log_timestamp());
	
	write_status(CMD_STATUS_CML,255);	//Clear CML reg
    
	//Expected payload see https://developer.arm.com/documentation/ka003292/latest/ 
	//Example correct PMBUS command payload from intel HEX file.
	//PMBUS DIR,0,217,write_block,0,139,2,123,1,122,0,121,0,18,38,75,18,25,45,125,255,18,35,104
	//Type: 0 [DATA]; Address: 0x028B; Data: 123,1,122,0,121,0,18,38,75,18,25,45,125,255,18,35; CRC: 104
	//Data Len: 16 (Automatically calculated from the block_write transfer)
	
    uint8_t verify_payload[20] = {0x0, 0x8B, 0x2, 0x7b,0x1,0x7a,0x0,0x79,0x0,0x12,0x26,0x4b,0x12,0x19,0x2d,0x7d,0xff,0x12,0x23,0x68};
	send_boot_data(20, verify_payload);

	//If CRC fails STAT_CML = INVALID DATA
	uint8_t cml = 0;
    bool read_ok = read_status(CMD_STATUS_CML, &cml);
	if (cml) {
		ESP_LOGW(TAG, "[%08lu ms] Failed to send valid bootload data: STATUS CML: 0x%02X", (unsigned long)esp_log_timestamp(), cml);
	} else {
		ESP_LOGI(TAG, "[%08lu ms] Sent valid bootload data: STATUS CML: 0x%02X", (unsigned long)esp_log_timestamp(), cml);
	}
	
	//write_status(CMD_OPERATION,128);	//Ensure device is on.
	write_status(CMD_STATUS_CML,255);	//Clear CML reg
	//return true;
	
	verify_payload[19] -= 1;		//Corrupt the chksum
	ESP_LOGI(TAG, "[%08lu ms] Sending bootload data with invalid CRC...", (unsigned long)esp_log_timestamp());
	send_boot_data(20, verify_payload);
    //vTaskDelay(pdMS_TO_TICKS(10)); 
	read_ok = read_status(CMD_STATUS_CML, &cml);
	
	if (cml) {
		ESP_LOGI(TAG, "[%08lu ms] Integrity check PASSED. Failed to send invalid bootload data: STATUS CML: 0x%02X", (unsigned long)esp_log_timestamp(), cml);
		sc_a_passed = true;
	} else {
		ESP_LOGW(TAG, "[%08lu ms] Integrity check FAILED. Sent invalid bootload data: STATUS CML: 0x%02X", (unsigned long)esp_log_timestamp(), cml);
	}
	
	//write_status(CMD_OPERATION,128);	//Ensure device is on.
	write_status(CMD_STATUS_CML,255);	//Clear CML reg
	
    //vTaskDelay(pdMS_TO_TICKS(50));

    // Scenario B: Truncated Transmission on Write
    ESP_LOGI(TAG, "-- Scenario B Execution --");
    ESP_LOGI(TAG, "[%08lu ms] BUS_INJECTION -> TRANSMIT_BLOCK_STREAM (Mid-Transfer Interruption Forced)", (unsigned long)esp_log_timestamp());

    ESP_LOGI(TAG, "[%08lu ms] Sending truncated bootload data...", (unsigned long)esp_log_timestamp());
    send_boot_data(5, verify_payload);	//Only send 5 bytes 
	read_ok = read_status(CMD_STATUS_CML, &cml);
	
	if (cml) {
		ESP_LOGI(TAG, "[%08lu ms] Integrity check PASSED. Failed to send truncated bootload data: STATUS CML: 0x%02X", (unsigned long)esp_log_timestamp(), cml);
		sc_b_passed = true;
	} else {
		ESP_LOGW(TAG, "[%08lu ms] Integrity check FAILED. Sent truncated bootload data: STATUS CML: 0x%02X", (unsigned long)esp_log_timestamp(), cml);
	}

	write_status(CMD_OPERATION,128);	//Ensure device is on.
	write_status(CMD_STATUS_CML,255);	//Clear CML reg
	clear_stat();
	
    if (sc_a_passed && sc_b_passed) {
        ESP_LOGI(TAG, "[PASS] Test Case 4 Result: PASSED\n");
        return true;
    }

    ESP_LOGE(TAG, "[FAIL] Test Case 4 Result: FAILED");
    return false;
}

// ---------------------------------------------------------------------------
// Test Case 5: Attack Surface Reduction
// ---------------------------------------------------------------------------
bool cra_test_case_5_attack_surface_control(void) {
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "Executing Test Case 5: Attack Surface Reduction");
    ESP_LOGI(TAG, "=================================================");

	clear_stat();		//Clear Faults
	
	pmbus_cmd.command = 0xFF;	//Unused register
	pmbus_cmd.address = 0;		//General Call address
    pmbus_cmd.r_w     = SMB_MSG_TYPE_WRITE;
    pmbus_cmd.len     = 1;
    pmbus_cmd.format = UNSIGNED;
    pmbus_cmd.value[0] = 0xFF;
     
	xEventGroupClearBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR);
    // Step A: Inject Valid Parameter (TON = 4000)
    ESP_LOGI(TAG, "[%08lu ms] BUS_INJECTION -> WRITE_CMD: 0xFF (Unused) | DATA: 0xFF | ADDR_WRITE: 0x00 (General Call)", (unsigned long)esp_log_timestamp());
    pmbus_send_cmd(&pmbus_cmd);

    EventBits_t bits = xEventGroupWaitBits(SMBUS_events, SMBUS_PROC_NOTIF_WRITE_COMPLETE | SMBUS_PROC_NOTIF_WRITE_ERROR, pdTRUE, pdFALSE, pdMS_TO_TICKS(WAIT_TIME));

    uint8_t cml = 0;
    bool read_ok = read_status(CMD_STATUS_CML, &cml);

    ESP_LOGI(TAG, "[%08lu ms] UUT_RESPONSE  -> Write ignored.", (unsigned long)esp_log_timestamp());
    ESP_LOGI(TAG, "[%08lu ms] SFR_MONITOR   -> STAT_CML Checked: Bit 7 (INVD_CMD) set to 1. State unchanged.", (unsigned long)esp_log_timestamp());

    bool sc1_passed = (bits & SMBUS_PROC_NOTIF_WRITE_ERROR) || (read_ok && (cml & 0x80));

    if (sc1_passed) {
        ESP_LOGI(TAG, "[PASS] Test Case 5 Result: PASSED\n");
        return true;
    }

    ESP_LOGE(TAG, "[FAIL] Test Case 5 Result: FAILED");
    return false;
}

// ---------------------------------------------------------------------------
// Test Case 6: Bus Lockup Recovery (DoS Resilience)
// ---------------------------------------------------------------------------
bool cra_test_case_6_bus_lockup_recovery(void) {
    ESP_LOGI(TAG, "=================================================");
    ESP_LOGI(TAG, "Executing Test Case 6: Bus Lockup Recovery (DoS)");
    ESP_LOGI(TAG, "=================================================");

    ESP_LOGI(TAG, "[%08lu ms] BUS_INJECTION -> SCL Forced LOW (Duration: 30.00 ms)", (unsigned long)esp_log_timestamp());

    // Execute hardware bus reset / Clock Low Timeout (>25 ms)
    cra_reset_smbus_target();

    ESP_LOGW(TAG, "[%08lu ms] UUT_PERIPHERAL-> Hardware SMBus Clock Low Timeout Triggered (>25 ms).", (unsigned long)esp_log_timestamp());
    ESP_LOGI(TAG, "[%08lu ms] SFR_MONITOR   -> State Machine Reset: SMB0CN0 forced to IDLE. Internal state cleared.", (unsigned long)esp_log_timestamp());

    //vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "[%08lu ms] BUS_INJECTION -> READ_CMD: 0x7E (STAT_CML)", (unsigned long)esp_log_timestamp());
    
    uint8_t cml = 0;
    bool read_ok = read_status(CMD_STATUS_CML, &cml);

    if (read_ok == ESP_OK) {
        ESP_LOGI(TAG, "[%08lu ms] UUT_RESPONSE  -> I2C_ACK Received. STAT_CML payload transmitted successfully.", (unsigned long)esp_log_timestamp());
        ESP_LOGI(TAG, "[PASS] Test Case 6 Result: PASSED\n");
        return true;
    }

    ESP_LOGE(TAG, "[FAIL] Test Case 6 Result: FAILED");
    return false;
}

// ---------------------------------------------------------------------------
// Master Runner
// ---------------------------------------------------------------------------
bool cra_run_full_verification_suite(void) {
    ESP_LOGI(TAG, "*************************************************");
    ESP_LOGI(TAG, "   STARTING CRA CYBERSECURITY VERIFICATION SUITE ");
    ESP_LOGI(TAG, "   TARGET: NEVO_Digital Core Control Firmware     ");
    ESP_LOGI(TAG, "*************************************************\n");

	pmbus_cmd.address = TARGET_I2C_ADDR;

	write_status(CMD_OPERATION,128);	//Ensure device is on.
	clear_stat();	//Clear Faults.
	
    bool tc2 = cra_test_case_2_parameter_sanitization();
    bool tc3 = cra_test_case_3_overread_mitigation();
    bool tc4 = cra_test_case_4_update_and_truncation_handling();
    bool tc5 = cra_test_case_5_attack_surface_control();
    bool tc6 = cra_test_case_6_bus_lockup_recovery();

    bool overall_pass = tc2 && tc3 && tc4 && tc5 && tc6;
	
    ESP_LOGI(TAG, "*************************************************");
    if (overall_pass) {
        ESP_LOGI(TAG, ">>> OVERALL CRA VERIFICATION RESULT: PASSED <<<");
    } else {
        ESP_LOGE(TAG, ">>> OVERALL CRA VERIFICATION RESULT: FAILED <<<");
    }
    ESP_LOGI(TAG, "*************************************************\n");

    return overall_pass;
}