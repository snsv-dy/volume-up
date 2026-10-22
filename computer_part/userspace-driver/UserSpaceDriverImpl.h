#ifndef _USERSPACE_DRIVER_IMPL_H_
#define _USERSPACE_DRIVER_IMPL_H_

#include "device_interface.h"
#include <pthread.h>

typedef struct
{
    int file;
    int running;
    bool fileClosed;
    pthread_t handle;
} ThreadIo;

typedef struct
{
    int file;
    bool running;
    // int readThreadRunning;
    // int readFile;
    // bool readFileClosed;
    // pthread_t readThread;
    pthread_t readThread;

    ActionCallback callback;
    void* callbackData;

    // int writeThreadRunning;
    // int writeFile;
    // bool writeFileClosed;
    // pthread_t writeThread;
    pthread_t writeThread;

    uint8_t writeBuffer[2];
    pthread_cond_t writeWaitCond;
    pthread_mutex_t writeWaitMut;
    uint8_t writeReady;
    
    sem_t* closing;
    sem_t* deviceReady;
} ProgramState;

// typedef struct 
// {
//     int descriptor;
//     int state : 2; // 0b01 - opened, 0b10 closed.
// } File;



// Chyba nie konieczne do uruchomienia testów, ale nic nie szkodzi
// zrobić tego poprawniej.
void *readThread(void* arg);

#endif