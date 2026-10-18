#ifndef _USERSPACE_DRIVER_IMPL_H_
#define _USERSPACE_DRIVER_IMPL_H_

#include "device_interface.h"
#include <pthread.h>

typedef struct
{
    // FILE* device;
    int readThreadRunning;
    int readFile;
    pthread_t readThread;
    ActionCallback callback;
    void* callbackData;

    int writeThreadRunning;
    int writeFile;
    pthread_t writeThread;
    uint8_t writeBuffer[2];
    pthread_cond_t writeWaitCond;
    pthread_mutex_t writeWaitMut;
    uint8_t writeReady;
    
    sem_t* closing;
    sem_t* deviceReady;
} ProgramState;

// Chyba nie konieczne do uruchomienia testów, ale nic nie szkodzi
// zrobić tego poprawniej.
void *readThread(void* arg);

#endif