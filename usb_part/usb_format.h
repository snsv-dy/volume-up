#ifndef _USB_FORMAT_H_
#define _USB_FORMAT_H_

typedef enum
{
    // ACTION_INCORRECT    = 0,
    ACTION_INC5         = 1,
    ACTION_DEC5         = 2,
    ACTION_INC1         = 3,
    ACTION_DEC1         = 4,
    ACTION_SET24        = 5,
    ACTION_SET29        = 6,
    ACTION_GET_VOLUME   = 7,

    ACTION_LAST,
} DeviceAction;

#endif