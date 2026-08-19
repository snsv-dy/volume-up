#include "usb_part/usb_part.h"
#include "bsp/board_api.h"
#include "bsp.h"
#include "tusb.h"
#include "pico/stdlib.h"
#include "usb_format.h"

//https://github.com/hathach/tinyusb/blob/master/examples/device/hid_multiple_interface/src/usb_descriptors.c


// uint8_t const desc_hid_report1[] =
// {
//     TUD_HID_REPORT_DESC_KEYBOARD()
// };

// uint8_t const desc_hid_report2[] =
// {
//     TUD_HID_REPORT_DESC_MOUSE()
// };

// uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf)
// {
//     if (itf == 0)
//     {
//         return desc_hid_report1;
//     }
//     else if (itf == 1)
//     {
//         return desc_hid_report2;
//     }

//     return NULL;
// }

// enum
// {
//     ITF_NUM_HID1,
//     ITF_NUM_HID2,
//     ITF_NUM_TOTAL,
// };



enum {
    ITF_KEYBOARD = 0,
    ITF_MOUSE = 1,
    ITF_JACEK = 2
};

// Blink pattern in ms.
enum {
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED = 1000,
    BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task();
// void hid_task();
//
// Funkcje widoczne na zewnątrz
//
void usbInit()
{
    board_init();

    tusb_rhport_init_t dev_init = 
    {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };
    tusb_init(BOARD_TUD_RHPORT, &dev_init);
    
    board_init_after_tusb();
}

void vendor_task();
void usbLoop()
{
    tud_task();
    led_blinking_task();

    vendor_task();
}

// Callbacks
void tud_mount_cb(void)
{
    blink_interval_ms = BLINK_MOUNTED;
}

void tud_umount_cb(void)
{
    blink_interval_ms = BLINK_NOT_MOUNTED;
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
    blink_interval_ms = BLINK_SUSPENDED;
}

void tud_resume_cb(void)
{
    blink_interval_ms = tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED;
}

static bool writeRequested = 0;
// void tud_ven
void tud_vendor_int_tx_cb(uint8_t itf, uint32_t sent_bytes) {
    // INFO("Sent 0x%02x bytes", sent_bytes);
    printf("tud_vendor_tx_cb: %d\n", sent_bytes);
    writeRequested = 0;

}
void tud_vendor_int_rx_cb(uint8_t itf, const uint8_t *buffer, uint32_t bufsize) 
{
    printf("tud_vendor_int_rx_cb\n");

    uint32_t bufferSizeLeft = bufsize;
    if (bufsize == 0)
    {
        printf("tud_vendor_rx_cb: bufsize == 0\n");
    }
    else if (!buffer)
    {
        printf("tud_vendor_rx_cb: buffer == null\n");
    }
    else
    {
        uint8_t action = buffer[0];
        bufferSizeLeft--;
        if (bufferSizeLeft > 0 && action == ACTION_GET_VOLUME)
        {
            uint8_t volume = buffer[1];
            
            printf("Received volume: %d\n", volume);
        }
        // printf("tud_vendor_rx_cb: itf: %d: ", itf);
        //     for (int i = 0; i < bufsize; i++)
        //     {
        //         printf("%c", buffer[i]);
        //     }
        //     printf(" (");
        //     for (int i = 0; i < bufsize; i++)
        //     {
        //         printf("0x%x, ", buffer[i]);
        //     }
        //     printf(")\n");
    }

    tud_vendor_n_int_read_xfer(itf);
}

bool tud_vendor_control_xfer_cb(uint8_t rhport, uint8_t stage, tusb_control_request_t const* request) {
    // request->bmRequestType_bit.direction;
    printf("tud_vendor_control_xfer_cb\n");
    return false;
}

// void hid_task()
// {
//     const uint32_t interval_ms = 10;
//     static uint32_t start_ms = 0;

//     if (board_millis() - start_ms < interval_ms) return;
//     start_ms += interval_ms;

//     // uint32_t const btn = board_button_read();
//     bool buttons[2] = {0};
//     buttons[0] = isVolumeUp();
//     buttons[1] = isVolumeDown();

//     bool anyButton = (buttons[0] || buttons[1]);

//     if (tud_suspended() && anyButton)
//     {
//         tud_remote_wakeup();
//     }

//     if (tud_hid_n_ready(ITF_KEYBOARD))
//     {
//         static bool has_key = false;

//         if (anyButton)
//         {
//             uint8_t keycode[6] = {0};
//             if (buttons[0])
//             {
//                 // keycode[0] = HID_KEY_VOLUME_UP;
//                 keycode[0] = HID_KEY_U;
//                 // keycode[1] = HID_KEY_P;
//                 // keycode[2] = HID_KEY_SPACE;
//             }
//             else if (buttons[1])
//             {

//                 keycode[0] = HID_KEY_D;
//                 // keycode[1] = HID_KEY_O;
//                 // keycode[2] = HID_KEY_W;
//                 // keycode[3] = HID_KEY_N;
//                 // keycode[4] = HID_KEY_SPACE;
//                 // keycode[0] = HID_KEY_VOLUME_DOWN;
//             }

//             tud_hid_n_keyboard_report(ITF_KEYBOARD, 0, 0, keycode);

//             has_key = true;
//         }
//         else
//         {
//             if (has_key)
//             {
//                 tud_hid_n_keyboard_report(ITF_KEYBOARD, 0, 0, NULL);
//                 has_key = false;
//             }
//         }
//     }
//     // if (tud_hid_n_ready(ITF_MOUSE))
//     // {
//     //     if (btn)
//     //     {
//     //         uint8_t const delta = 5;

//     //         tud_hid_n_mouse_report(ITF_MOUSE, 0, 0, delta, delta, 0, 0);
//     //     }
//     // }
// }


// // Invoked when received GET_REPORT control request
// // Application must fill buffer report's content and return its length.
// // Return zero will cause the stack to STALL request
// uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
// {
//   // TODO not Implemented
//   (void) itf;
//   (void) report_id;
//   (void) report_type;
//   (void) buffer;
//   (void) reqlen;

//   return 0;
// }

// // Invoked when received SET_REPORT control request or
// // received data on OUT endpoint ( Report ID = 0, Type = 0 )
// void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
// {
// //   TODO set LED based on CAPLOCK, NUMLOCK etc...
//     (void) itf;

//     // if (report_type == HID_REPORT_TYPE_OUTPUT)
//     // {
//     //     if (report_id == REPORT_ID_KEYBOARD)
//     //     {
//     //         if (buf)
//     //     }
//     // }
// }

// Different numbers than the audio.c host application.
// To fit in one byte.

static uint8_t writeBuffer[INTERRUPT_EP_SIZE] = {0};
void vendor_task()
{
    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;

    if (tusb_time_millis_api() - start_ms < interval_ms) return;
    start_ms += interval_ms;

    if (!tud_vendor_mounted())
    {
        return;
    }

    static int nPrinted = 0;
    bool result = tud_vendor_n_int_read_xfer(0);
    if (!result && nPrinted++ < 5)
    {
        printf("!tud_vendor_n_int_read_xfer(0);\n");
    }
    else if (result)
    {
        nPrinted = 0;
    }

    if (writeRequested)
    {
        return;
    }

    if (tud_vendor_n_int_write_available(0))
    {

        bool volUp = isVolumeUp();
        bool volDown = isVolumeDown();
        if (volUp)
        {
            writeBuffer[0] = ACTION_INC1;
        }
        else if (volDown)
        {
            writeBuffer[0] = ACTION_DEC1;
        }
        else
        {
            return;
        }

        tud_vendor_n_int_write(0, writeBuffer, INTERRUPT_EP_SIZE);
        // tud_vendor_write_flush();
        writeRequested = 1;
        printf("Packet sent\n");
    }
    else
    {
        printf("Not available\n");
    }
}

void led_blinking_task()
{
    static uint32_t start_ms = 0;
    static bool led_state = false;

    if (!blink_interval_ms) return;

    if (tusb_time_millis_api() - start_ms < blink_interval_ms) return;
    start_ms += blink_interval_ms;

    gpio_put(USB_STATUS_LED, led_state);
    led_state = !led_state;
}
