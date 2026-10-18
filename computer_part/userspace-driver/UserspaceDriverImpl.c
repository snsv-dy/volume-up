#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h> 

#include "device_interface.h"
#include <usb_format.h>

#include "UserSpaceDriverImpl.h"

#ifdef UNIT_TESTING

int example_main(int argc, char *argv[]);
#define main example_main

#define static 

#endif

// 1. Read & write threads
// 2. profit
// 3. implement writing from the device interface function.
// 4. error handling
// 5. gracefull close (even on failing to open udev file. Currently it's a segfault)
// x. testing.

void shutdownThreads(ProgramState* programState)
{
    if (programState->readThreadRunning)
    {
        close(programState->readFile);
        pthread_join(programState->readThread, NULL);
    }
    
    if (programState->writeThreadRunning)
    {
        close(programState->writeFile);
        pthread_join(programState->writeThread, NULL);
    }
}

#define DEVPATH_STATIC_LEN 128
static char devpath[DEVPATH_STATIC_LEN] = "testfifo";

void *readThread(void* arg)
{
    ProgramState* programState = (ProgramState*)arg;
    uint8_t action = ACTION_INCORRECT;
    while (programState->readThreadRunning)
    {
        // size_t count = fread(&action, 1, 1, file);
        ssize_t count = read(programState->readFile, &action, 1);
        if (count > 0)
        {
            if (programState->callback)
            {
                // Pass value from device to the rest of the program.
                programState->callback(action, programState->callbackData);
            }
            printf("Read %x\n", action);
        }
        else if (count < 0)
        {
            perror("read");
            printf("size: %i\n", count);
            // TODO: signal the write thread and close.
            // running = 0;??
            break;
        }
        else
        {
            printf("[read] eof\n");
            break;
        }
    }

    programState->readThreadRunning = false;
    close(programState->readFile);
    shutdownThreads(programState);
    printf("[read] thread end\n");
    // 
    // TODO: try recover, or do this through driverInit by the upper layer.
    //       We don't recover, udev will start the driver when device is connected again.
}

void *writeThread(void* arg)
{
    ProgramState* programState = (ProgramState*)arg;
    sem_post(programState->deviceReady);

    while (programState->writeThreadRunning)
    {
        pthread_mutex_lock(&programState->writeWaitMut);
        while (programState->writeThreadRunning && !programState->writeReady)
        {
            pthread_cond_wait(&programState->writeWaitCond, &programState->writeWaitMut);
        }

        if (!programState->writeThreadRunning)
        {
            break;
            // exit
        }

        ssize_t count = 0;
        bool weBail = false;
        while (count < 2)
        {
            ssize_t written = write(programState->writeFile, programState->writeBuffer + count, 2 - count);
            if (written <= 0) // 0 (eof) or -1 (error)
            {
                perror("write");
                weBail = true;
                break;
            }
            count += written;
        }
        if (weBail)
        {
            break;
        }

        programState->writeReady = 0;
        pthread_mutex_unlock(&programState->writeWaitMut);
    }
    programState->writeThreadRunning = false;
    close(programState->writeFile);
    pthread_mutex_unlock(&programState->writeWaitMut);
    shutdownThreads(programState);
}

static ProgramState programState = {0};

int driverInit(ActionCallback callback, void* userData, sem_t* closing, sem_t* deviceReady)
{
    programState.readFile = open(devpath, O_RDONLY | O_CLOEXEC);
    if (programState.readFile == -1)
    {
        perror("[init] fopen /dev/volumeknob");
        return errno;
    }

    programState.writeFile = open(devpath, O_RDONLY | O_CLOEXEC);
    if (programState.writeFile == -1)
    {
        // Would be strange to arrive here.
        close(programState.readFile);
        perror("[init] fopen /dev/volumeknob");
        return errno;
    }
    // char devpath[128] = "/dev/volumeknob";
    // char devpath[128] = "testfifo";

    programState.writeThreadRunning = true;
    programState.readThreadRunning = true;
    programState.closing = closing;
    programState.deviceReady = deviceReady;
    programState.callback = callback;
    programState.callbackData = userData;

    if (
            pthread_mutex_init(&programState.writeWaitMut, NULL)
        ||  pthread_cond_init(&programState.writeWaitCond, NULL)
    )
    {
        perror("itc cond");
        close(programState.readFile);
        close(programState.writeFile);
        return errno;
    }

    if (pthread_create(&programState.readThread, NULL, readThread, (void*)&programState) != 0)
    {
        perror("Failed to create input thread");
        programState.writeThreadRunning = false;
        close(programState.readFile);
        close(programState.writeFile);
        return errno;
    }

    if (pthread_create(&programState.writeThread, NULL, writeThread, (void*)&programState) != 0)
    {
        perror("Failed to create output thread");
        programState.readThreadRunning = false;
        close(programState.readFile);
        close(programState.writeFile);
        pthread_join(programState.readThread, NULL);
        return errno;
    }

    if (deviceReady != NULL)
    {
        sem_post(deviceReady);
    }


    if (closing)
    {
        sem_wait(closing);
    }
    else
    {
        // TODO: runtime error. closing should be passed by audio.c
        while (programState.writeThreadRunning && programState.readThreadRunning)
        {
            sleep(1);
            // static uint8_t volume = 0;
            // volumeChanged(volume);
            // volume++;
        }
    }

    shutdownThreads(&programState);

    // pthread_join(programState.readThread, NULL);
    // pthread_join(programState.writeThread, NULL);

    // TODO: Move to driverInit when he will be blocking.
    // TODO: Keep track if file was closed??? 
    // double close only results in EBADF errno.
    // close(programState.readFile);
    // close(programState.writeFile);

    return 0;
}

void volumeChanged(uint8_t volumePercent)
{
    pthread_mutex_lock(&programState.writeWaitMut);
    programState.writeBuffer[0] = ACTION_GET_VOLUME;
    programState.writeBuffer[1] = volumePercent;
    programState.writeReady = 1;
    pthread_cond_broadcast(&programState.writeWaitCond);
    pthread_mutex_unlock(&programState.writeWaitMut);
}


void interruptSignalHandler()
{
    printf("[ctrl+c] Received\n");
    // Zamykanie wątków komunikacji z urządzeniem.
    // programState.running = 0;
    shutdownThreads(&programState);
    sem_post(programState.closing);
    
    printf("[ctrl+c] Finished\n");
}

void initInterruptSignal()
{
    struct sigaction sa;
    sa.sa_handler = interruptSignalHandler;  
    sa.sa_flags = SA_RESTART;  
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL))
    {
        printf("Failed to set SIGINT action\n");
    }
}

void printingCallback(uint32_t action, void* userData)
{
    printf("Received command from dev: ");
    switch (action)
    {
        case ACTION_INC5: printf("ACTION_INC5"); break;
        case ACTION_DEC5: printf("ACTION_DEC5"); break;
        case ACTION_INC1: printf("ACTION_INC1"); break;
        case ACTION_DEC1: printf("ACTION_DEC1"); break;
        case ACTION_SET24: printf("ACTION_SET24"); break;
        case ACTION_SET29: printf("ACTION_SET29"); break;
        default:
            printf("default");
    }
    printf("\n");
}


// Main tylko do testowania w sumie, bo finalnie to będzie używane jako biblioteka.
int main(int argc, char **argv)
{
    initInterruptSignal();

    sem_t waitForDeviceThread;
    sem_t closing;
    if (
           sem_init(&closing, 0, 0)
        || sem_init(&waitForDeviceThread, 0, 0)
    )
    {
        printf("Failed to initialize itc\n");
        // exit here;
        return 1;
    }

    if (argc > 1)
    {
        size_t devpathLen = strlen(argv[1]);
        if (devpathLen > DEVPATH_STATIC_LEN - 1)
        {
            printf("Device path '%s' is too long (co ty kombinujesz z taką długą nazwą, co?)\n");
            exit(1);
        }

        memcpy(devpath, argv[1], devpathLen + 1);
    }
    
    if (driverInit(printingCallback, NULL, &closing, &waitForDeviceThread))
    {
        printf("Driver init aint worked\n");
    }
    else
    {
        printf("Waiting for initialization.\n");
        sem_wait(&waitForDeviceThread);
        printf("Device connected.\n");
    }
    sem_destroy(&waitForDeviceThread);


    return 0;
}