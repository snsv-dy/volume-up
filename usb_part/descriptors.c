#include "usb_part/usb_part.h"
#include "bsp/board_api.h"
// #include "bsp.h"
#include "tusb.h"
// #include "pico/stdlib.h"
// 1. Napisz deskryptor do interfejsu vendora
// 2. przenieś device desc tutaj i zrób, żeby się kompilowało
// 2.1 Obsługa po stronie libusb tego interfejsu.
// 3. Niech endpoint out wysyła hello world
// 4. Niech endpoint in printuje na uarcie to co przyszło.


#define USB_PID 0x4013

static tusb_desc_device_t const desc_device =
{
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,

    .bDeviceClass       = TUSB_CLASS_VENDOR_SPECIFIC,
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


enum
{
    ITF_NUM_VENDOR,
    ITF_NUM_TOTAL,
};



// #define EPNUM_HID1 0x81
// #define EPNUM_HID2 0x82
#define TUD_INTERRUPT_VENDOR_DESC_LEN  (9+7+7)

// Interface number, string index, EP Out & IN address, EP size
#define TUD_INTERRUPT_VENDOR_DESCRIPTOR(_itfnum, _stridx, _epout, _epin, _epsize, _ep_interval) \
/* Interface */\
9, TUSB_DESC_INTERFACE, _itfnum, 0, 2, TUSB_CLASS_VENDOR_SPECIFIC, 0x00, 0x00, _stridx,\
/* Endpoint Out */\
7, TUSB_DESC_ENDPOINT, _epout, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_epsize), _ep_interval,\
/* Endpoint In */\
7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_epsize), _ep_interval

//   7, TUSB_DESC_ENDPOINT, _epin, TUSB_XFER_INTERRUPT, U16_TO_U8S_LE(_epsize), _ep_interval


#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_INTERRUPT_VENDOR_DESC_LEN)

uint8_t const desc_configuration[] =
{
    // TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x80, 100),

    TUD_INTERRUPT_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, 0, INTERRUPT_EP_OUT, INTERRUPT_EP_IN, INTERRUPT_EP_SIZE, INTERRUPT_EP_INTERVAL),
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