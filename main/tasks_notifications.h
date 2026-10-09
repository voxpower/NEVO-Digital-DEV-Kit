#ifndef tasks_notifications_h
#define tasks_notifications_h


typedef enum
{
	SMBUS_PROC_NOTIF_IDLE = 0,
	SMBUS_PROC_NOTIF_CYCLE_COMPLETE =  	1 << 0,
	SMBUS_PROC_NOTIF_WRITE_COMPLETE =  	1 << 1,
	SMBUS_PROC_NOTIF_CYCLE_ERROR =  	1 << 2,
	SMBUS_PROC_NOTIF_WRITE_ERROR =  	1 << 3,
	SMBUS_PROC_NOTIF_READ_COMPLETE =  	1 << 4,
	SMBUS_PROC_NOTIF_SMBUS_READALL_COMPLETE =  		1 << 5,	
	SMBUS_PROC_NOTIF_READ_ERROR =  		1 << 6,        
	SMBUS_PROC_NOTIF_INIT_DONE = 		1<< 7,
	SMBUS_PROC_NOTIF_SMBUS_READDEVICES_COMPLETE = 1<<8,
}smbus_process_notif_t;


#endif /* tasks_notifications_h */