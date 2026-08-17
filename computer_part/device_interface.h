#ifndef _DEVICE_INTERFACE_H_
#define _DEVICE_INTERFACE_H_

#include <stdint.h>

typedef void (*ActionCallback)(uint32_t action);

// 0 - success else not inited.
int driverInit();
void volumeChanged(uint8_t volumePercent);
// void setActionCallback(ActionCallback callback);
void setActionCallback(ActionCallback);

#endif