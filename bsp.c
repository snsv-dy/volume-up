#include "bsp.h"
#include "impulsator.h"
#include "lcd_commands.h"

#include <stdio.h>
#include <hardware/i2c.h>


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
    // printf("[%2d, %2d] %x -> %x", !gpio_get(VOL_UP_BUTTON), !gpio_get(VOL_DN_BUTTON), volumeEncoderState, nextState);
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

#define LCD_ADDR 0x27
#define BAUD_RATE 100000

// #define LCD_SDA_GPIO 8
// #define LCD_SCL_GPIO 9

// #define LCD_ADDR 0x70 // ??

void i2cWriteByte(LcdState* lcdState, uint8_t value)
{
    static uint8_t data;

    data = value | lcdState->backlight;
    i2c_write_blocking(lcdState->i2cInstance, lcdState->address, &data, 1, false);
}

void pulseEnable(LcdState* lcdState, uint8_t value)
{
    const static uint16_t delay = 600;

    // Wtf any of this do?!?!?
    sleep_us(delay);
    i2cWriteByte(lcdState, value | LCD_ENABLE);
    sleep_us(delay);
    i2cWriteByte(lcdState, value & ~LCD_ENABLE);
    sleep_us(delay);
}

void sendNibble(LcdState* lcdState, const uint8_t value)
{
    i2cWriteByte(lcdState, value);
    pulseEnable(lcdState, value);
}

void sendByte(LcdState* lcdState, const uint8_t value, const uint8_t mode)
{
    sendNibble(lcdState, ( value       & 0xf0) | mode);
    sendNibble(lcdState, ((value << 4) & 0xf0) | mode);
}

void sendCommand(LcdState* lcdState, const uint8_t value)
{
    sendByte(lcdState, value, LCD_COMMAND);
}

void lcdDisplayOn(LcdState* lcdState)
{
    lcdState->displayControl |= LCD_DISPLAY_ON;
    sendByte(lcdState, lcdState->displayControl | LCD_DISPLAY_CONTROL, LCD_COMMAND);
}

void lcdClear(LcdState* lcdState)
{
    sendByte(lcdState, LCD_CLEAR_DISPLAY, LCD_COMMAND);
}

void lcdHome(LcdState* lcdState)
{
    sendByte(lcdState, LCD_RETURN_HOME, LCD_COMMAND);
}

void lcdSendChar(LcdState* lcdState, uint8_t value)
{
    sendByte(lcdState, value, LCD_CHAR);
}
void lcdSetCursor(LcdState* lcdState, uint8_t row, uint8_t column)
{
    if (row > LCD_N_ROWS)
    {
        row > LCD_N_ROWS;
    }

    if (column > LCD_N_COLUMNS)
    {
        column = LCD_N_COLUMNS;
    }

    sendByte(lcdState, LCD_SET_DDRAM_ADDR | (row * 0x40 + column), LCD_COMMAND);
}

void initLcd(LcdState* lcdState)
{
    lcdState->i2cInstance = PICO_DEFAULT_I2C_INSTANCE();
    lcdState->address = LCD_ADDR;
    lcdState->backlight = LCD_BACKLIGHT;
    // lcdState->backlight = LCD_NO_BACKLIGHT;

    i2c_init(lcdState->i2cInstance, BAUD_RATE);
    gpio_set_function(LCD_SDA_GPIO, GPIO_FUNC_I2C);
    gpio_set_function(LCD_SCL_GPIO, GPIO_FUNC_I2C);

    gpio_set_pulls(LCD_SDA_GPIO, true, false);
    gpio_set_pulls(LCD_SCL_GPIO, true, false);

    lcdState->displayMode = LCD_ENTRY_LEFT | LCD_ENTRY_SHIFT_DECREMENT;
    lcdState->displayFunction = LCD_MODE_4_BIT | LCD_LINE_2 | LCD_DOTS_5x8;
    lcdState->displayControl = LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF;

    sendCommand(lcdState, 0x03);
    sendCommand(lcdState, 0x03);
    sendCommand(lcdState, 0x03);
    sendCommand(lcdState, 0x02);

    sendCommand(lcdState, LCD_ENTRY_MODE_SET | lcdState->displayMode);
    sendCommand(lcdState, LCD_FUNCTION_SET | lcdState->displayFunction);
    lcdDisplayOn(lcdState);
    lcdClear(lcdState);
    lcdHome(lcdState);

    lcdSetCursor(lcdState, 0, 0);
    lcdSendChar(lcdState, 'A');
}

static LcdState __globalLcdState = {0};

void bspInit()
{
    if (!add_repeating_timer_ms(100, periodicCheck, NULL, &periodicCheckTimer))
    {
        printf("Failed to add timer\n");
    }

    initLcd(&__globalLcdState);
}