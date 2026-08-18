
#include <stdio.h>
#include <string.h>

#include <libusb-1.0/libusb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
// #include <libusb.h>
#include <err.h>
#include <usb_format.h>
#include <device_interface.h>

#define MFGR_ID 0 // given manufacturer ID 
#define DEV_ID 0  // given device ID

/* If device IDs are not known, use libusb_get_device_list() to see a 
list of all USB devices connected to the machine. Follow this call with    
libusb_free_device_list() to free the allocated device list memory.
*/
#define DATA_LEN 8
#define INTERRUPT_TIMEOUT_MS 2000
#define KEYBOARD_INTERFACE 0

typedef struct
{
    uint8_t eventThreadRun;
    pthread_mutex_t mutexEventThreadRun;

    pthread_t libusbThread;
    struct libusb_device_handle* deviceHandle;
    
    unsigned char inEndpointAddress;
    struct libusb_transfer *inTransfer;
    char inBuffer[DATA_LEN];

    unsigned char outEndpointAddress;
    struct libusb_transfer *outTransfer;
    char outBuffer[DATA_LEN];

    // TODO: Spróbuj napisać na to test?
    uint8_t nTransfersInProgress;

    void* userData;
    ActionCallback actionCallback;
} ProgramState;

// TODO: ehhh, a da się bez globalnych zmiennych?
// btw. globalne żeby funkcje z device_interface.h miały dostęp do tej struktury.
static ProgramState* __programState = NULL;

void* libusbMainLoop(void* arg);
void initLibusbThread(ProgramState* programState)
{
    pthread_mutex_init(&programState->mutexEventThreadRun, NULL);

    programState->eventThreadRun = 1;
    if (pthread_create(&programState->libusbThread, NULL, libusbMainLoop, (void*)programState) != 0)
    {
        printf("Failed to create input thread\n");
    }
}

void cleanLibusbThread(ProgramState* programState)
{
    pthread_join(programState->libusbThread, NULL);
    pthread_mutex_destroy(&programState->mutexEventThreadRun);
}

void setEventThreadRun(ProgramState* programState, const uint8_t value)
{
    printf("[setEventThreadRun] entering mutex\n");
    pthread_mutex_lock(&programState->mutexEventThreadRun);
    printf("[setEventThreadRun] mutex enter\n");
    
    programState->eventThreadRun = value;
    printf("[setEventThreadRun] programState->eventThreadRun = value; done\n");

    pthread_mutex_unlock(&programState->mutexEventThreadRun);
    printf("[setEventThreadRun] mutex exit\n");
}

void closeDeviceAndStop(ProgramState* programState)
{
    printf("[closeDeviceAndStop] entering mutex\n");
    pthread_mutex_lock(&programState->mutexEventThreadRun);
    printf("[closeDeviceAndStop] mutex enter\n");

    libusb_close(programState->deviceHandle);
    programState->eventThreadRun = 0;
    printf("[closeDeviceAndStop] libusb_close done\n");

    pthread_mutex_unlock(&programState->mutexEventThreadRun);
    printf("[closeDeviceAndStop] mutex exit\n");
}

void inTransferCallback(struct libusb_transfer *transfer)
{
    ProgramState* programState = (ProgramState*)transfer->user_data;
    // programState->transferSubmitted = 0; // No mutex to be as fast as fuck.
    programState->nTransfersInProgress--;

    printf("inTransferCallback "
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

            const uint8_t action = transfer->buffer[0];
            switch(action)
            {
                case ACTION_INC5: printf("Received +5%% action\n"); break;
                case ACTION_DEC5: printf("Received -5%% action\n"); break;
                default: printf("default action\n");
            }

            programState->actionCallback(action, programState->userData);
        }

        if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE 
            || transfer->status == LIBUSB_TRANSFER_ERROR
            || transfer->status == LIBUSB_TRANSFER_CANCELLED)
        {
            // We can't close handle here since it will stop the libusb loop immediately.
            // closeDeviceAndStop(programState);
            return;
        }

        libusb_fill_interrupt_transfer(transfer, programState->deviceHandle, programState->inEndpointAddress, programState->inBuffer, DATA_LEN, inTransferCallback, programState, INTERRUPT_TIMEOUT_MS);
        int err = libusb_submit_transfer(transfer);
        if (err)
        {
            errx(err, "libusb_submit_transfer error: %s\n", libusb_error_name(err));
            // We can't close handle here since it will stop the libusb loop immediately.
            // closeDeviceAndStop(programState);
        }
        programState->nTransfersInProgress++;

        // static uint8_t dumVolume = 0;
        // volumeChanged(dumVolume++);

        // printf("Next transfer submitted\n");
}


void outTransferCallback(struct libusb_transfer *transfer)
{
    ProgramState* programState = (ProgramState*)transfer->user_data;
    programState->nTransfersInProgress--;
    // programState->transferSubmitted = 0; // No mutex to be as fast as fuck.

    printf("outTransferCallback "
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
            printf("Out transfer completed\n");
        }

        if (transfer->status == LIBUSB_TRANSFER_STALL)
        {
            printf("Stall\n");
            // TODO: Retry?
        }

        if (transfer->status == LIBUSB_TRANSFER_NO_DEVICE 
            || transfer->status == LIBUSB_TRANSFER_ERROR
            || transfer->status == LIBUSB_TRANSFER_CANCELLED)
        {
            // We can't close handle here since it will stop the libusb loop immediately.
            // closeDeviceAndStop(programState);
            return;
        }
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

#ifdef STANDALONE_USB
int main() {
    driverInit();
}
#endif

int initEndpointAdresses(ProgramState* programState, struct libusb_config_descriptor* activeConfig)
{
    int nFound = 0;
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
        
        
        // printf("bLength: 0x%x\n", interfaceDescriptor->bLength);
        // printf("bDescriptorType: 0x%x\n", interfaceDescriptor->bDescriptorType);
        // printf("bInterfaceNumber: 0x%x\n", interfaceDescriptor->bInterfaceNumber);
        // printf("bAlternateSetting: 0x%x\n", interfaceDescriptor->bAlternateSetting);
        // printf("bNumEndpoints: %d\n", interfaceDescriptor->bNumEndpoints);
        // printf("bInterfaceClass: 0x%x\n", interfaceDescriptor->bInterfaceClass);
        // printf("bInterfaceSubClass: 0x%x\n", interfaceDescriptor->bInterfaceSubClass);
        // printf("bInterfaceProtocol: 0x%x\n", interfaceDescriptor->bInterfaceProtocol);
        // printf("iInterface: 0x%x\n", interfaceDescriptor->iInterface);
        // printf("extra_length: %u\n", interfaceDescriptor->extra_length);
        // printf("--------------\n");
        for (int j = 0; j < interfaceDescriptor->bNumEndpoints; j++)
        {
            const struct libusb_endpoint_descriptor *endpoint = (const struct libusb_endpoint_descriptor *)&interfaceDescriptor->endpoint[j];
            // printf("Endpoint %d\n", j);
            // printf("bLength: 0x%x\n", endpoint->bLength);
            // printf("bDescriptorType: 0x%x\n", endpoint->bDescriptorType);
            // printf("bEndpointAddress: 0x%x\n", endpoint->bEndpointAddress);
            // printf("bmAttributes: 0x%x\n", endpoint->bmAttributes);
            // printf("wMaxPacketSize: %d\n", endpoint->wMaxPacketSize);
            // printf("bInterval: %d\n", endpoint->bInterval);
            // printf("bRefresh: 0x%x\n", endpoint->bRefresh);
            // printf("bSynchAddress: 0x%x\n", endpoint->bSynchAddress);
            // printf("--------------\n");

            if (i == 0 && (endpoint->bEndpointAddress & 0x80)) // The in endpoint.
            {
                programState->inEndpointAddress = endpoint->bEndpointAddress;
                nFound++;
            }
            else if ( j < 2)// out endpoint
            {
                programState->outEndpointAddress = endpoint->bEndpointAddress;
                nFound++;
            }
        }
        // printf("==============\n");
    }

    return nFound;
}

int driverInit(ActionCallback callback, void* userData, sem_t* closing)
{
    if (!callback || !userData || !closing)
    {
        return 1;
    }

    int init = libusb_init(NULL); // NULL is the default libusb_context
    int err = 0;

    if (init < 0) {
        errx(1,"\n\nERROR: Cannot Initialize libusb\n\n");  
    }

    //
    //  Initialization and gathering information.
    //
    ProgramState programState = {0};
    __programState = &programState;
    programState.userData = userData;
    programState.actionCallback = callback;

    libusb_device** devices;
    libusb_device* pico = NULL;
    ssize_t devicesNum = libusb_get_device_list(NULL, &devices);
    struct libusb_device_descriptor lastDescriptor;

    for (int i = 0; i < devicesNum - 1; i++)
    {
        libusb_get_device_descriptor(devices[i], &lastDescriptor);

        // printf("Device class: %x, vendor: %x\n", lastDescriptor.bDeviceClass, lastDescriptor.idVendor);
        if (lastDescriptor.idVendor == 0xcafe)
        {
            printf("Pi znalezione!!!\n");
            pico = devices[i];
            break;
        }
    }

    if (pico)
    {
        // Ref device, żeby nie zostało usunięte przez libusb_free_device_list.
        // Można by też pierw otworzyć, a potem zwolnić listę, ale na razie to wydaje mi się schludniejsze.
        libusb_ref_device(pico);
    }

    printf("libusb_free_device_list\n");
    libusb_free_device_list(devices, 1);

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
        initEndpointAdresses(&programState, activeConfig);
        

        libusb_device_handle* handle;
        err = libusb_open(pico, &handle);
        if (err)
        {
            printf("err libusb_open: %s\n", libusb_error_name(err));
            libusb_unref_device(pico);
            return err;
        }
        programState.deviceHandle = handle;
        err = libusb_set_auto_detach_kernel_driver(handle, 1);
        if (err)
        {
            printf("libusb_set_auto_detach_kernel_driver error: %s\n", libusb_error_name(err));
            libusb_close(pico);
            libusb_unref_device(pico);
            return err;
        }

        err = libusb_claim_interface(handle, KEYBOARD_INTERFACE);
        if (err)
        {
            printf("libusb_claim_interface error: %s\n", libusb_error_name(err));
            libusb_close(pico);
            libusb_unref_device(pico);
        }

        /*
        [x]    1. libusb w osobnym wątku.
               2. callback w przypadku odebrania danych (może być null)
        [1/2x] 3. czekanie na zakończenie przez użytkownika w mainie (wraz z zamknięciem struktur libusba)
               3.1 Nie zamyka się poprawnie jeszcze.
               4. Własny endpoint/interface do odbierania
               \/ -- To zrób, żeby sprawdzić czy urządzenie cokolwiek odbiera.
               5. I wysyłania danych? (np. aktualna głośność systemowa)

        */
          

        programState.inTransfer = libusb_alloc_transfer(0);
        programState.outTransfer = libusb_alloc_transfer(0);

        if (!programState.inTransfer || !programState.outTransfer)
        {
            printf("!programState.inTransfer || !programState.outTransfer\n");
            err = libusb_release_interface(handle, KEYBOARD_INTERFACE);
            if (err)
            {
                printf(err, "auto detach error: %s\n", libusb_error_name(err));
            }
            libusb_close(pico);
            libusb_unref_device(pico);
            return -1;
        }


        // volumeChanged(12);

        libusb_fill_interrupt_transfer(
            programState.inTransfer, 
            handle, 
            programState.inEndpointAddress, 
            programState.inBuffer, 
            DATA_LEN, 
            inTransferCallback, 
            &programState, 
            INTERRUPT_TIMEOUT_MS);

        err = libusb_submit_transfer(programState.inTransfer);
        if (err)
        {
            errx(err, "libusb_submit_transfer error: %s\n", libusb_error_name(err));
        }
        // programState.transferSubmitted = 1;

        //
        // Communication with the device.
        //
        initLibusbThread(&programState);

        printf("Any key to close application\n");
        // char theGuy = getchar();
        sem_wait(closing);
        printf ("[sem posted]\n");

        // printf ("[Char read] '%c'\n", theGuy);

        libusb_cancel_transfer(programState.inTransfer);
        libusb_cancel_transfer(programState.outTransfer);
        
        while (programState.nTransfersInProgress)
        {
            // Timeout for cancellation to execute.
            usleep(1000 * 100);
        }
        // If still not shut down after 100ms do it here. 
        // (the other thread had whole 100ms to do its job and still failed, lazy ass mf, smh)
        // printf("[main] entering mutex\n");
        pthread_mutex_lock(&programState.mutexEventThreadRun);
        // printf("[main] mutex enter\n");
        if (programState.eventThreadRun)
        {
            programState.eventThreadRun = 0;
            // Wake up the libusb thread.

            printf("libusb_release_interface\n");
            // Release interface before closing the handle.
            err = libusb_release_interface(handle, KEYBOARD_INTERFACE);
            if (err)
            {
                errx(err, "auto detach error: %s\n", libusb_error_name(err));
            }
            libusb_close(programState.deviceHandle);
        }
        pthread_mutex_unlock(&programState.mutexEventThreadRun);
        // printf("[main] mutex exit\n");

        // Wait for thread to join.
        cleanLibusbThread(&programState);
        // printf("[main] cleanLibusbThread exit\n");
        //
        // Cleaning up. libusb
        //

        printf("libusb_free_transfer\n");
        libusb_free_transfer(programState.inTransfer);
        libusb_free_transfer(programState.outTransfer);

        printf("libusb_free_config_descriptor\n");
        libusb_free_config_descriptor(activeConfig);

        // close in callback.
        // libusb_close(handle);
    }
    else
    {
        printf("Nie otwieramy?\n");
    }

    printf("libusb_exit\n");
    libusb_exit(NULL);

    return 1;
}

//
// Device interface implementation.

// int init();

void volumeChanged(uint8_t volumePercent)
{
    __programState->outBuffer[0] = ACTION_GET_VOLUME;
    __programState->outBuffer[1] = volumePercent;

    libusb_fill_interrupt_transfer(
        __programState->outTransfer, 
        __programState->deviceHandle, 
        __programState->outEndpointAddress, 
        __programState->outBuffer,
        DATA_LEN, 
        outTransferCallback, 
        __programState, 
        INTERRUPT_TIMEOUT_MS);

    int err = libusb_submit_transfer(__programState->outTransfer);
    if (err)
    {
        errx(err, "[out]libusb_submit_transfer: %s\n", libusb_error_name(err));
    }
    __programState->nTransfersInProgress++;
}
// void setActionCallback(ActionCallback callback);

void setActionCallback(ActionCallback, void* userData)
{

}

//
//