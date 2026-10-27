#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h> 
#include <semaphore.h>
#include <stdatomic.h>

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

static atomic_int threadInitializedN = 0;
void reportThreadUp()
{
    // if (threadInitializedN++ == 1)
    // {
    //     sem_post(programState.deviceReady);
    // }
}


// Włącza SIGUSR1 w wywołującym wątku.
void enableSigusr1()
{
    sigset_t set;
    int s;
    
    // Odblokowanie sygnału w wątku wykonującym operacje na pliku.
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    s = pthread_sigmask(SIG_UNBLOCK, &set, NULL);
    if (s != 0)
    {
        perror("pthread_sigmask");
    }
}

#define DEVPATH_STATIC_LEN 128
static char devpath[DEVPATH_STATIC_LEN] = "/dev/skel0";

void shutdownThreads(ProgramState* programState, bool fromRead, bool fromWrite)
{
    programState->running = false;
    programState->callback(ACTION_DEVICE_SHUTDOWN, programState->callbackData);

    if (!fromRead)
    {
        pthread_kill(programState->readThread, SIGUSR1);
    }

    if (!fromWrite)
    {
        pthread_mutex_lock(&programState->writeWaitMut);
        // Wywołaj wątek write. Jak zobaczy, że program nie jest running to się zakończy.
        pthread_cond_broadcast(&programState->writeWaitCond);
        pthread_mutex_unlock(&programState->writeWaitMut);
        // Na wszelki wypadek, jakby był w trakcie zapisywania czegoś idk.
        pthread_kill(programState->writeThread, SIGUSR1);
    }
}

void *readThread(void* arg)
{
    enableSigusr1();
    reportThreadUp();

    ProgramState* programState = (ProgramState*)arg;
    uint8_t action = ACTION_INCORRECT;
    ssize_t count = 0;
    bool killProgram = false;
    printf("[read] programState->running: %d\n", programState->running);
    while (programState->running)
    {
        // size_t count = fread(&action, 1, 1, file);
        count = read(programState->file, &action, 1);
        if (count > 0)
        {
            if (programState->callback)
            {
                // Pass value from device to the rest of the program.
                programState->callback(action, programState->callbackData);
            }
            printf("[read] action: %x\n", action);
        }
        else if (count < 0)
        {
            if (errno != EINTR)
            {
                perror("read");
                printf("[read] size: %i, errno: \n", count, errno);
                killProgram = true;
                break;
            }
        }
        else
        {
            printf("[read] eof\n");
            killProgram = true;
            break;
        }
    }

    if (killProgram)
    {
        printf("[read] killing program\n");
        shutdownThreads(programState, true, false);
    }
    // programState->readThreadRunning = false;
    
    // close(programState->readFile);
    printf("[read] thread end\n");
    // 
    // TODO: try recover, or do this through driverInit by the upper layer.
    //       We don't recover, udev will start the driver when device is connected again.
}

// Write disabled for now, by usb_temp.c:392
void *writeThread(void* arg)
{
    enableSigusr1();
    reportThreadUp();

    ProgramState* programState = (ProgramState*)arg;
    sem_post(programState->deviceReady);

    bool killProgram = false;
    printf("[read] programState->running: %d\n", programState->running);
    while (programState->running)
    {
        pthread_mutex_lock(&programState->writeWaitMut);
        while (programState->running && !programState->writeReady)
        {
            pthread_cond_wait(&programState->writeWaitCond, &programState->writeWaitMut);
            if (errno == EINTR)
            {
                printf("pthread_cond_wait interrupted\n");
            }
        }

        if (!programState->running)
        {
            printf("[write] !programState->running after wait\n");
            pthread_mutex_unlock(&programState->writeWaitMut);
            break;
            // exit
        }

        ssize_t count = 0;
        while (count < 2)
        {
            ssize_t written = write(programState->file, programState->writeBuffer + count, 2 - count);
            if (written <= 0) // 0 (eof) or -1 (error)
            {
                perror("write");
                killProgram = true;
                break;
            }
            count += written;
        }

        if (killProgram)
        {
            printf("[write] write error or eof\n");
            pthread_mutex_unlock(&programState->writeWaitMut);
            break;
        }

        programState->writeReady = 0;
        pthread_mutex_unlock(&programState->writeWaitMut);
    }

    if (killProgram)
    {
        printf("[write] killing program\n");
        shutdownThreads(programState, false, true);
    }

    printf("[write] end\n");
}

// Obsługa sygnału.
void signalHandler()
{
    // programState should be set to false, if we actually want to exit application.
    // Should be done before signaling.
}

static void initUsr1Signal()
{
    // Rejestracja obsługi sygnału.
    struct sigaction sa;
    sa.sa_handler = signalHandler;  
    // sa.sa_flags = SA_RESTART;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL))
    {
        printf("Failed to set SIGINT action\n");
    }

    sigset_t set;
    int s;
    
    // Blokowanie sygnału w mainie.
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    s = pthread_sigmask(SIG_BLOCK, &set, NULL);
    if (s != 0)
    {
        perror("pthread_sigmask");
    }
}

static ProgramState programState = {0};
//
// Zakładam, że ta funkcja jest wywoływana więcej niż raz w trakcie życia procesu.
// (Bo to w sumie jest wątek komunikujący się z urządzeniem, i jeżeli kończy się
//  to tracimy możliwość komunikacji z urządzeniem i sterownik przestaje być użyteczny.)
// 
int driverInit(ActionCallback callback, void* userData, sem_t* closing, sem_t* deviceReady)
{
    programState.file = open(devpath, O_RDWR | O_CLOEXEC);
    if (programState.file == -1)
    {
        perror("[init] fopen /dev/volumeknob");
        return errno;
    }

    // Sygnał do zakańczania blokujących operacji na plikach.
    // (Bo sterownik może nie implementować epolla)
    initUsr1Signal();
    programState.running = true;

    // programState.writeThreadRunning = true;
    // programState.readThreadRunning = true;
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
        close(programState.file);
        return errno;
    }

    if (pthread_create(&programState.readThread, NULL, readThread, (void*)&programState) != 0)
    {
        perror("Failed to create input thread");
        programState.running = false;
        close(programState.file);
        return errno;
    }

    if (pthread_create(&programState.writeThread, NULL, writeThread, (void*)&programState) != 0)
    {
        // TODO: zamknij czytający wątek.
        perror("Failed to create output thread");
        programState.running = false;
        close(programState.file);
        pthread_join(programState.readThread, NULL);
        return errno;
    }

    if (deviceReady != NULL)
    {
        sem_post(deviceReady);
    }


    if (closing)
    {
        // sem_wait(closing);
        while(sem_trywait(closing) == -1)
        {
            sleep(1);
            static uint8_t volume = 0;
            volumeChanged(volume);
            volume++;
        }
    }
    else
    {
        // TODO: runtime error. closing should be passed by audio.c
        while (programState.running)
        {
            sleep(1);
            // static uint8_t volume = 0;
            // volumeChanged(volume);
            // volume++;
        }
    }

    if (programState.running)
    {
        programState.running = false;

        pthread_kill(programState.readThread, SIGUSR1);
        pthread_kill(programState.writeThread, SIGUSR1);
    }

    pthread_join(programState.readThread, NULL);
    pthread_join(programState.writeThread, NULL);

    int result = close(programState.file);
    if (result == -1)
    {
        int closecount = 0;
        while (closecount++ < 2 && errno == EINTR)
        {
            if (closecount > 1)
            {
                printf("[driverMain] file close retry: %d, last_errno: %d\n", closecount, errno);
            }
            result = close(programState.file);
        }
        if (result != 0)
        {
            perror("file close");
        }
    }

    // TODO: Move to driverInit when he will be blocking.
    // TODO: Keep track if file was closed??? 
    // double close only results in EBADF errno.
    // close(programState.readFile);
    // close(programState.writeFile);

    return 0;
}

void volumeChanged(uint8_t volumePercent)
{
    printf("[volumeChanged] setting volume to: %d\n", volumePercent);
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

static sem_t closing;
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
        case ACTION_DEVICE_SHUTDOWN: 
        {
            printf("ACTION_DEVICE_SHUTDOWN");
            sem_post(&closing);
        }
        break;
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
    printf("[main] using device %s\n", devpath);
    
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