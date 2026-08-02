#include "usb_part.h"
#include "bsp/board_api.h"
#include "bsp.h"
#include "tusb.h"
#include "pico/stdlib.h"

//https://github.com/hathach/tinyusb/blob/master/examples/device/hid_multiple_interface/src/usb_descriptors.c

#define USB_PID 0x4013

static tusb_desc_device_t const desc_device =
{
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,

    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0xCafe,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01,
};

uint8_t const * tud_descriptor_device_cb(void)
{
    return (uint8_t const *) &desc_device;
}

uint8_t const desc_hid_report1[] =
{
    TUD_HID_REPORT_DESC_KEYBOARD()
};

uint8_t const desc_hid_report2[] =
{
    TUD_HID_REPORT_DESC_MOUSE()
};

uint8_t const * tud_hid_descriptor_report_cb(uint8_t itf)
{
    if (itf == 0)
    {
        return desc_hid_report1;
    }
    else if (itf == 1)
    {
        return desc_hid_report2;
    }

    return NULL;
}

enum
{
    ITF_NUM_HID1,
    ITF_NUM_HID2,
    ITF_NUM_TOTAL,
};

#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN)

#define EPNUM_HID1 0x81
#define EPNUM_HID2 0x82

uint8_t const desc_configuration[] =
{
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    TUD_HID_DESCRIPTOR(ITF_NUM_HID1, 4, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report1), EPNUM_HID1, CFG_TUD_HID_EP_BUFSIZE, 10),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID2, 5, HID_ITF_PROTOCOL_NONE, sizeof(desc_hid_report2), EPNUM_HID2, CFG_TUD_HID_EP_BUFSIZE, 10)
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
    (void)index;
    return desc_configuration;
}

enum 
{
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
};


static char const * string_desc_arr[] = 
{
    (const char[]) {0x09, 0x04},
    "TinyUSB",
    "TinyUSB Device",
    NULL,
    "Keyboard Interface",
    "Mous Interface",
};

static uint16_t _desc_str[32 + 1];

uint16_t const * tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    (void)langid;
    size_t chr_count;

    switch(index)
    {
        case STRID_LANGID:
            memcpy(&_desc_str[1], string_desc_arr[0], 2);
            chr_count = 1;
            break;

        case STRID_SERIAL:
            chr_count = board_usb_get_serial(_desc_str + 1, 32);
            break;

        default:
            if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0]))) return NULL;

            const char * str = string_desc_arr[index];

            chr_count = strlen(str);
            size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1; // -1 for string type
            if ( chr_count > max_count ) chr_count = max_count;

            // Convert ASCII string into UTF-16
            for ( size_t i = 0; i < chr_count; i++ ) {
                _desc_str[1 + i] = str[i];
            }
        break;
    }

  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}

enum {
    ITF_KEYBOARD = 0,
    ITF_MOUSE = 1
};

// Blink pattern in ms.
enum {
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED = 1000,
    BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_blinking_task();
void hid_task();
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

void usbLoop()
{
    tud_task();
    led_blinking_task();

    hid_task();
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
//

void hid_task()
{
    const uint32_t interval_ms = 10;
    static uint32_t start_ms = 0;

    if (board_millis() - start_ms < interval_ms) return;
    start_ms += interval_ms;

    // uint32_t const btn = board_button_read();
    bool buttons[2] = {0};
    buttons[0] = isVolumeUp();
    buttons[1] = isVolumeDown();

    bool anyButton = (buttons[0] || buttons[1]);

    if (tud_suspended() && anyButton)
    {
        tud_remote_wakeup();
    }

    if (tud_hid_n_ready(ITF_KEYBOARD))
    {
        static bool has_key = false;

        if (anyButton)
        {
            uint8_t keycode[6] = {0};
            if (buttons[0])
            {
                // keycode[0] = HID_KEY_VOLUME_UP;
                keycode[0] = HID_KEY_U;
                // keycode[1] = HID_KEY_P;
                // keycode[2] = HID_KEY_SPACE;
            }
            else if (buttons[1])
            {

                keycode[0] = HID_KEY_D;
                // keycode[1] = HID_KEY_O;
                // keycode[2] = HID_KEY_W;
                // keycode[3] = HID_KEY_N;
                // keycode[4] = HID_KEY_SPACE;
                // keycode[0] = HID_KEY_VOLUME_DOWN;
            }

            tud_hid_n_keyboard_report(ITF_KEYBOARD, 0, 0, keycode);

            has_key = true;
        }
        else
        {
            if (has_key)
            {
                tud_hid_n_keyboard_report(ITF_KEYBOARD, 0, 0, NULL);
                has_key = false;
            }
        }
    }
    // if (tud_hid_n_ready(ITF_MOUSE))
    // {
    //     if (btn)
    //     {
    //         uint8_t const delta = 5;

    //         tud_hid_n_mouse_report(ITF_MOUSE, 0, 0, delta, delta, 0, 0);
    //     }
    // }
}


// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
//   TODO set LED based on CAPLOCK, NUMLOCK etc...
    (void) itf;

    // if (report_type == HID_REPORT_TYPE_OUTPUT)
    // {
    //     if (report_id == REPORT_ID_KEYBOARD)
    //     {
    //         if (buf)
    //     }
    // }
}

void led_blinking_task()
{
    static uint32_t start_ms = 0;
    static bool led_state = false;

    if (!blink_interval_ms) return;

    if (board_millis() - start_ms < blink_interval_ms) return;
    start_ms += blink_interval_ms;

    gpio_put(USB_STATUS_LED, led_state);
    led_state = !led_state;
}
