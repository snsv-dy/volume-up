#include <cmocka.h>
#include <string.h>
#include <stdio.h>

#define UNIT_TESTING 1
#include "UserSpaceDriverImpl.h"
#include <usb_format.h>

#define UNUSED(x) (void)(x)

ssize_t __wrap_read(int __fd, void *__buf, size_t __nbytes);
ssize_t __wrap_read(int __fd, void *__buf, size_t __nbytes)
{
    check_expected_int(__fd);
    check_expected_uint(__nbytes);

    ssize_t result = mock_type(ssize_t);
    if (result <= 0)
    {
        return result;
    }

    void *readData = mock_ptr_type(void *);
    memcpy(__buf, readData, result);

    return result;
}


static void mockCallback(uint32_t action, void* userData) {
    function_called();
    check_expected_uint(action);
}

static void test_read_once_successfully(void **/*state*/)
{
    int readFile = 1;
    uint8_t expectedAction = ACTION_INC5;
    expect_int_value(__wrap_read, __fd, 0xa);
    expect_uint_value(__wrap_read, __nbytes, 1);
    will_return_int_count(__wrap_read, 1, 1);
    will_return_ptr(__wrap_read, &expectedAction);
    expect_function_call(mockCallback);
    expect_uint_value(mockCallback, action, expectedAction);

    // To exit loop
    expect_int_value(__wrap_read, __fd, 0xa);
    expect_uint_value(__wrap_read, __nbytes, 1);
    will_return_int_count(__wrap_read, 0, 1);

    // Expect callback to shutdown application after eof.
    expect_function_call(mockCallback);
    expect_uint_value(mockCallback, action, ACTION_DEVICE_SHUTDOWN);

    // TODO: mock ShutdownThreads???

    ProgramState state = {0};
    state.running = 1;
    state.callback = mockCallback;
    state.file = 0xa;


    readThread(&state);
}

static void test_read_eof(void **/*state*/)
{

}

static void test_read_error(void **/*state*/)
{

}

extern ProgramState programState;
static void test_integration_read_error_closes_driver(void **/*state*/)
{
    // int driverInit(ActionCallback callback, void* userData, sem_t* closing, sem_t* deviceReady)
    // driverInit();
}

int main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_read_once_successfully)
    };

    // int cmocka_run_group_tests(const struct CMUnitTest group_tests[],
    //                            CMFixtureFunction group_setup,
    //                            CMFixtureFunction group_teardown);
    return cmocka_run_group_tests(tests, NULL, NULL);
}