#ifndef BLE_NOTIFICATIONS_H_
#define BLE_NOTIFICATIONS_H_

class BLENotifications {
public:
	void beginListening(void (*msgCallback)(char*));
	
};
	
#endif