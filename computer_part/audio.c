#include <stdio.h>
#include <string.h>
#include <pulse/pulseaudio.h>
#include <pthread.h>
#include <stdatomic.h>

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

void pa_state_cb(pa_context *c, void *userdata);
void pa_sinklist_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata);
void pa_sourcelist_cb(pa_context *c, const pa_source_info *l, int eol, void *userdata);
int pa_get_devicelist(pa_devicelist_t *input, pa_devicelist_t *output, mySinkInputInfo* sinkInputs, int action, IpcStruct*);

void pa_sink_input_info_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata);



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

enum
{
    ACTION_INCORRECT = 0,
    ACTION_INC5  = 0b000001,
    ACTION_DEC5  = 0b000010,
    ACTION_INC1  = 0b000100,
    ACTION_DEC1  = 0b001000,
    ACTION_SET24 = 0b010000,
    ACTION_SET29 = 0b100000,
};

typedef struct {
    int action;
    pthread_mutex_t actionMutex;

    atomic_bool operationInProcess;
    pthread_mutex_t operationMutex;

    pthread_mutex_t mainloopMutex;

    pthread_mutex_t inputThreadSignal;
    pthread_mutex_t mainloopActionSignal;

    // exp
    // pthread_cond_t 
} IpcStruct;

void setAction(IpcStruct* ipc, int action);
int WaitForAction();
void SetOperationInProgress();
void ClearOperationInProgress();

void* inputThread(void* arg)
{
    IpcStruct* ipc = (IpcStruct*)arg;
    int running = 1;
    char actionChar = '\0';
    int action;
    while(running)
    {
        action = ACTION_INCORRECT;
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
            
            printf("[input] cs enter\n");
            setAction(ipc, action);
            printf("[input] cs exit\n");

            printf("got '%c', ", actionChar);
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
        }
    }
}

// void mainloopAquire(IpcStruct* ipc);
// void mainloopRelease(IpcStruct* ipc);

void initInputThread(pthread_t* thread, IpcStruct* ipc)
{
    assert(thread && ipc);

    atomic_init(&ipc->operationInProcess, 0);
    if (   pthread_mutex_init(&ipc->mainloopMutex, NULL)
        || pthread_mutex_init(&ipc->operationMutex, NULL)
        || pthread_mutex_init(&ipc->actionMutex, NULL)
        || pthread_mutex_init(&ipc->mainloopActionSignal, NULL)
        || pthread_mutex_init(&ipc->inputThreadSignal, NULL)
    )
    {
        printf("Failed to initialize ipc\n");
        // exit here;
        return;
    }
    

    if (pthread_create(thread, NULL, inputThread, NULL) != 0)
    {
        printf("Failed to create input thread\n");
    }
}

void setAction(IpcStruct* ipc, int action)
{
    // TODO: Po co tu mutex, jak to jest atomowe?!??!?!
    // pthread_mutex_lock(&ipc->operationMutex);
    // int operationValue = atomic_load(&ipc->operationInProcess);
    // pthread_mutex_unlock(&ipc->operationMutex);

    while (atomic_load(&ipc->operationInProcess) == 1)
    // if (operationValue == 1)
    {
        // Unlocked by mainloop.
        pthread_mutex_lock(&ipc->inputThreadSignal);
    }

    pthread_mutex_lock(&ipc->actionMutex);
    ipc->action = action;
    pthread_mutex_unlock(&ipc->actionMutex);
    // Signal the mainloop that action is ready to execute.
    pthread_mutex_unlock(&ipc->mainloopActionSignal);
}

int WaitForAction(IpcStruct* ipc)
{
    pthread_mutex_lock(&ipc->actionMutex);
    if (ipc->action != ACTION_INCORRECT)
    {
        SetOperationInProgress(ipc);
        pthread_mutex_unlock(&ipc->actionMutex);
        return ipc->action;
    }

    // Unlock mutex before long wait.
    pthread_mutex_unlock(&ipc->actionMutex);
    pthread_mutex_lock(&ipc->mainloopActionSignal);

    SetOperationInProgress(ipc);
    return ipc->action;
}
void SetOperationInProgress(IpcStruct* ipc)
{
    atomic_store(&ipc->operationInProcess, 1);
}

void ClearOperationInProgress(IpcStruct* ipc)
{
    // Unlock input thread.

    atomic_store(&ipc->operationInProcess, 0);
    pthread_mutex_unlock(&ipc->inputThreadSignal);
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
    IpcStruct ipc = {0};
    initInputThread(&inputThread, &ipc);


    if (pa_get_devicelist(pa_input_devicelist, pa_output_devicelist, sinkInputInfoList, action, &ipc) < 0) {
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

#define SMALL_STR_LEN 128

void server_info_cb(pa_context *c, const pa_server_info*i, void *userdata)
{
    if (userdata)
    {
        strncpy((char*)userdata, i->default_sink_name, SMALL_STR_LEN - 1);
    }
}

typedef struct {
    uint32_t set;
    uint32_t index;
    pa_volume_t volume;
    pa_cvolume cvolume;
} smallerSinkInfo;

void sink_info_cb(pa_context *c, const pa_sink_info *i, int eol, void *userdata)
{
    if (eol < 0) {
        printf("Failed to get sink information: %s", pa_strerror(pa_context_errno(c)));
        return;
    }

    if (eol)
    {
        return;
    }

    smallerSinkInfo* info = (smallerSinkInfo*)userdata;
    if (i == NULL)
    {
        printf("Sink with that name is null, eol: %d\n", eol);
        return;
    }

    info->index = i->index;
    info->volume = pa_cvolume_avg(&(i->volume));
    info->cvolume.channels = i->volume.channels;
    for (int index = 0; index < i->volume.channels; index++)
    {
        info->cvolume.values[index] = i->volume.values[index];
    }
    info->set = 1;
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

int pa_get_devicelist(pa_devicelist_t *input, pa_devicelist_t *output, mySinkInputInfo* sinkInputs, int action, IpcStruct* ipc) {
    // Define our pulse audio loop and connection variables
    pa_mainloop *pa_ml;
    pa_mainloop_api *pa_mlapi;
    pa_operation *pa_op = NULL;
    pa_context *pa_ctx;

    // We'll need these state variables to keep track of our requests
    // #define STATE_INIT 5
    // #define STATE_READY 0
    // #define STATE_PROCESSING 1
    // #define STATE_EXIT 2
    // #define ACTION_GET_SINK_INPUTS 3
    // #define ACTION_GET_DEFAULT_SINK 4

    #define GET_SERVER 6
    #define GET_DEFAULT_SINK_INFO 7
    #define GET_DEFAULT_SINK_VOLUME 8
    #define SET_DEFAULT_SINK_VOLUME 9
    #define EXITING 10

    // Get server params
    // Get default sink name
    // Get default sink volume
    // Set default sink volume +5%

    int state = GET_SERVER;
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

    // This function connects to the pulse server
    pa_context_connect(pa_ctx, NULL, 0, NULL);

    // This function defines a callback so the server will tell us it's state.
    // Our callback will wait for the state to be ready.  The callback will
    // modify the variable to 1 so we know when we have a connection and it's
    // ready.
    // If there's an error, the callback will set pa_ready to 2
    pa_context_set_state_callback(pa_ctx, pa_state_cb, &pa_ready);

    // Now we'll enter into an infinite loop until we get the data we receive
    // or if there's an error
    for (;;) {

        int action = WaitForAction();
        // SetOperationInProgress(); // To się wywołuje w wait for action

        // loop

        // Clear operation in progress

        // We can't do anything until PA is ready, so just iterate the mainloop
        // and continue
        if (pa_ready == 0) {
            pa_mainloop_iterate(pa_ml, 1, NULL);
            continue;
        }
        // We couldn't get a connection to the server, so exit out
        if (pa_ready == 2) {
            pa_context_disconnect(pa_ctx);
            pa_context_unref(pa_ctx);
            pa_mainloop_free(pa_ml);
            return -1;
        }
        // At this point, we're connected to the server and ready to make
        // requests
        switch (state) {
            case GET_SERVER:
                pa_op = pa_context_get_server_info(pa_ctx, server_info_cb, (void *)defaultSinkName);
                state = GET_DEFAULT_SINK_INFO;
            break;
            case GET_DEFAULT_SINK_INFO:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    printf("Default sink name: '%s'\n", defaultSinkName);
                    pa_op = pa_context_get_sink_info_by_name(pa_ctx, defaultSinkName, sink_info_cb, (void *)&defaultSinkInfo);
                    state = GET_DEFAULT_SINK_VOLUME;
                }
                break;
            case GET_DEFAULT_SINK_VOLUME:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);
                    if (defaultSinkInfo.set)
                    {
                        printf("Default sink index: %d\n", defaultSinkInfo.index);
                        setVolume(&defaultSinkInfo.cvolume, action);
                        pa_op = pa_context_set_sink_volume_by_index(pa_ctx, defaultSinkInfo.index, &defaultSinkInfo.cvolume, set_volume_success_cb, NULL);
                    }
                    else
                    {
                        printf("Default sink info not set\n");
                        pa_operation_unref(pa_op);
                        pa_context_disconnect(pa_ctx);
                        pa_context_unref(pa_ctx);
                        pa_mainloop_free(pa_ml);
                        return 0;
                    }
                    state = SET_DEFAULT_SINK_VOLUME;

                }
            case SET_DEFAULT_SINK_VOLUME:
                if (pa_operation_get_state(pa_op) == PA_OPERATION_DONE) {
                    pa_operation_unref(pa_op);

                    ClearOperationInProgress();
                }
                break;
            case EXITING:
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
                fprintf(stderr, "in state %d\n", state);
                return -1;
        }
        // Iterate the main loop and go again.  The second argument is whether
        // or not the iteration should block until something is ready to be
        // done.  Set it to zero for non-blocking.
        pa_mainloop_iterate(pa_ml, 1, NULL);
        
    }
}

void pa_sink_input_info_cb(pa_context *c, const pa_sink_input_info *i, int eol, void *userdata)
{
    // typedef struct pa_sink_input_info {
    //     uint32_t index;                      /**< Index of the sink input */
    //     const char *name;                    /**< Name of the sink input */
    //     uint32_t owner_module;               /**< Index of the module this sink input belongs to, or PA_INVALID_INDEX when it does not belong to any module. */
    //     uint32_t client;                     /**< Index of the client this sink input belongs to, or PA_INVALID_INDEX when it does not belong to any client. */
    //     uint32_t sink;                       /**< Index of the connected sink */
    //     pa_sample_spec sample_spec;          /**< The sample specification of the sink input. */
    //     pa_channel_map channel_map;          /**< Channel map */
    //     pa_cvolume volume;                   /**< The volume of this sink input. */
    //     pa_usec_t buffer_usec;               /**< Latency due to buffering in sink input, see pa_timing_info for details. */
    //     pa_usec_t sink_usec;                 /**< Latency of the sink device, see pa_timing_info for details. */
    //     const char *resample_method;         /**< The resampling method used by this sink input. */
    //     const char *driver;                  /**< Driver name */
    //     int mute;                            /**< Stream muted \since 0.9.7 */
    //     pa_proplist *proplist;               /**< Property list \since 0.9.11 */
    //     int corked;                          /**< Stream corked \since 1.0 */
    //     int has_volume;                      /**< Stream has volume. If not set, then the meaning of this struct's volume member is unspecified. \since 1.0 */
    //     int volume_writable;                 /**< The volume can be set. If not set, the volume can still change even though clients can't control the volume. \since 1.0 */
    //     pa_format_info *format;              /**< Stream format information. \since 1.0 */
    // } pa_sink_input_info;

    // pa_proplist_gets
    // pa_proplist_contains
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