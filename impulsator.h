#ifndef _IMPULSATOR_H_
#define _IMPULSATOR_H_

#include "stdint.h"
#include <stdbool.h>

#define ENCODER_INIT 0
#define DIR_CW 0x10
#define DIR_CCW 0x20

uint8_t processEncoder(uint8_t state, bool pin1, bool pin2);

#endif
