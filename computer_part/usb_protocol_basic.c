
#include <stdio.h>
#include <string.h>

#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
// #include <libusb.h>
#include <err.h>

#define MFGR_ID 0 // given manufacturer ID 
#define DEV_ID 0  // given device ID

/* If device IDs are not known, use libusb_get_device_list() to see a 
list of all USB devices connected to the machine. Follow this call with    
libusb_free_device_list() to free the allocated device list memory.
*/


int main() {
    int init = libusb_init(NULL); // NULL is the default libusb_context
    int err = 0;

    if (init < 0) {
        errx(1,"\n\nERROR: Cannot Initialize libusb\n\n");  
    }

    libusb_device** devices;
    libusb_device* pico = NULL;
    ssize_t devicesNum = libusb_get_device_list(NULL, &devices);
    struct libusb_device_descriptor lastDescriptor;

    for (int i = 0; i < devicesNum - 1; i++)
    {
        libusb_get_device_descriptor(devices[i], &lastDescriptor);

        printf("Device class: %x, vendor: %x\n", lastDescriptor.bDeviceClass, lastDescriptor.idVendor);
        if (lastDescriptor.idVendor == 0xcafe)
        {
            printf("Jes pi!!\n");
            pico = devices[i];
            break;
        }
    }

    if (pico)
    {
        libusb_device_handle* handle;
        err = libusb_open(pico, &handle);
        if (err)
        {
            errx(err, "err open");
            return err;
        }
        struct libusb_config_descriptor* activeConfig;
        err = libusb_get_active_config_descriptor(pico, &activeConfig);
        if (err)
        {
            errx(err, "libusb_get_active_config_descriptor");
            return err;
        }

        printf("activeConfig->bNumInterfaces: %d\n", activeConfig->bNumInterfaces);
        // printf("bNumConfigurations: %d\n", lastDescriptor.bNumConfigurations);
        // for (int i = 0; i < lastDescriptor.bNumConfigurations; i++)
        // {
        //     printf("Kernel driver active on conf[%d]: %d\n", 
        //         libusb_kernel_driver_active(handle, i)
        //     );
        // }

        libusb_close(handle);
        // Get some data
        // n interfaces
        // the strings etc.
    }
    else
    {
        printf("Nie otwieramy?\n");
    }

    libusb_free_device_list(devices, 1);

    // struct libusb_device_handle *dh = NULL; // The device handle
    // dh = libusb_open_device_with_vid_pid(NULL,MFGR_ID,DEV_ID);

    // if (!dh) {
    //     errx(1,"\n\nERROR: Cannot connect to device %d\n\n",DEV_ID);
    // }

    // // set fields for the setup packet as needed              
    // uint8_t       bmReqType = 0;   // the request type (direction of transfer)
    // uint8_t            bReq = 0;   // the request field for this packet
    // uint16_t           wVal = 0;   // the value field for this packet
    // uint16_t         wIndex = 0;   // the index field for this packet
    // unsigned char*   data = NULL;   // the data buffer for the in/output data
    // uint16_t           wLen = 0;   // length of this setup packet 
    // unsigned int     to = 0;       // timeout duration (if transfer fails)

    // // transfer the setup packet to the USB device
    // int config =     
    // libusb_control_transfer(dh,bmReqType,bReq,wVal,wIndex,data,wLen,to);

    // if (config < 0) {
    //     errx(1,"\n\nERROR: No data transmitted to device %d\n\n",DEV_ID);
    // }

    // // now you can use libusb_bulk_transfer to send raw data to the device

    libusb_exit(NULL);
}