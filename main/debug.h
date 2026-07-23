
#ifndef debug_h
#define debug_h



//DEBUG
#define DEBUG_SCPI       1
#define DEBUG_PMBUS       1
#define DEBUG_HTTP       1

#define DISABLE_4_SMBUSADDRESS	1

//#define SAVE_AUTOADDRESS 1

//Use SDKconfig to disable console output to UART when using UART for communication interfaces such as CANBUS, MODBUS etc
//Menuconfig-> component config -> ESP system settings -> channel for console output = None. Default is UART0

#endif