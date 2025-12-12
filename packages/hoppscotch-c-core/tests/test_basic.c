#include "hoppscotch.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    static void name(void); \
    static void name##_wrapper(void) { \
        printf("Running: %s... ", #name); \
        name(); \
        tests_passed++; \
        printf("PASS\n"); \
    } \
    static void name(void)

#define RUN_TEST(test) \
    do { \
        tests_run++; \
        test##_wrapper(); \
    } while (0)

TEST(test_version) {
    const char* version = hopp_version();
    assert(version != NULL);
    assert(strlen(version) > 0);
    (void)version;
    assert(strstr(version, "0.1.0") != NULL);
}

TEST(test_init_cleanup) {
    hopp_error_t err = hopp_init();
    assert(err == HOPP_SUCCESS);
    hopp_cleanup();
}

TEST(test_double_init) {
    hopp_error_t err1 = hopp_init();
    assert(err1 == HOPP_SUCCESS);
    
    hopp_error_t err2 = hopp_init();
    assert(err2 == HOPP_SUCCESS);
    
    hopp_cleanup();
}

int main(void) {
    printf("=================================\n");
    printf("Hoppscotch C Core - Test Suite\n");
    printf("=================================\n\n");

    RUN_TEST(test_version);
    RUN_TEST(test_init_cleanup);
    RUN_TEST(test_double_init);

    printf("\n=================================\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("=================================\n");

    return (tests_run == tests_passed) ? 0 : 1;
}
