
#include <stdio.h>
#include <string.h>

#include <libusb-1.0/libusb.h>
#include <pthread.h>
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
#define DATA_LEN 8
#define INTERRUPT_TIMEOUT_MS 500
#define KEYBOARD_INTERFACE 0

typedef struct
{
    uint8_t eventThreadRun;
    pthread_t libusbThread;
    struct libusb_device_handle* deviceHandle;
    unsigned char keyboardEndpointAddress;

    char interruptData[DATA_LEN];
} ProgramState;

void keyboardInterrupt(struct libusb_transfer *transfer)
{
    ProgramState* programState = (ProgramState*)transfer->user_data;

    printf("Interrup callback\n"
            "Status: (0x%x) ", transfer->status);
        switch(transfer->status)
        {
            case LIBUSB_TRANSFER_COMPLETED: printf("LIBUSB_TRANSFER_COMPLETED\n"); break;
            case LIBUSB_TRANSFER_ERROR: printf("LIBUSB_TRANSFER_ERROR\n"); break;
            case LIBUSB_TRANSFER_TIMED_OUT: printf("LIBUSB_TRANSFER_TIMED_OUT\n"); break;
            case LIBUSB_TRANSFER_CANCELLED: printf("LIBUSB_TRANSFER_CANCELLED\n"); break;
            case LIBUSB_TRANSFER_STALL: printf("LIBUSB_TRANSFER_STALL\n"); break;
            case LIBUSB_TRANSFER_NO_DEVICE: printf("LIBUSB_TRANSFER_NO_DEVICE\n"); break;
            case LIBUSB_TRANSFER_OVERFLOW: printf("LIBUSB_TRANSFER_OVERFLOW\n"); break;
            default: printf("idk lol\n"); break;
        }

        if (transfer->status == LIBUSB_TRANSFER_COMPLETED)
        {
            printf("transfer data: ");
            for (int i = 0; i < DATA_LEN; i++)
            {
                printf("%c", transfer->buffer[i]);
            }
            printf(" (");
            for (int i = 0; i < DATA_LEN; i++)
            {
                printf("0x%x, ", transfer->buffer[i]);
            }
            printf(")\n");
        }

        if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE || transfer->status == LIBUSB_TRANSFER_ERROR)
        {
            libusb_close(transfer->user_data);
            programState->eventThreadRun = 0;
            return;
        }

        libusb_fill_interrupt_transfer(transfer, programState->deviceHandle, programState->keyboardEndpointAddress, programState->interruptData, DATA_LEN, keyboardInterrupt, programState, INTERRUPT_TIMEOUT_MS);
        int err = libusb_submit_transfer(transfer);
        if (err)
        {
            errx(err, "libusb_submit_transfer error: %s\n", libusb_error_name(err));
            programState->eventThreadRun = 0;
            libusb_close(transfer->user_data);
        }

        // printf("Next transfer submitted\n");
}


void* libusbMainLoop(void* arg)
{
    ProgramState* programState = (ProgramState*)arg;
    
    while (programState->eventThreadRun)
    {
        libusb_handle_events(NULL);
    }

    return NULL;
}

int main() {
    int init = libusb_init(NULL); // NULL is the default libusb_context
    int err = 0;

    if (init < 0) {
        errx(1,"\n\nERROR: Cannot Initialize libusb\n\n");  
    }

    //
    //  Initialization and gathering information.
    //
    ProgramState programState = {0};

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
        struct libusb_config_descriptor* activeConfig;
        err = libusb_get_active_config_descriptor(pico, &activeConfig);
        if (err)
        {
            errx(err, "libusb_get_active_config_descriptor");
            return err;
        }

        printf("activeConfig->bNumInterfaces: %d\n", activeConfig->bNumInterfaces);
        for (int i = 0; i < activeConfig->bNumInterfaces; i++)
        {
            struct libusb_interface* interface = (struct libusb_interface*)&activeConfig->interface[i];

            // struct libusb_interface_descriptor* interfaceDescriptor = (struct libusb_interface_descriptor*)&activeConfig->interface[i];
            printf("Interface %d(%d): \n", i, 0);
            printf("num_altsetting: %d\n", interface->num_altsetting);
            if (interface->num_altsetting == 0)
            {
                continue;
            }
            const struct libusb_interface_descriptor* interfaceDescriptor = &interface->altsetting[0];
            
            
            printf("bLength: 0x%x\n", interfaceDescriptor->bLength);
            printf("bDescriptorType: 0x%x\n", interfaceDescriptor->bDescriptorType);
            printf("bInterfaceNumber: 0x%x\n", interfaceDescriptor->bInterfaceNumber);
            printf("bAlternateSetting: 0x%x\n", interfaceDescriptor->bAlternateSetting);
            printf("bNumEndpoints: %d\n", interfaceDescriptor->bNumEndpoints);
            printf("bInterfaceClass: 0x%x\n", interfaceDescriptor->bInterfaceClass);
            printf("bInterfaceSubClass: 0x%x\n", interfaceDescriptor->bInterfaceSubClass);
            printf("bInterfaceProtocol: 0x%x\n", interfaceDescriptor->bInterfaceProtocol);
            printf("iInterface: 0x%x\n", interfaceDescriptor->iInterface);
            printf("extra_length: %u\n", interfaceDescriptor->extra_length);
            printf("--------------\n");
            for (int j = 0; j < interfaceDescriptor->bNumEndpoints; j++)
            {
                const struct libusb_endpoint_descriptor *endpoint = (const struct libusb_endpoint_descriptor *)interfaceDescriptor->endpoint;
                printf("Endpoint %d\n", j);
                printf("bLength: 0x%x\n", endpoint->bLength);
                printf("bDescriptorType: 0x%x\n", endpoint->bDescriptorType);
                printf("bEndpointAddress: 0x%x\n", endpoint->bEndpointAddress);
                printf("bmAttributes: 0x%x\n", endpoint->bmAttributes);
                printf("wMaxPacketSize: %d\n", endpoint->wMaxPacketSize);
                printf("bInterval: %d\n", endpoint->bInterval);
                printf("bRefresh: 0x%x\n", endpoint->bRefresh);
                printf("bSynchAddress: 0x%x\n", endpoint->bSynchAddress);
                printf("--------------\n");

                if (i == 0 && j == 0)
                {
                    programState.keyboardEndpointAddress = endpoint->bEndpointAddress;
                }
            }
            printf("==============\n");
        }

        libusb_device_handle* handle;
        err = libusb_open(pico, &handle);
        if (err)
        {
            errx(err, "err open: %s\n", libusb_error_name(err));
            return err;
        }
        programState.deviceHandle = handle;
        err = libusb_set_auto_detach_kernel_driver(handle, 1);
        if (err)
        {
            errx(err, "auto detach error: %s\n", libusb_error_name(err));
            return err;
        }

        err = libusb_claim_interface(handle, KEYBOARD_INTERFACE);
        if (err)
        {
            errx(err, "auto detach error: %s\n", libusb_error_name(err));
        }

        /*
        [x]    1. libusb w osobnym wątku.
               2. callback w przypadku odebrania danych (może być null)
        [1/2x] 3. czekanie na zakończenie przez użytkownika w mainie (wraz z zamknięciem struktur libusba)
               3.1 Nie zamyka się poprawnie jeszcze.
               4. Własny endpoint/interface do odbierania
               5. I wysyłania danych? (np. aktualna głośność systemowa)

        */
          

        struct libusb_transfer* transfer = libusb_alloc_transfer(0);
        libusb_fill_interrupt_transfer(transfer, handle, programState.keyboardEndpointAddress, programState.interruptData, DATA_LEN, keyboardInterrupt, &programState, 500);
        err = libusb_submit_transfer(transfer);
        if (err)
        {
            errx(err, "auto detach error: %s\n", libusb_error_name(err));
        }

        //
        // Communication with the device.
        //
        programState.eventThreadRun = 1;
        if (pthread_create(&programState.libusbThread, NULL, libusbMainLoop, (void*)&programState) != 0)
        {
            printf("Failed to create input thread\n");
        }

        printf("Any key to close application\n");
        char theGuy = getchar();
        printf ("[Char read] '%c'\n", theGuy);
        if (programState.eventThreadRun)
        {
            programState.eventThreadRun = 0;
            libusb_close(programState.deviceHandle);
        }

        pthread_join(programState.libusbThread, NULL);
        

        // Tu pętla była

        //
        // Cleaning up.
        //

        libusb_free_transfer(transfer);

        err = libusb_release_interface(handle, 0);
        if (err)
        {
            errx(err, "auto detach error: %s\n", libusb_error_name(err));
        }
        libusb_free_config_descriptor(activeConfig);

        // close in callback.
        // libusb_close(handle);
    }
    else
    {
        printf("Nie otwieramy?\n");
    }

    libusb_free_device_list(devices, 1);

    libusb_exit(NULL);
}