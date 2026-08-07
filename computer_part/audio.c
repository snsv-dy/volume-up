#include <stdio.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>

#define SMALL_STR_LEN 128

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
    uint32_t initialized;
    uint32_t index;                      /**< Index of the sink input */
    char name[128];

    char mediaName[256];
    char appName[256];
    uint32_t volume;
    float percentVolume;
} mySinkInputInfo;


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
} smallerSinkInfo;

typedef struct
{
    uint32_t objIndex;
    pa_subscription_event_type_t objType;
} OperationParams;
typedef struct {
    int state;
    int initialized;
    char defaultSinkName[SMALL_STR_LEN];
    smallerSinkInfo defaultSink;

    OperationParams params;
    ItcStruct* itc;
} OperationState;


void pa_state_cb(pa_context *c, void *userdata);
void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata);
void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata);

void pa_sink_input_info_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata);
void UpdateObject(OperationState* operationState, uint32_t id, pa_subscription_event_type_t objectType);


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
// 1. subskrybuj eventy
// 2. jednocześnie pozwalaj na input
// ====================================
// później na spokojnie
// 1. Zmiana głośności sink inputa youtuba (może się resetować przy zmianie filmu, bo zmiana nie pochodzi z ui)
// 2. 


void setAction(ItcStruct* itc, int action);
int WaitForAction(ItcStruct* itc);
void OperationCompleted(ItcStruct* itc);
int pa_get_devicelist(pa_devicelist_t *input, pa_devicelist_t *output, mySinkInputInfo* sinkInputs, int action, ItcStruct*);

void* inputThread(void* arg)
{
    ItcStruct* itc = (ItcStruct*)arg;
    assert(itc);

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

            setAction(itc, action);
        }
    }
}

// void mainloopAquire(ItcStruct* itc);
// void mainloopRelease(ItcStruct* itc);

void initInputThread(pthread_t* thread, ItcStruct* itc)
{
    assert(thread && itc);
    if (
           sem_init(&(itc->actionReady), 0, 0)
        || sem_init(&(itc->actionConsumed), 0, 1)
    )
    {
        printf("Failed to initialize itc\n");
        // exit here;
        return;
    }
    

    if (pthread_create(thread, NULL, inputThread, (void*)itc) != 0)
    {
        printf("Failed to create input thread\n");
    }
}

void setAction(ItcStruct* itc, int action)
{
    // Unlocked by mainloop.
    sem_wait(&(itc->actionConsumed));
    // if (!itc->initial)
    // {
        // Pulse audio może se blokować jak nic się nie dzieje.
        pa_mainloop_wakeup(itc->mainloop);
    // }

    itc->action = action;
    
    // Signal the mainloop that action is ready to execute.
    sem_post(&itc->actionReady);
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

int main(int argc, char *argv[]) {
    int action = ACTION_INC1;
    if (argc > 1)
    {
             if (argv[1][0] == '+') { action = ACTION_INC5; }
        else if (argv[1][0] == '-') { action = ACTION_DEC5; }
        else if (argv[1][0] == 'i') { action = ACTION_INC1; }
        else if (argv[1][0] == 'd') { action = ACTION_DEC1; }
        else if (argv[1][0] == '4') { action = ACTION_SET24; }
        else if (argv[1][0] == '9') { action = ACTION_SET29; }
    }

    int ctr;

    // This is where we'll store the input device list
    pa_devicelist_t pa_input_devicelist[16];

    // This is where we'll store the output device list
    pa_devicelist_t pa_output_devicelist[16];
    mySinkInputInfo sinkInputInfoList[16];

    // pa_context_subscribe();
    pthread_t inputThread;
    ItcStruct itc = {0};
    itc.initial = 1;
    initInputThread(&inputThread, &itc);


    if (pa_get_devicelist(pa_input_devicelist, pa_output_devicelist, sinkInputInfoList, action, &itc) < 0) {
        fprintf(stderr, "failed to get device list\n");
        return 1;
    }

    for (ctr = 0; ctr < 16; ctr++) {
        if (! pa_output_devicelist[ctr].initialized) {
            break;
        }
        printf("=======[ Output Device #%d ]=======\n", ctr+1);
        printf("Description: %s\n", pa_output_devicelist[ctr].description);
        printf("Name: %s\n", pa_output_devicelist[ctr].name);
        printf("Index: %d\n", pa_output_devicelist[ctr].index);
        printf("\n");
    }

    for (ctr = 0; ctr < 16; ctr++) {
        if (! pa_input_devicelist[ctr].initialized) {
            break;
        }
        printf("=======[ Input Device #%d ]=======\n", ctr+1);
        printf("Description: %s\n", pa_input_devicelist[ctr].description);
        printf("Name: %s\n", pa_input_devicelist[ctr].name);
        printf("Index: %d\n", pa_input_devicelist[ctr].index);
        printf("\n");
    }

    for (ctr = 0; ctr < 16; ctr++) {
        if (! sinkInputInfoList[ctr].initialized) {
            break;
        }
        mySinkInputInfo* sinkInput = &(sinkInputInfoList[ctr]);

        printf("=======[ Sink Input #%d ]=======\n", ctr+1);
        printf("Name: %s\n", sinkInput->name);
        printf("Application name: %s\n", sinkInput->appName);
        printf("Media name: %s\n", sinkInput->mediaName);
        printf("Volume: %d\n", sinkInput->volume);
        printf("Volume: %f\n", sinkInput->percentVolume);
        printf("Index: %d\n", sinkInput->index);
        printf("\n");
    }

    pthread_join(inputThread, NULL);

    return 0;
}

pa_operation* GetSinkInputs(pa_context *pa_ctx, mySinkInputInfo* sinkInputs, int* state)
{
    return pa_context_get_sink_input_info_list(
        pa_ctx,
        pa_sink_input_info_cb,
        sinkInputs
    );
    // Update the state so we know what to do next
    (*state)++;
}



void server_info_cb(pa_context *c, const pa_server_info*i, void *userdata)
{
    OperationState* os = userdata;
    if (userdata)
    {
        strncpy(os->defaultSinkName, i->default_sink_name, SMALL_STR_LEN - 1);
    }
}


void sink_info_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata)
{
    if (eol < 0) {
        printf("Failed to get sink information: %s", pa_strerror(pa_context_errno(c)));
        return;
    }

    if (eol)
    {
        printf("eol\n");
        return;
    }
    else
    {
        printf("sink\n");
    }

    OperationState* os = (OperationState*)userdata;
    if (i == NULL)
    {
        printf("Sink with that name is null, eol: %d\n", eol);
        return;
    }

    os->defaultSink.index = i->index;
    os->defaultSink.volume = pa_cvolume_avg(&(i->volume));
    os->defaultSink.cvolume.channels = i->volume.channels;
    for (int index = 0; index < i->volume.channels; index++)
    {
        os->defaultSink.cvolume.values[index] = i->volume.values[index];
    }
    os->defaultSink.set = 1;
    printf("[Sink updated] sink[%d] vol: %2f\n", os->defaultSink.index, (double)pa_cvolume_avg(&os->defaultSink.cvolume) / PA_VOLUME_NORM * 100.0);
}

void set_volume_success_cb(pa_context *c, int success, void *userdata)
{
    if (!success)
    {
        printf("No success\n");
    }
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



int NextState(OperationState* s)
{
    // 1. Request operation() (ethier by subscription event, or user input)
    // 2. Store operation info/steps in OperationState.operation
    // 3. In callback call OperationComplete() to move to the next step, or finish.
    /*
        for example:

        RequestOperation(os, INC_DEF_SINK_BY_5);
        1. if !os.operation.active -> os.operation.active = 1;
           else drop it (later queue operations)
           os.operation.type = INC_DEF_SINK_BY_5;
           OperationComplete(os)
        2. from OperationComplete()
           switch (os.operation.type)
           case INC_DEF_SINK_BY_5:
           if !os.defaultSink.name -> os.operation.state = GET_SERVER
           else if !os.defaultSink.name -> ... <- we get name
           else if !os.defaultSink.id -> os.operation.state = GET_SINK_INFO <- we get info (id, volume)
           else os.operation.state = GET_SINK_VOL
        3. State read by mainloop, and doing shit.
    */

    return STATE_INCORRECT;
}

void subscribe_cb(pa_context *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata)
{
    // PA_SUBSCRIPTION_EVENT_FACILITY_MASK  <- kind of object
    // PA_SUBSCRIPTION_EVENT_TYPE_MASK      <- what happened
    int objectType = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
    int event      = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;
    
    printf("[subscribe] obj: %2x, event: %2x, id: %d\n", objectType, event, idx);
    OperationState* operation = (OperationState *)userdata;
    if (objectType == PA_SUBSCRIPTION_EVENT_SINK && event == PA_SUBSCRIPTION_EVENT_CHANGE)
    {
        UpdateObject(operation, idx, objectType);
    }
    // if (operation->state == STATE_INITALIZED)
    // {
    //     operation->;
    // }
    // prolly pass to userdata and make request in mainloop. :/
}

void context_success_cb(pa_context *c, int success, void *userdata)
{
    OperationState* operationState = (OperationState*)userdata;
    operationState->initialized = success ? 1 : -1;
    // *(int *)userdata = success;
    printf("Set subscribe success: %d\n", success);
}


void UpdateObject(OperationState* operationState, uint32_t id, pa_subscription_event_type_t objectType)
{
    // TODO: Alternatively prolly just requesting pa_get_sink_info_by_id and unlinking operation will suffice.
    //       (a will be much simpler)
    if (sem_trywait(&(operationState->itc->actionConsumed)) == -1)
    {
        printf("operation in progress. Skipping update\n");
        return;
    }

    if (operationState->state == STATE_INITIALIZED)
    {
        operationState->params.objIndex = id;
        operationState->params.objType = objectType;

        pa_mainloop_wakeup(operationState->itc->mainloop);

        operationState->itc->action = ACTION_UPDATE_OBJ;
        
        // Signal the mainloop that action is ready to execute.
        sem_post(&operationState->itc->actionReady);
    }
    else
    {
        printf("operation in progress. Skipping update1\n");
    }
}

// void pa_source_info_cb(pa_context *c, const pa_source_info *i, int eol, void *userdata);


int pa_get_devicelist(pa_devicelist_t *input, pa_devicelist_t *output, mySinkInputInfo* sinkInputs, int action, ItcStruct* itc) {
    // Define our pulse audio loop and connection variables
    pa_mainloop *pa_ml;
    pa_mainloop_api *pa_mlapi;
    pa_operation *pa_op = NULL;
    pa_context *pa_ctx;
    OperationState operationState = {0};
    operationState.state = STATE_INCORRECT;
    operationState.itc = itc;

    // int state = STATE_INIT;
    int pa_ready = 0;

    char defaultSinkName[128] = {0};
    smallerSinkInfo defaultSinkInfo = {0};

    // Initialize our device lists
    memset(input, 0, sizeof(pa_devicelist_t) * 16);
    memset(output, 0, sizeof(pa_devicelist_t) * 16);
    memset(sinkInputs, 0, sizeof(mySinkInputInfo) * 16);

    // Create a mainloop API and connection to the default server
    pa_ml = pa_mainloop_new();
    pa_mlapi = pa_mainloop_get_api(pa_ml);
    pa_ctx = pa_context_new(pa_mlapi, "test");
    itc->mainloop = pa_ml;

    // This function connects to the pulse server
    pa_context_connect(pa_ctx, NULL, 0, NULL);

    // This function defines a callback so the server will tell us it's state.
    // Our callback will wait for the state to be ready.  The callback will
    // modify the variable to 1 so we know when we have a connection and it's
    // ready.
    // If there's an error, the callback will set pa_ready to 2
    pa_context_set_state_callback(pa_ctx, pa_state_cb, &pa_ready);
    pa_context_set_subscribe_callback(pa_ctx, subscribe_cb, &operationState);

    // Now we'll enter into an infinite loop until we get the data we receive
    // or if there's an error
    // pa_mainloop_run(pa_ml);
    // return;

    action = ACTION_INCORRECT;
    for (;;) {

        // Iterate the main loop and go again.  The second argument is whether
        // or not the iteration should block until something is ready to be
        // done.  Set it to zero for non-blocking.
        pa_mainloop_iterate(pa_ml, 1, NULL);


        // We can't do anything until PA is ready, so just iterate the mainloop
        // and continue
        if (pa_ready == 0) {
            // pa_mainloop_iterate(pa_ml, 1, NULL);
            continue;
        }
        // We couldn't get a connection to the server, so exit out
        if (pa_ready == 2) {
            pa_context_disconnect(pa_ctx);
            pa_context_unref(pa_ctx);
            pa_mainloop_free(pa_ml);
            return -1;
        }
        else
        {
            printf("pa_ready %d\n", pa_ready);
        }

        if ((operationState.state != STATE_INCORRECT) && action == ACTION_INCORRECT)
        {
            printf("Waiting for action\n");
            action = WaitForAction(itc);
            if (action == ACTION_INCORRECT)
            {
                printf("action incorrect\n");
                continue;
            }
            printf("action aquired: %x\n", action);
        }
        
        printf("got state: %d\n", operationState.state);
        switch (operationState.state) {
            case STATE_INCORRECT: // Need to set up stuff
                if (isOperation(&pa_op))
                {
                    break;
                }
                
                // Setting subscription.
                if (!operationState.initialized)
                {
                    // initialize stuff
                    pa_op = pa_context_subscribe(
                        pa_ctx, 
                        PA_SUBSCRIPTION_MASK_SINK_INPUT
                        | PA_SUBSCRIPTION_MASK_SINK,
                        context_success_cb,
                        (void*)&operationState);
                    break;
                }

                // Getting default sink info
                if (operationState.defaultSinkName[0] == '\0')
                {
                    pa_op = pa_context_get_server_info(pa_ctx, server_info_cb, (void *)&operationState);
                    break;
                }

                if (!operationState.defaultSink.set)
                {
                    printf("Default sink name: '%s'\n", operationState.defaultSinkName);
                    pa_op = pa_context_get_sink_info_by_name(pa_ctx, operationState.defaultSinkName, sink_info_cb, (void *)&operationState);
                    break;
                }

                // TODO: Get all sink inputs.

                printf("state initialized\n");
                operationState.state = STATE_INITIALIZED;
                break;
            case STATE_INITIALIZED:
                if (isOperation(&pa_op))
                {
                    break;
                }

                if (action & ACTION_VOLUME_SET_MASK)
                {
                    if (operationState.defaultSink.set)
                    {
                        printf("Default sink index: %d\n", operationState.defaultSink.index);
                        setVolume(&operationState.defaultSink.cvolume, action);
                        pa_op = pa_context_set_sink_volume_by_index(
                            pa_ctx, 
                            operationState.defaultSink.index, 
                            &operationState.defaultSink.cvolume, 
                            set_volume_success_cb, 
                            &operationState);
                        operationState.state = STATE_SETTING_VOLUME;
                        break;
                    }
                    else
                    {
                        printf("Default sink info not set\n");
                        // state -> INCORRECT?/UPDATING_OBJECTS
                    }
                }
                else if (action == ACTION_UPDATE_OBJ)
                {
                    pa_context_get_sink_info_by_index(
                        pa_ctx,
                        operationState.params.objIndex,
                        sink_info_cb,
                        &operationState
                    );
                    operationState.state = STATE_UPDATING_OBJECTS;
                    break;
                    // operationState->params.objIndex = id;
                    // operationState->params.objType = objectType;
                    // Update sink.
                }
            break;
            case STATE_UPDATING_OBJECTS:
                // if (isOperation(&pa_op))
                // {
                //     break;
                // }
                // operationState.
            case STATE_SETTING_VOLUME:
                    if (isOperation(&pa_op))
                    {
                        break;
                    }
                    // pa_op = NULL;
                    
                    // TODO: botch, just for thread input testing.
                    //       rework when subscriptions.
                    // STATE_INITstate = GET_DEFAULT_SINK_VOLUME;
                    
                    
                    operationState.params.objIndex = 0;
                    operationState.params.objType = 0;
                    operationState.state = STATE_INITIALIZED;
                    action = ACTION_INCORRECT;
                    OperationCompleted(itc);
                break;
            case STATE_SHUTTING_DOWN:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    pa_context_disconnect(pa_ctx);
                    pa_context_unref(pa_ctx);
                    pa_mainloop_free(pa_ml);
                    return 0;
                }
                break;

            default:
                // We should never see this state
                fprintf(stderr, "in state %d\n", operationState.state);
                // return -1;
        }
    }
}

void pa_sink_input_info_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata)
{
    mySinkInputInfo *sinkInputInfoList = userdata;
    int ctr = 0;

    if (eol > 0) {
        return;
    }

    for (ctr = 0; ctr < 16; ctr++) {
        if (!sinkInputInfoList[ctr].initialized) {
            mySinkInputInfo* sink = &(sinkInputInfoList[ctr]);
            sink->index = i->index;
            sink->volume = pa_cvolume_avg(&i->volume);
            sink->percentVolume = ((double)sink->volume / PA_VOLUME_NORM) * 100;
            // char mediaName[256];
            // char appName[256];

            strncpy(sink->name, i->name, 127);

            if (pa_proplist_contains(i->proplist, "media.name") == 1)
            {
                const char* propStr = pa_proplist_gets(i->proplist, "media.name");
                strncpy(sink->mediaName, propStr, 255);
            }
            if (pa_proplist_contains(i->proplist, "application.name") == 1)
            {
                const char* propStr = pa_proplist_gets(i->proplist, "application.name");
                strncpy(sink->appName, propStr, 255);
            }
            sink->initialized = 1;
            break;
        }
    }
}

// This callback gets called when our context changes state.  We really only
// care about when it's ready or if it has failed
void pa_state_cb(pa_context *c, void *userdata) {
        pa_context_state_t state;
        int *pa_ready = userdata;

        state = pa_context_get_state(c);
        switch  (state) {
                // There are just here for reference
                case PA_CONTEXT_UNCONNECTED:
                case PA_CONTEXT_CONNECTING:
                case PA_CONTEXT_AUTHORIZING:
                case PA_CONTEXT_SETTING_NAME:
                default:
                        break;
                case PA_CONTEXT_FAILED:
                case PA_CONTEXT_TERMINATED:
                        *pa_ready = 2;
                        break;
                case PA_CONTEXT_READY:
                        *pa_ready = 1;
                        break;
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