#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

static int timeToStop = 0;

// Obsługa sygnału.
void signalHandler()
{
    timeToStop = 1;
    printf("[signal]: timeToStop: %d\n", timeToStop);
}

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

void initSignal()
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

void *readThread(void* arg)
{
    enableSigusr1();

    printf("[read] thread begin\n");
    int file = (int)arg;

    char buffer[128];
    int result;
    while (!timeToStop)
    {
        printf("[read] start\n");
        result = read(file, buffer, 128);
        if (result < 0)
        {
            printf("[read] read error: %d\n", result);
            if (errno == EINTR)
            {
                printf("[read] interrupt, time to stop: %d\n", timeToStop);
            }
            else
            {
                perror("error aborting");
                break;
            }
        }
        else if (result == 0)
        {
            printf("[read] read eof\n");
            break;
        }
        else
        {
            printf("[read] read ok, len: %d\n", result);
            // ok
        }
    }
    printf("[read] completed, code: %d, errno: %d\n", result, errno);
}

int main()
{
    initSignal();

    int file = open("/dev/tty29", O_RDONLY | O_CLOEXEC);
    if (file == -1)
    {
        perror("open");
        return errno;
    }

    pthread_t thread;
    int result = pthread_create(&thread, NULL, readThread, file);
    if (result != 0)
    {
        perror("thread create");
        return result;
    }

    sleep(1);

    printf("[main] sending signal\n");
    // Do the signal here.
    result = pthread_kill(thread, SIGUSR1);
    printf("[main] kill result: %d\n", result);
    perror("[main] pthread_kill");
    
    printf("[main] joining thread\n");
    pthread_join(thread, NULL);

    printf("[main] closing file\n");
    result = close(file);
    if (result == -1)
    {
        perror("file close");
        return errno;
    }


    return 0;
}