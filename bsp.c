#include "bsp.h"
#include "impulsator.h"

#include <stdio.h>


static bool volUpAvailable = false;
static bool volDnAvailable = false;
bool isVolumeUp()
{
    bool result = volUpAvailable;
    volUpAvailable = false;
    return result;
}

bool isVolumeDown()
{
    bool result = volDnAvailable;
    volDnAvailable = false;
    return result;
}

static int ledsLit[2] = {0};
static repeating_timer_t periodicCheckTimer;
bool periodicCheck(repeating_timer_t *rt)
{
    // Dim the leds
    // printf("from callback, yay\n");
    for (int i = 0; i < 2; i++)
    {
        if (ledsLit[i])
        {
            // __disable_irq();
            // vol up is the first of the two.
            gpio_put(VOL_UP_LED + i, 0);
            ledsLit[i] = 0;
            // __enable_irq();
        }
    }

    return true;
}

// void button_callback(uint gpio, uint32_t event_mask)
// {

// }

static uint8_t volumeEncoderState = ENCODER_INIT;
uint8_t getState() {return volumeEncoderState;}
#define ENCODER 1
void button_callback(uint gpio, uint32_t event_mask)
{
    #ifdef ENCODER
    uint8_t nextState = processEncoder(volumeEncoderState, gpio_get(VOL_UP_BUTTON), gpio_get(VOL_DN_BUTTON));
    printf("[%2d, %2d] %x -> %x", !gpio_get(VOL_UP_BUTTON), !gpio_get(VOL_DN_BUTTON), volumeEncoderState, nextState);
    if ((nextState & DIR_CW) != 0)
    {
        volUpAvailable = true;
        printf(", volUp");
    }
    else if ((nextState & DIR_CCW) != 0)
    {
        volDnAvailable = true;
        printf(", volDn");
    }
    printf("\n");
    volumeEncoderState = nextState;


    // Pamiętaj, że przyciski mają pull-upy.
    if (!gpio_get(VOL_UP_BUTTON))
    // if (gpio == VOL_UP_BUTTON)
    {
        gpio_put(VOL_UP_LED, 1);
        ledsLit[0] = 1;
    }

    if (!gpio_get(VOL_DN_BUTTON))
    // if (gpio == VOL_DN_BUTTON)
    {
        gpio_put(VOL_DN_LED, 1);
        ledsLit[1] = 1;
    }

    // if (!gpio_get(VOL_UP_BUTTON) && !gpio_get(VOL_DN_BUTTON))
    // {
    //     gpio_put(USB_STATUS_LED, 1);
    // }
    // __irq_enable();
    #else
    // Przyciski
    static const uint32_t debounceDelay = 100;
    static uint32_t lastMs = 0;

    const uint32_t currentMs = to_ms_since_boot(get_absolute_time());
    if (currentMs - lastMs <= debounceDelay)
    {
        return;
    }
    lastMs = currentMs;

    // printf("gpio, pin: %d, events: %d\n", gpio, event_mask);

    // __irq_disable();
    switch (gpio)
    {
        case VOL_UP_BUTTON:
            volUpAvailable = true;
            gpio_put(VOL_UP_LED, 1);
            ledsLit[0] = 1;
            break;
        case VOL_DN_BUTTON:
            volDnAvailable = true;
            gpio_put(VOL_DN_LED, 1);
            ledsLit[1] = 1;
            break;
        default:
            break;
    }
    // __irq_enable();
    #endif
}

void bspInit()
{
    if (!add_repeating_timer_ms(100, periodicCheck, NULL, &periodicCheckTimer))
    {
        printf("Failed to add timer\n");
    }
}