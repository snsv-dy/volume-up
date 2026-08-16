#ifndef _USB_PART_H_
#define _USB_PART_H_

#define INTERRUPT_EP_IN  (0x01 | 0b10000000) // 7th bit is direction (1 - in, 0 - out)
#define INTERRUPT_EP_OUT (0x02 | 0b00000000)
#define INTERRUPT_EP_SIZE 8
#define INTERRUPT_EP_INTERVAL 20 // ms (allowed: 1 - 255)

void usbInit();
void usbLoop();

#endif