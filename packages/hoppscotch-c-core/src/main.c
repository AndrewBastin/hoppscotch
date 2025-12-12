#include "hoppscotch.h"
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("Hoppscotch C Core - Version %s\n", hopp_version());
    printf("=====================================\n\n");

    hopp_error_t err = hopp_init();
    if (err != HOPP_SUCCESS) {
        fprintf(stderr, "Failed to initialize Hoppscotch library\n");
        return EXIT_FAILURE;
    }

    printf("✓ Library initialized successfully\n");
    printf("✓ libcurl version: %s\n", curl_version());
    printf("\nThis is a placeholder executable for the Hoppscotch C rewrite.\n");
    printf("Future functionality will include:\n");
    printf("  - High-performance HTTP client\n");
    printf("  - Request collection management\n");
    printf("  - Environment and variable handling\n");
    printf("  - Testing and assertions\n");
    printf("  - Authentication support\n\n");

    hopp_cleanup();
    printf("✓ Cleanup complete\n");

    return EXIT_SUCCESS;
}
