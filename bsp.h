#ifndef _BSP_H_
#define _BSP_H_

#include <stdint.h>
#include "pico/stdlib.h"
// #include "bool.h"

#define VOL_UP_BUTTON 0
#define VOL_DN_BUTTON 1

#define VOL_UP_LED 2
#define VOL_DN_LED 3
#define USB_STATUS_LED 4

#define UART_TX 16
#define UART_RX 17

void bspInit();
void button_callback(uint gpio, uint32_t event_mask);
bool isVolumeUp();
bool isVolumeDown();
uint8_t getState();

#endif