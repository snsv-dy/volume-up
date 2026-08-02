#include "impulsator.h"

typedef enum {
    START = ENCODER_INIT,
    CW_FINAL = 1,
    CW_BEGIN = 2,
    CW_NEXT = 3,
    CCW_BEGIN = 4,
    CCW_FINAL = 5,
    CCW_NEXT = 6
} encoder_state;

static const uint8_t states[][4] = {
    // START
    {START, CW_BEGIN, CCW_BEGIN, START},
    // CW_FINAL
    {CW_NEXT, START, CW_FINAL, START | DIR_CW},
    // CW_BEGIN
    {CW_NEXT, CW_BEGIN, START, START},
    // CW_NEXT
    {CW_NEXT, CW_BEGIN, CW_FINAL, START},
    // CCW_BEGIN
    {CCW_NEXT, START, CCW_BEGIN, START},
    // CCW_FINAL
    {CCW_NEXT, CCW_FINAL, START, START | DIR_CCW},
    // CCW_NEXT
    {CCW_NEXT, CCW_FINAL, CCW_BEGIN, START}


    // // START
    // {START, CW_BEGIN, CCW_BEGIN, START},
    // // CW_FINAL
    // {CW_NEXT, START, CW_FINAL, START | DIR_CW},
    // // CW_BEGIN
    // {START, CW_BEGIN, START, CW_NEXT},
    // // CW_NEXT
    // {CW_NEXT, CW_BEGIN, CW_FINAL, START},
    // // CCW_BEGIN
    // {CCW_NEXT, START, CCW_BEGIN, START},
    // // CCW_FINAL
    // {CCW_NEXT, CCW_FINAL, START, START | DIR_CCW},
    // // CCW_NEXT
    // {CCW_NEXT, CCW_FINAL, CCW_BEGIN, START}
};

uint8_t processEncoder(uint8_t state, bool pin1, bool pin2)
{
    uint8_t pinstate = ((uint8_t)pin2 << 1) | (uint8_t)pin1;
    return states[(state) & 0xF][pinstate];
    // return state;
}