#ifndef _USB_FORMAT_H_
#define _USB_FORMAT_H_

typedef enum
{
    ACTION_INCORRECT    = 0,
    ACTION_INC5         = 1,
    ACTION_DEC5         = 2,
    ACTION_INC1         = 3,
    ACTION_DEC1         = 4,
    ACTION_SET24        = 5,
    ACTION_SET29        = 6,
    ACTION_GET_VOLUME   = 7, // Sent from host followed by a float
    ACTION_ACK          = 8, // Sent to host after get volume action was received.
    ACTION_DEVICE_SHUTDOWN = 9, // Sent to audio.c from driver when there is a problem with device.

    ACTION_LAST,
} DeviceAction;

#endif
