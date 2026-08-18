#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdatomic.h>
#include <device_interface.h>

#define SMALL_STR_LEN 128
#define MEDIUM_STR_LEN 256
#define SINK_INPUTS_N 16
#define OPERATIONS_N 16

enum
{
    ACTION_INCORRECT    = 0,
    ACTION_INC5         = 0b0000001,
    ACTION_DEC5         = 0b0000010,
    ACTION_INC1         = 0b0000100,
    ACTION_DEC1         = 0b0001000,
    ACTION_SET24        = 0b0010000,
    ACTION_SET29        = 0b0100000,
    ACTION_VOLUME_SET_MASK  = 0b0111111,
    ACTION_UPDATE_OBJ   = 0b1000000,
    ACTION_SHUT_DOWN   = 0b10000000,
};

enum {
    STATE_INCORRECT = 0,

    // NEW concept
    // STATE_INCORRECT = ,
    STATE_UPDATING_OBJECTS = 14,
    STATE_INITIALIZED = 15,
    STATE_SETTING_VOLUME = 16,
    STATE_SHUTTING_DOWN = 17,
};

// Field list is here: http://0pointer.de/lennart/projects/pulseaudio/doxygen/structpa__sink__info.html
typedef struct pa_devicelist {
        uint8_t initialized;
        char name[512];
        uint32_t index;
        char description[256];
} pa_devicelist_t;


typedef struct {
    int action;

    sem_t actionReady;
    sem_t actionConsumed;
    int initial;
    pa_mainloop* mainloop;
} ItcStruct;

typedef struct {
    uint32_t set;
    uint32_t index;
    pa_volume_t volume;
    pa_cvolume cvolume;
} SmallerSinkInfo;

typedef struct {
    uint32_t initialized;
    uint32_t index;                      /**< Index of the sink input */
    char name[SMALL_STR_LEN];

    char mediaName[MEDIUM_STR_LEN];
    char appName[MEDIUM_STR_LEN];
    pa_cvolume volume;
} SinkInputInfo;

typedef struct
{
    uint32_t taken;
    uint32_t objIndex;
    int action;
    pa_subscription_event_type_t objType;
    pa_subscription_event_type_t eventType;
} OperationParams;

// TODO: Replace with your impl
//https://embeddedartistry.com/blog/2017/05/17/creating-a-circular-buffer-in-c-and-c/
typedef struct 
{
    uint32_t front;
    uint32_t back;
    uint32_t full;
    OperationParams data[OPERATIONS_N]; // +1 slot for full detection.
} OperationParamsCb;

typedef struct {
    // pa variables;
    pa_context *pa_ctx;
    pa_mainloop *pa_mainloop;
    pa_context_state_t pa_state;
    //
    int state;
    int initialized;
    char defaultSinkName[SMALL_STR_LEN];
    SmallerSinkInfo defaultSink;
    
    SinkInputInfo sinkInputs[SINK_INPUTS_N];

    OperationParams params; // Maybe unused?
    OperationParamsCb operations;
    pthread_mutex_t operationsMutex;

    ItcStruct* itc;
    sem_t deviceThreadSem;
    int running; // Na razie tylko w wątku urządzenia.
} OperationState;

//
//  TODO: Later
//
void operations_cb_init(OperationParamsCb* buffer);
// Allocates new element on the queue, and returns pointer to it.
// Null if queue is full.
OperationParams* operations_cb_nextFree(OperationParamsCb* buffer);
// Removes element from front.
// Returns 1 if element removed 0 if not.
uint32_t operations_cb_pop(OperationParamsCb* buffer);
// Returns pointer to oldest element.
OperationParams* operations_cb_front(OperationParamsCb* buffer);
// N element on the queue.
uint32_t operations_cb_size(OperationParamsCb* buffer);
void operationsCbTests();
//
//  TODO: Later
//

void nextState(OperationState* operationState);
void printSinkInput(SinkInputInfo* info);
void pa_state_cb(pa_context *c, void *userdata);
void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata);
void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata);

void sink_input_info_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata);
void remove_sink_input(OperationState* operationState, uint32_t index);

void QueueOperation(OperationState* operationState, uint32_t id, pa_subscription_event_type_t objectType, pa_subscription_event_type_t eventType, int action);
void FetchOperation(OperationState* operationState);

// W pierwszym kroku:
// [X] 1. Pobierz aktualną głośność systemu
// [X]  a0. funkcja do requestowania rzeczy.
// [X]  a. get server cośtam
// [X] 2. Zwiększ/Zmniejsz o 5%
// [X] 3. Zwiększ/Zmniejsz o obojętnie ile
// ------------------------------------
// Hw capab:
//  * Tryb klawiatury
//  * Tryb urządzenia ze sterownikiem
// * Przycisk Play/pause
// * Przycisk wyciszenia
// ====================================
// W kolejnym kroku
// [x] 1. subskrybuj eventy
//     2. jednocześnie pozwalaj na input
// ====================================
// później na spokojnie
// 0.1. Poprawne zamykanie aplikacji? (Może być potrzebne gdy system odłączy sterownik po odłączeniu urządzenia)
// 1. Zmiana głośności sink inputa youtuba (może się resetować przy zmianie filmu, bo zmiana nie pochodzi z ui)


void setAction(OperationState* operationState, int action);
int WaitForAction(ItcStruct* itc);
void OperationCompleted(ItcStruct* itc);
int pulseaudioMainLoop(OperationState* operationState);

void* stdinInputThread(void* arg)
{
    OperationState* operationState = (OperationState*)arg;
    assert(operationState);

    int running = 1;
    char actionChar = '\0';
    int action;
    while(running)
    {
        action = ACTION_INCORRECT;
        printf("> ");
        int nGot = scanf("%c", &actionChar);
        if (nGot == 1)
        {
            if (actionChar == '+') { action = ACTION_INC5; }
            else if (actionChar == '-') { action = ACTION_DEC5; }
            else if (actionChar == 'i') { action = ACTION_INC1; }
            else if (actionChar == 'd') { action = ACTION_DEC1; }
            else if (actionChar == '4') { action = ACTION_SET24; }
            else if (actionChar == '9') { action = ACTION_SET29; }
            else if (actionChar == '\n') { continue; }
            else if (actionChar == 'q') { return NULL; }
            
            printf("got '%c', ", actionChar);
            printf("Sending action: ");
            switch (action)
            {
                case ACTION_INC5: printf("ACTION_INC5"); break;
                case ACTION_DEC5: printf("ACTION_DEC5"); break;
                case ACTION_INC1: printf("ACTION_INC1"); break;
                case ACTION_DEC1: printf("ACTION_DEC1"); break;
                case ACTION_SET24: printf("ACTION_SET24"); break;
                case ACTION_SET29: printf("ACTION_SET29"); break;
                default:
            }
            printf("\n");

            setAction(operationState, action);
        }
    }
}

void actionFromDeviceCallback(uint32_t action, void* userData)
{
    OperationState* operationState = (OperationState*)userData;
    uint32_t paAction = 0;
    switch(action)
    {
        case 1: paAction = 0b0000001; break; //ACTION_INC5  ACTION_INC5
        case 2: paAction = 0b0000010; break; //ACTION_DEC5  ACTION_DEC5
        case 3: paAction = 0b0000100; break; //ACTION_INC1  ACTION_INC1
        case 4: paAction = 0b0001000; break; //ACTION_DEC1  ACTION_DEC1
        case 5: paAction = 0b0010000; break; //ACTION_SET24 ACTION_SET24
        case 6: paAction = 0b0100000; break; //ACTION_SET29 ACTION_SET29
        case 7: paAction = 0; break; //ACTION_GET_VOLUME   = ,
    }

    printf("[audio.c] got action: %x\n", paAction);

    setAction(operationState, paAction);
}

void* inputThread(void* arg)
{
    OperationState* operationState = (OperationState*)arg;
    assert(operationState);

    char actionChar = '\0';
    int action;

    // #define INIT_RETRIES 2
    // while(running)
    // {
        // if (moduleState == MODULE_STATE_INCORRECT)
        // {
    printf("Trying to connect to device\n");
    const int maxRetries = 2;
    int nRetries = 0;
    while (operationState->running && nRetries++ < maxRetries && driverInit(actionFromDeviceCallback, arg, &operationState->deviceThreadSem))
    {
        if (!operationState->running)
        {
            break;
        }
        usleep(1000 * 1000); // 1000ms
    }
    printf("[inputThread] exit.\n");
        // }
    // }
}

void initInputThread(pthread_t* thread, OperationState* operationState)
{
    assert(thread && operationState);
    // TODO:
    // TODO: Gracefully destroy this shit after exit.
    if (
           sem_init(&(operationState->itc->actionReady), 0, 0)
        || sem_init(&(operationState->itc->actionConsumed), 0, 1)
        || pthread_mutex_init(&operationState->operationsMutex, NULL)
        || sem_init(&(operationState->deviceThreadSem), 0, 0)
    )
    {
        printf("Failed to initialize itc\n");
        // exit here;
        return;
    }
    operationState->running = 1;
    

    if (pthread_create(thread, NULL, inputThread, (void*)operationState) != 0)
    {
        printf("Failed to create input thread\n");
    }
}

void setAction(OperationState* operationState, int action)
{

    QueueOperation(operationState, 0, 0, 0, action);
    // TODO: !!!
    nextState(operationState);
    // FetchOperation(operationState);
}

int WaitForAction(ItcStruct* itc)
{
    // TODO: Nie zadziała ze subskrypcjami.
    // sem_wait(&itc->actionReady);
    
    return itc->action;
}

void OperationCompleted(ItcStruct* itc)
{
    itc->initial = 0;
    itc->action = ACTION_INCORRECT;
    sem_post(&itc->actionConsumed);
}

// For the interrupt signal handler 
// (Proszę zachować szczególną ostrożność podczas używania)
static OperationState* __globalOperationState = NULL;

void interruptSignalHandler()
{
    if (!__globalOperationState)
    {
        printf("[ctrl+c] !__globalOperationState\n");
    }
    printf("[ctrl+c] Received\n");
    
    // Zamykanie wątku komunikacji z urządzeniem.
    __globalOperationState->running = 0; // TODO: atomic or mutex for this?
    sem_post(&__globalOperationState->deviceThreadSem);
    // Zamykanie wątku pulseaudio.
    QueueOperation(__globalOperationState, 0, 0, 0, ACTION_SHUT_DOWN);
    nextState(__globalOperationState);

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

int main(int argc, char *argv[]) {
    // operationsCbTests();
    // return 0;
    int ctr;

    // pa_context_subscribe();
    pthread_t inputThread;
    ItcStruct itc = {0};
    itc.initial = 1;

    OperationState operationState = {0};
    __globalOperationState = &operationState;
    operationState.state = STATE_INCORRECT;
    operationState.itc = &itc;
    operations_cb_init(&operationState.operations);

    initInterruptSignal();

    initInputThread(&inputThread, &operationState);
    printf("input thread inited\n");

    if (pulseaudioMainLoop(&operationState) < 0) {
        fprintf(stderr, "failed to start pulseaudio\n");
        return 1;
    }
    printf("Mainloop exit\n");

    pthread_join(inputThread, NULL);

    return 0;
}



void server_info_cb(pa_context *c, const pa_server_info*i, void *userdata)
{
    OperationState* operationState = userdata;
    if (userdata)
    {
        strncpy(operationState->defaultSinkName, i->default_sink_name, SMALL_STR_LEN - 1);
    }
    nextState(operationState);
}


void sink_info_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata)
{
    OperationState* operationState = (OperationState*)userdata;
    if (eol < 0) {
        printf("Failed to get sink information: %s", pa_strerror(pa_context_errno(c)));
        nextState(operationState);
        return;
    }

    if (eol)
    {
        printf("eol\n");
        nextState(operationState);
        return;
    }
    else
    {
        printf("sink\n");
    }

    if (i == NULL)
    {
        printf("Sink with that name is null, eol: %d\n", eol);
        return;
    }

    // Hanlde only default sink for now.
    // TODO: Handle default sink change.
    if (operationState->defaultSinkName[0] && strcmp(i->name, operationState->defaultSinkName))
    {
        // nextState(operationState);
        return;
    }

    operationState->defaultSink.index = i->index;
    operationState->defaultSink.volume = pa_cvolume_avg(&(i->volume));
    operationState->defaultSink.cvolume.channels = i->volume.channels;
    for (int index = 0; index < i->volume.channels; index++)
    {
        operationState->defaultSink.cvolume.values[index] = i->volume.values[index];
    }
    operationState->defaultSink.set = 1;
    printf("[Sink updated] sink[%d] vol: %2f\n", operationState->defaultSink.index, (double)pa_cvolume_avg(&operationState->defaultSink.cvolume) / PA_VOLUME_NORM * 100.0);
    
    nextState(operationState);
}

void set_volume_success_cb(pa_context *c, int success, void *userdata)
{
    OperationState* operationState = userdata;
    if (!success)
    {
        printf("No success\n");
    }
    nextState(operationState);
}

void setVolume(pa_cvolume* cvolume, int action)
{
    printf("Action: ");
    switch (action)
    {
        case ACTION_INC5: printf("ACTION_INC5"); break;
        case ACTION_DEC5: printf("ACTION_DEC5"); break;
        case ACTION_INC1: printf("ACTION_INC1"); break;
        case ACTION_DEC1: printf("ACTION_DEC1"); break;
        case ACTION_SET24: printf("ACTION_SET24"); break;
        case ACTION_SET29: printf("ACTION_SET29"); break;
        default:
    }
    printf("\n");

    static const struct percentMap {
        uint32_t action;
        double factor;
    } percents[] = {
        {ACTION_INC5, 0.05},
        {ACTION_DEC5, -0.05},
        {ACTION_INC1, 0.01},
        {ACTION_DEC1, -0.01},
        {ACTION_SET24, 0.24},
        {ACTION_SET29, 0.29},
    };

    double factor = 0.0;
    for (int index = 0; index < 6; index++) 
    {
        factor = percents[index].factor;
        if (percents[index].action == action)
        {
            break;
        }
    }
        
    const pa_volume_t oldVolume = pa_cvolume_avg(cvolume);
    if ((action & (ACTION_INC5 | ACTION_DEC5 | ACTION_INC1 | ACTION_DEC1)) != 0)
    {
        if (factor > 0.0)
        {
            pa_cvolume_inc(cvolume, PA_VOLUME_NORM * factor);
        }
        else
        {
            pa_cvolume_dec(cvolume, PA_VOLUME_NORM * -factor);
        }
    }
    else if ((action & (ACTION_SET24 | ACTION_SET29)) != 0)
    {
        pa_cvolume_set(cvolume, cvolume->channels, PA_VOLUME_NORM * factor);
    }

    float oldVolumePercent = (double)oldVolume / PA_VOLUME_NORM * 100.0;
    float newVolumePercent = (double)pa_cvolume_avg(cvolume) / PA_VOLUME_NORM * 100.0;
    printf("Action: decrease by 5%%, old volume: %u (%2.2f), setting to: %u (%2.2f)\n",
        oldVolume,
        oldVolumePercent,
        pa_cvolume_avg(cvolume),
        newVolumePercent
    );
}

int isOperation(pa_operation** pa_op)
{
    if (*pa_op) {
        if (pa_operation_get_state(*pa_op) == PA_OPERATION_DONE)
        {
            pa_operation_unref(*pa_op);
            *pa_op = NULL;
        }
        else
        {
            return 1;
        }
    }
    return 0;
}



// int NextState(OperationState* s)
// {
//     // 1. Request operation() (ethier by subscription event, or user input)
//     // 2. Store operation info/steps in OperationState.operation
//     // 3. In callback call OperationComplete() to move to the next step, or finish.
//     /*
//         for example:

//         RequestOperation(os, INC_DEF_SINK_BY_5);
//         1. if !os.operation.active -> os.operation.active = 1;
//            else drop it (later queue operations)
//            os.operation.type = INC_DEF_SINK_BY_5;
//            OperationComplete(os)
//         2. from OperationComplete()
//            switch (os.operation.type)
//            case INC_DEF_SINK_BY_5:
//            if !os.defaultSink.name -> os.operation.state = GET_SERVER
//            else if !os.defaultSink.name -> ... <- we get name
//            else if !os.defaultSink.id -> os.operation.state = GET_SINK_INFO <- we get info (id, volume)
//            else os.operation.state = GET_SINK_VOL
//         3. State read by mainloop, and doing shit.
//     */

//     return STATE_INCORRECT;
// }

void subscribe_cb(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata)
{
    // PA_SUBSCRIPTION_EVENT_FACILITY_MASK  <- kind of object
    // PA_SUBSCRIPTION_EVENT_TYPE_MASK      <- what happened
    int objectType = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
    int eventType  = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;
    
    printf("[subscribe] obj: %2x, event: %2x, id: %d\n", objectType, eventType, idx);
    OperationState* operationState = (OperationState *)userdata;
    if (   (objectType == PA_SUBSCRIPTION_EVENT_SINK && eventType == PA_SUBSCRIPTION_EVENT_CHANGE)
        || (objectType == PA_SUBSCRIPTION_EVENT_SINK_INPUT))
    {
        QueueOperation(operationState, idx, objectType, eventType, ACTION_UPDATE_OBJ);
    }
    // if (operation->state == STATE_INITALIZED)
    // {
    //     operation->;
    // }
    nextState(operationState);
}

void context_success_cb(pa_context *c, int success, void *userdata)
{
    OperationState* operationState = (OperationState*)userdata;
    operationState->initialized = success ? 1 : -1;
    // *(int *)userdata = success;
    printf("Set subscribe success: %d\n", success);
    nextState(operationState);
}

void QueueOperation(OperationState* operationState, uint32_t id, pa_subscription_event_type_t objectType, pa_subscription_event_type_t eventType, int action)
{
    // TODO: To spróbuj zrobić
    // TODO: To spróbuj zrobić
    // TODO: To spróbuj zrobić
    // TODO: To spróbuj zrobić
    // TODO: Alternatively prolly just requesting pa_get_sink_info_by_id and unlinking operation will suffice.
    //       (a will be much simpler)
    // TODO: To spróbuj zrobić
    // TODO: To spróbuj zrobić
    // TODO: To spróbuj zrobić
    // TODO: To spróbuj zrobić
    pthread_mutex_lock(&operationState->operationsMutex);
    OperationParams* params = operations_cb_nextFree(&operationState->operations);
    if (params == NULL)
    {
        printf("operation buffer full. Skipping update\n");
        pthread_mutex_unlock(&operationState->operationsMutex);
        return;
    }

    params->objIndex = id;
    params->objType = objectType;
    params->eventType = eventType;
    params->action = action;

    pthread_mutex_unlock(&operationState->operationsMutex);
    FetchOperation(operationState);
}

void FetchOperation(OperationState* operationState)
{
    // TODO: Debug with device attached. (sudo or add device to udev)
    pthread_mutex_lock(&operationState->operationsMutex);
    OperationParams* operation = operations_cb_front(&operationState->operations);
    if (operation != NULL && operationState->state != STATE_INCORRECT && operationState->itc->action == ACTION_INCORRECT)
    {
        operationState->itc->action = operation->action;
        // operations_cb_pop(&operationState->operations);

        pa_mainloop_wakeup(operationState->itc->mainloop);
        printf("waking Mainloop\n");
    }
    else
    {
        printf("FetchOperation: %d %d %d\n", operation != NULL, operationState->state, operationState->itc->action == ACTION_INCORRECT);
    }
    pthread_mutex_unlock(&operationState->operationsMutex);
}
// void pa_source_info_cb(pa_context *c, const pa_source_info *i, int eol, void *userdata);


int pulseaudioMainLoop(OperationState* operationState) {
    // Define our pulse audio loop and connection variables
    pa_mainloop *pa_ml;
    pa_mainloop_api *pa_mlapi;
    // pa_operation *pa_op = NULL;
    pa_context *pa_ctx;

    // int state = STATE_INIT;
    int pa_ready = 0;

    char defaultSinkName[128] = {0};
    SmallerSinkInfo defaultSinkInfo = {0};

    // Initialize our device lists
    // memset(input, 0, sizeof(pa_devicelist_t) * 16);
    // memset(output, 0, sizeof(pa_devicelist_t) * 16);
    // memset(sinkInputs, 0, sizeof(SinkInputInfo) * 16);

    // Create a mainloop API and connection to the default server
    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, "test");
    operationState->itc->mainloop = pa_ml;

    // This function connects to the pulse server
    pa_context_connect(pa_ctx, NULL, 0, NULL);

    // This function defines a callback so the server will tell us it's state.
    // Our callback will wait for the state to be ready.  The callback will
    // modify the variable to 1 so we know when we have a connection and it's
    // ready.
    // If there's an error, the callback will set pa_ready to 2
    pa_context_set_state_callback(pa_ctx, pa_state_cb, operationState);
    pa_context_set_subscribe_callback(pa_ctx, subscribe_cb, operationState);

    operationState->pa_ctx = pa_ctx;
    operationState->pa_mainloop = pa_ml;

    pa_mainloop_wakeup(pa_ml);
    // Now we'll enter into an infinite loop until we get the data we receive
    // or if there's an error
    // pa_mainloop_run(pa_ml);
    // return;
    int first = 1;
    printf("[audio.c]begin loop\n");
    int action = ACTION_INCORRECT;
    // for (;;) {

        // Iterate the main loop and go again.  The second argument is whether
        // or not the iteration should block until something is ready to be
        // done.  Set it to zero for non-blocking.
        // if (!first)
        // {
        //     pa_mainloop_iterate(pa_ml, 1, NULL);
        // }
        // first = 0;
        // printf("[audio.c]iterate\n");

        // We can't do anything until PA is ready, so just iterate the mainloop
        // and continue
        // if (pa_ready == 0) {
        //     // pa_mainloop_iterate(pa_ml, 1, NULL);
        //     continue;
        // }
        // // We couldn't get a connection to the server, so exit out
        // if (pa_ready == 2) {
        //     pa_context_disconnect(pa_ctx);
        //     pa_context_unref(pa_ctx);
        //     pa_mainloop_free(pa_ml);
        //     return -1;
        // }

        // if ((operationState->state != STATE_INCORRECT) && action == ACTION_INCORRECT)
        // {
        //     // printf("Waiting for action\n");
        //     action = WaitForAction(operationState->itc);
        //     if (action == ACTION_INCORRECT)
        //     {
        //         // printf("action incorrect\n");
        //         continue;
        //     }
        //     // printf("action aquired: %x\n", action);
        // }
    printf("[audio.c] mainloop run\n");
    int retval = 0;
    pa_mainloop_run(pa_ml, &retval);

    printf("[audio.c] mainloop finished\n");
    pa_context_disconnect(pa_ctx);
    pa_context_unref(pa_ctx);
    pa_mainloop_free(pa_ml);

    return retval;
}

void nextState(OperationState* operationState)
{
    printf("[nextState]\n");
    pa_operation *pa_op = NULL;
        // printf("got state: %d\n", operationState->state);

    // Do this once, unless specially requested. (set volume and update objects states)
    for (int nLoops = 0; nLoops < 1; nLoops++)
    {
        switch (operationState->state) {
            case STATE_INCORRECT: // Need to set up stuff
                // if (isOperation(&pa_op))
                // {
                //     break;
                // }
                
                // Setting subscription.
                if (!operationState->initialized)
                {
                    // initialize stuff
                    pa_op = pa_context_subscribe(
                        operationState->pa_ctx, 
                        PA_SUBSCRIPTION_MASK_SINK_INPUT
                        | PA_SUBSCRIPTION_MASK_SINK,
                        context_success_cb,
                        (void*)operationState);
                    pa_operation_unref(pa_op);
                    break;
                }

                // Getting default sink info
                if (operationState->defaultSinkName[0] == '\0')
                {
                    pa_op = pa_context_get_server_info(operationState->pa_ctx, server_info_cb, (void *)operationState);
                    pa_operation_unref(pa_op);
                    break;
                }

                if (!operationState->defaultSink.set)
                {
                    printf("Default sink name: '%s'\n", operationState->defaultSinkName);
                    pa_op = pa_context_get_sink_info_by_name(operationState->pa_ctx, operationState->defaultSinkName, sink_info_cb, (void *)operationState);
                    pa_operation_unref(pa_op);
                    break;
                }

                // TODO: To następne
                // TODO: To następne
                // TODO: To następne
                // TODO: To następne
                // TODO: Get all sink inputs.
                pa_op = pa_context_get_sink_input_info_list(
                    operationState->pa_ctx,
                    sink_input_info_cb,
                    operationState
                );
                pa_operation_unref(pa_op);
                pa_op = NULL;
                // TODO: To następne
                // TODO: To następne
                // TODO: To następne
                // TODO: To następne

                printf("state initialized\n");
                operationState->state = STATE_INITIALIZED;
                break;
            case STATE_INITIALIZED:
                // if (isOperation(&pa_op))
                // {
                //     break;
                // }
                // TODO: Get action from first element of queue maybe?
                //       And after setting request to the pa, the operation is finished.
                if (operationState->itc->action == ACTION_INCORRECT)
                {
                    FetchOperation(operationState);
                }

                if (operationState->itc->action & ACTION_VOLUME_SET_MASK)
                {
                    if (operationState->defaultSink.set)
                    {
                        printf("Default sink index: %d\n", operationState->defaultSink.index);
                        setVolume(&operationState->defaultSink.cvolume, operationState->itc->action);
                        pa_op = pa_context_set_sink_volume_by_index(
                            operationState->pa_ctx, 
                            operationState->defaultSink.index, 
                            &operationState->defaultSink.cvolume, 
                            set_volume_success_cb, 
                            operationState);
                        pa_operation_unref(pa_op);
                        operationState->state = STATE_SETTING_VOLUME;
                        break;
                    }
                    else
                    {
                        printf("Default sink info not set\n");
                        // state -> INCORRECT?/UPDATING_OBJECTS
                    }
                }
                else if (operationState->itc->action == ACTION_UPDATE_OBJ)
                {
                    pthread_mutex_lock(&operationState->operationsMutex);
                    OperationParams* params = operations_cb_front(&operationState->operations);
                    if (!params)
                    {
                        operationState->itc->action = ACTION_INCORRECT;
                        pthread_mutex_unlock(&operationState->operationsMutex);
                        break;
                    }

                    switch(params->objType)
                    {
                        case PA_SUBSCRIPTION_EVENT_SINK:
                            if (    params->eventType == PA_SUBSCRIPTION_EVENT_NEW
                                ||  params->eventType == PA_SUBSCRIPTION_EVENT_CHANGE)
                            {
                                pa_op = pa_context_get_sink_info_by_index(
                                    operationState->pa_ctx,
                                    params->objIndex,
                                    sink_info_cb,
                                    operationState
                                );
                                pa_operation_unref(pa_op);
                            }
                            break;
                        case PA_SUBSCRIPTION_EVENT_SINK_INPUT:
                            if (    params->eventType == PA_SUBSCRIPTION_EVENT_NEW
                                ||  params->eventType == PA_SUBSCRIPTION_EVENT_CHANGE)
                            {
                                pa_op = pa_context_get_sink_input_info(
                                    operationState->pa_ctx,
                                    params->objIndex,
                                    sink_input_info_cb,
                                    operationState
                                );
                                pa_operation_unref(pa_op);
                            }
                            else
                            {
                                // Idk if callback would be called if object is destroyed.
                                // (the one above)
                                remove_sink_input(operationState, params->objIndex);
                            }
                            break;
                            default:
                                operationState->itc->action = ACTION_INCORRECT;
                    }
                    operationState->state = operationState->itc->action != ACTION_INCORRECT ? STATE_UPDATING_OBJECTS : STATE_INITIALIZED;
                    pthread_mutex_unlock(&operationState->operationsMutex);
                    break;
                }
                else if (operationState->itc->action == ACTION_SHUT_DOWN)
                {
                    // pa_context_disconnect(pa_ctx);
                    // pa_context_unref(pa_ctx);
                    // pa_mainloop_free(pa_ml);
                    pa_mainloop_quit(operationState->pa_mainloop, 1);
                    return;
                }
            break;
            case STATE_UPDATING_OBJECTS:
            case STATE_SETTING_VOLUME:
                printf("[nextState] operation completed\n");
                    // if (isOperation(&pa_op))
                    // {
                    //     break;
                    // }
                    // if (action == ACTION_UPDATE_OBJ)
                    // {
                        pthread_mutex_lock(&operationState->operationsMutex);
                        operations_cb_pop(&operationState->operations);
                        pthread_mutex_unlock(&operationState->operationsMutex);
                    // }
                    // pa_op = NULL;
                    
                    // TODO: botch, just for thread input testing.
                    //       rework when subscriptions.
                    // STATE_INITstate = GET_DEFAULT_SINK_VOLUME;
                    
                    
                    operationState->params.objIndex = 0;
                    operationState->params.objType = 0;
                    operationState->state = STATE_INITIALIZED;
                    operationState->itc->action = ACTION_INCORRECT;
                    OperationCompleted(operationState->itc);
                    FetchOperation(operationState);
                    nLoops--; // Go to the Initialized state and check if there is something to do.
                break;
            case STATE_SHUTTING_DOWN:
                // if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                //     pa_operation_unref(pa_op);
                //     pa_context_disconnect(pa_ctx);
                //     pa_context_unref(pa_ctx);
                //     pa_mainloop_free(pa_ml);
                //     return 0;
                // }
                // break;

            default:
                // We should never see this state
                fprintf(stderr, "in state %d\n", operationState->state);
                // return -1;
        }
    }
}

void printSinkInput(SinkInputInfo* info)
{
    if (info->initialized)
    {
        double volumePercent = (double)pa_cvolume_avg(&info->volume) / PA_VOLUME_NORM * 100.0;
        printf(
            "=== [SinkInput] ===\n"
            "index              : %3d\n"
            "name               : %s\n"
            "media name         : %s\n"
            "application name   : %s\n"
            "volume             : %2.f\n"
            "===================\n",
            info->index, 
            info->name,
            info->mediaName,
            info->appName,
            volumePercent
        );
    }
}

void remove_sink_input(OperationState* operationState, uint32_t index)
{
    SinkInputInfo* sinkInputList = operationState->sinkInputs;
    
    for (int i = 0; i < SINK_INPUTS_N; i++) {
        if (sinkInputList[i].initialized && sinkInputList[i].index == index)
        {
            sinkInputList[i].initialized = 0;
            printf("[SinkInput] removed: %3d\n", index);
        }
    }
}

void sink_input_info_cb(pa_context *c, const pa_sink_input_info *info, int eol, void *userdata)
{
    // SinkInputInfo *sinkInputInfoList = userdata;
    OperationState* operationState = userdata;
    SinkInputInfo* sinkInputList = operationState->sinkInputs;
    
    if (eol > 0) {
        nextState(operationState);
        return;
    }

    if (eol < 0)
    {
        // eol < 0 == some error happened
        // probably sink input was removed somewhere between 
        // requesting information about it.
        printf("[sink input info cb] eol < 0\n");
        nextState(operationState);
        return;
    }

    OperationParams* params = operations_cb_front(&operationState->operations);
    if (operationState->state == STATE_UPDATING_OBJECTS && params == NULL)
    {
        // When not initialized. It might just listing them.
        printf("[SinkInput cb] No operation, idk what to do. id: %d\n", info->index);
        nextState(operationState);
        return;
    }

    // TODO: ehhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh
    OperationParams tempParams = {0};
    if (params == NULL)
    {
        params = &tempParams;
        params->eventType = PA_SUBSCRIPTION_EVENT_NEW;
    }
    
    SinkInputInfo* sink = NULL;
    for (int i = 0; i < SINK_INPUTS_N; i++) {
        if (
            (params->eventType == PA_SUBSCRIPTION_EVENT_NEW && !sinkInputList[i].initialized)
            || (((params->eventType == PA_SUBSCRIPTION_EVENT_CHANGE) 
                 || (params->eventType == PA_SUBSCRIPTION_EVENT_REMOVE)) 
                 && sinkInputList[i].initialized)
            )
        {
            sink = &(sinkInputList[i]);
        }
    }

    if (sink)
    {
        if (params->eventType == PA_SUBSCRIPTION_EVENT_REMOVE)
        {
            sink->initialized = 0;
            printf("[SinkInput] removed: %3d\n", sink->index);
            return;
        }
        sink->index = info->index;
        sink->volume.channels = info->volume.channels;
        for (int index = 0; index < info->volume.channels; index++)
        {
            sink->volume.values[index] = info->volume.values[index];
        }
        // char mediaName[256];
        // char appName[256];

        // TODO: Check if its necessary to copy the strings on update;
        strncpy(sink->name, info->name, SMALL_STR_LEN - 1);

        if (pa_proplist_contains(info->proplist, "media.name") == 1)
        {
            const char* propStr = pa_proplist_gets(info->proplist, "media.name");
            strncpy(sink->mediaName, propStr, MEDIUM_STR_LEN - 1);
        }
        if (pa_proplist_contains(info->proplist, "application.name") == 1)
        {
            const char* propStr = pa_proplist_gets(info->proplist, "application.name");
            strncpy(sink->appName, propStr, MEDIUM_STR_LEN - 1);
        }
        sink->initialized = 1;
        printSinkInput(sink);
    }

    nextState(operationState);
}

// This callback gets called when our context changes state.  We really only
// care about when it's ready or if it has failed
void pa_state_cb(pa_context *c, void *userdata) {
    // pa_context_state_t state;
    OperationState* operationState = userdata;
    // int *pa_ready = userdata;

    operationState->pa_state = pa_context_get_state(c);
    switch  (operationState->pa_state) {
            // There are just here for reference
            case PA_CONTEXT_UNCONNECTED:
            case PA_CONTEXT_CONNECTING:
            case PA_CONTEXT_AUTHORIZING:
            case PA_CONTEXT_SETTING_NAME:
            default:
                    break;
            case PA_CONTEXT_FAILED:
            case PA_CONTEXT_TERMINATED:
                    // TODO: Quit the pa main loop;
                    pa_mainloop_quit(operationState->pa_mainloop, 1);
                    break;
            case PA_CONTEXT_READY:
                // Start executing requests.
                nextState(operationState);
                    // *pa_ready = 1;
                    // operationState->pa_ready
                    return;
    }
}

// pa_mainloop will call this function when it's ready to tell us about a sink.
// Since we're not threading, there's no need for mutexes on the devicelist
// structure
void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata) {
    pa_devicelist_t *pa_devicelist = userdata;
    int ctr = 0;

    // If eol is set to a positive number, you're at the end of the list
    if (eol > 0) {
        return;
    }

    // We know we've allocated 16 slots to hold devices.  Loop through our
    // structure and find the first one that's "uninitialized."  Copy the
    // contents into it and we're done.  If we receive more than 16 devices,
    // they're going to get dropped.  You could make this dynamically allocate
    // space for the device list, but this is a simple example.
    for (ctr = 0; ctr < 16; ctr++) {
        if (! pa_devicelist[ctr].initialized) {
            strncpy(pa_devicelist[ctr].name, l->name, 511);
            strncpy(pa_devicelist[ctr].description, l->description, 255);
            pa_devicelist[ctr].index = l->index;
            pa_devicelist[ctr].initialized = 1;
            break;
        }
    }
}

// See above.  This callback is pretty much identical to the previous
void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata) {
    pa_devicelist_t *pa_devicelist = userdata;
    int ctr = 0;

    if (eol > 0) {
        return;
    }

    for (ctr = 0; ctr < 16; ctr++) {
        if (! pa_devicelist[ctr].initialized) {
            strncpy(pa_devicelist[ctr].name, l->name, 511);
            strncpy(pa_devicelist[ctr].description, l->description, 255);
            pa_devicelist[ctr].index = l->index;
            pa_devicelist[ctr].initialized = 1;
            break;
        }
    }
}


uint32_t operations_cb_empty(OperationParamsCb* buffer)
{
    return !buffer->full && (buffer->front == buffer->back);
}


uint32_t operations_cb_full(OperationParamsCb* buffer)
{
    return buffer->full;
}

void operations_cb_advance(OperationParamsCb* buffer)
{
    if (buffer->full)
    {
        buffer->front = (buffer->front + 1) % OPERATIONS_N;    
    }

    buffer->back = (buffer->back + 1) % OPERATIONS_N;
    buffer->full = (buffer->front == buffer->back);
}

void operations_cb_retreat(OperationParamsCb* buffer)
{
    buffer->full = 0;
    buffer->front = (buffer->front + 1) % OPERATIONS_N;
}

void operations_cb_init(OperationParamsCb* buffer)
{
    assert(buffer);

    buffer->front = 0;
    buffer->back = 0;
    buffer->full = 0;
}

OperationParams* operations_cb_nextFree(OperationParamsCb* buffer)
{
    if (!operations_cb_full(buffer))
    {
        OperationParams* freeElem = &buffer->data[buffer->back];
        freeElem->taken = 1;
        operations_cb_advance(buffer);
        return freeElem;
    }

    return NULL;
}

uint32_t operations_cb_pop(OperationParamsCb* buffer)
{
    if (!operations_cb_empty(buffer))
    {
        buffer->data[buffer->front].taken = 0;
        operations_cb_retreat(buffer);
        return 1;
    }

    return 0;
}

OperationParams* operations_cb_front(OperationParamsCb* buffer)
{
    OperationParams* item =  &buffer->data[buffer->front];
    return !operations_cb_empty(buffer) ? item : NULL;
}

uint32_t operations_cb_size(OperationParamsCb* buffer)
{
    uint32_t size = OPERATIONS_N;
    if (!buffer->full)
    {
        if (buffer->back >= buffer->front)
        {
            size = buffer->back - buffer->front;
        }
        else
        {
            size = OPERATIONS_N + buffer->back - buffer->front;
        }
    }

    return size;
}


void operationsCbTests_push_pop()
{
    OperationParamsCb queue;
    operations_cb_init(&queue);

    for (int i = 0; i < OPERATIONS_N; i++)
    {
        OperationParams* item = operations_cb_nextFree(&queue);
        assert(item);
        assert(item->taken);
        item->objIndex = i;
        printf("Pushing: %d, \n", i);
    }

    for (int i = OPERATIONS_N - 1;  i >= 0; i-- )
    {
        OperationParams* item = operations_cb_front(&queue);
        assert(item);
        assert(item->taken);
        assert(operations_cb_pop(&queue));
        printf("removed: %d\n", i);
    }
}

void operationsCbTests_push_full()
{
    OperationParamsCb queue;
    operations_cb_init(&queue);

    for (int i = 0; i < OPERATIONS_N; i++)
    {
        OperationParams* item = operations_cb_nextFree(&queue);
        assert(item);
        assert(item->taken);
        item->objIndex = i;
    }

    OperationParams* item = operations_cb_nextFree(&queue);
    assert(!item);
    assert(operations_cb_size(&queue) == OPERATIONS_N);
}


void operationsCbTests_pop_empty()
{
    OperationParamsCb queue;
    operations_cb_init(&queue);

    OperationParams* item = operations_cb_front(&queue);
    assert(!item);
    assert(operations_cb_size(&queue) == 0);
    assert(operations_cb_pop(&queue) == 0);
}

void operationsCbTests_back_lt_front_size()
{
    OperationParamsCb queue;
    operations_cb_init(&queue);

    for (int i = 0; i < OPERATIONS_N; i++)
    {
        OperationParams* item = operations_cb_nextFree(&queue);
        assert(item);
        assert(item->taken);
        item->objIndex = i;
    }

    for (int i = 0; i < (OPERATIONS_N) / 4 * 3; i++)
    {
        operations_cb_pop(&queue);
    }


    for (int i = 0; i < (OPERATIONS_N) / 4; i++)
    {
        OperationParams* item = operations_cb_nextFree(&queue);
        assert(item);
        assert(item->taken);
    }
    printf("size: %d\n", operations_cb_size(&queue));
    assert(operations_cb_size(&queue) == (OPERATIONS_N) / 2);
}
void operationsCbTests_back_lt_front_size2()
{
    OperationParamsCb queue;
    operations_cb_init(&queue);

    for (int i = 0; i < OPERATIONS_N; i++)
    {
        OperationParams* item = operations_cb_nextFree(&queue);
        assert(item);
        assert(item->taken);
        item->objIndex = i;
    }

    for (int i = 0; i < (OPERATIONS_N) -1; i++)
    {
        operations_cb_pop(&queue);
    }


    for (int i = 0; i < (OPERATIONS_N) -2; i++)
    {
        OperationParams* item = operations_cb_nextFree(&queue);
        assert(item);
        assert(item->taken);
    }
    printf("size: %d\n", operations_cb_size(&queue));
    assert(operations_cb_size(&queue) == (OPERATIONS_N - 1));
}

void operationsCbTests()
{
    operationsCbTests_push_pop();
    operationsCbTests_push_full();
    operationsCbTests_pop_empty();
    operationsCbTests_back_lt_front_size2();
    operationsCbTests_back_lt_front_size();
};