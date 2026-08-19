#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/gpio.h"

#include "usb_part/usb_part.h"
#include "bsp.h"

// #define DEBOUNCE_DELAY_MS 



void initLedGpio(const uint32_t gpioN)
{
    gpio_init(gpioN);
    gpio_set_dir(gpioN, true);
    // gpio_set_input_enabled(gpioN, false);
    gpio_put(gpioN, 0);
}

int main()
{
    usbInit();
    stdio_init_all();

    // Initialise the Wi-Fi chip
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    // move

    gpio_init(VOL_UP_BUTTON);
    gpio_set_input_enabled(VOL_UP_BUTTON, true);
    gpio_set_pulls(VOL_UP_BUTTON, true, false);
    gpio_set_dir(VOL_UP_BUTTON, false);
    gpio_set_irq_enabled_with_callback(VOL_UP_BUTTON, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &button_callback);
    // gpio_set_irq_enabled_with_callback(VOL_UP_BUTTON, GPIO_IRQ_LEVEL_LOW, true, &button_callback);

    gpio_init(VOL_DN_BUTTON);
    gpio_set_input_enabled(VOL_DN_BUTTON, true);
    gpio_set_pulls(VOL_DN_BUTTON, true, false);
    gpio_set_dir(VOL_DN_BUTTON, false);
    gpio_set_irq_enabled_with_callback(VOL_DN_BUTTON, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &button_callback);
    // gpio_set_irq_enabled_with_callback(VOL_DN_BUTTON, GPIO_IRQ_LEVEL_LOW, true, &button_callback);


    initLedGpio(VOL_UP_LED);
    initLedGpio(VOL_DN_LED);
    initLedGpio(USB_STATUS_LED);    

    gpio_init(UART_TX);
    gpio_init(UART_RX);
    gpio_set_function(UART_TX, 2);
    gpio_set_function(UART_RX, 2);

    bspInit();

    printf("Yay!\n");
    while (true) {
        // static int count = 0;
        // printf("Hello, world! %d\n", count++);
        usbLoop();
        // static int count = 0;
        // printf("Hello, world! %d\n", count++);
        // sleep_ms(1000);
    }
}
