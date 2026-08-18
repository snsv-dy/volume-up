#ifndef _DEVICE_INTERFACE_H_
#define _DEVICE_INTERFACE_H_

#include <stdint.h>
#include <semaphore.h>

// WAŻNE!!!
// Wartość action jest zdefiniowana w usb_format.h które różni się od action używanego w audio.c.
// WAŻNE!!!
typedef void (*ActionCallback)(uint32_t action, void* userData);

// 0 - success else not inited.
// callback - called when device generated an event
// userData - passed to callback
// closing  - post this semaphore to close connection to device and return.
int driverInit(ActionCallback callback, void* userData, sem_t* closing);
void volumeChanged(uint8_t volumePercent);
// void setActionCallback(ActionCallback callback);
// void setActionCallback(ActionCallback, void* userData);

#endif