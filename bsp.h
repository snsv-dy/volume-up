#ifndef _BSP_H_
#define _BSP_H_

#include <stdint.h>
#include "pico/stdlib.h"
#include <hardware/i2c.h>
// #include "bool.h"

// https://www.raspberrypi.com/documentation/microcontrollers/pico-series.html#layout_wireless
// 
//     PIN|GPIO
// [x]  1 ( 0)    vbus 40 [ ]
// [x]  2 ( 1)    vsys 39 [ ]
// [ ]  3 gnd      gnd 38 [ ]
// [x]  4 ( 2)  3v3_en 37 [ ]
// [x]  5 ( 3)  3v3out 36 [ ]
// [x]  6 ( 4)         35 [ ]
// [ ]  7 ( 5)    (28) 34 [ ]
// [ ]  8 gnd      gnd 33 [ ]
// [ ]  9 ( 6)    (27) 32 [ ]
// [ ] 10 ( 7)    (26) 31 [ ]
// [x] 11 ( 8)     run 30 [ ]
// [x] 12 ( 9)    (22) 29 [ ]
// [ ] 13 gnd      gnd 28 [ ]
// [ ] 14 (10)    (21) 27 [x]
// [ ] 15 (11)    (20) 26 [x]
// [ ] 16 (12)    (19) 25 [ ]
// [ ] 17 (13)    (18) 24 [ ]
// [ ] 18 gnd      gnd 23 [ ]
// [ ] 19 (14)    (17) 22 [x]
// [ ] 20 (15)    (16) 21 [x]


#define VOL_UP_BUTTON 0
#define VOL_DN_BUTTON 1

#define VOL_UP_LED 2
#define VOL_DN_LED 3
#define USB_STATUS_LED 4

#define UART_TX 16
#define UART_RX 17

#define LCD_SDA_GPIO 8
#define LCD_SCL_GPIO 9

typedef struct 
{
    uint8_t address;

    uint8_t displayMode;
    uint8_t displayFunction;
    uint8_t displayControl;
    uint8_t backlight;
    i2c_inst_t* i2cInstance;
} LcdState;

void bspInit();
void button_callback(uint gpio, uint32_t event_mask);
bool isVolumeUp();
bool isVolumeDown();
uint8_t getState();

#endif