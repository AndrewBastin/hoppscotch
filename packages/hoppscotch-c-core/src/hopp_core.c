#include "hoppscotch.h"
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>

static int initialized = 0;

hopp_error_t hopp_init(void) {
    if (initialized) {
        return HOPP_SUCCESS;
    }

    CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (res != CURLE_OK) {
        fprintf(stderr, "Failed to initialize libcurl: %s\n", curl_easy_strerror(res));
        return HOPP_ERROR_UNKNOWN;
    }

    initialized = 1;
    return HOPP_SUCCESS;
}

void hopp_cleanup(void) {
    if (initialized) {
        curl_global_cleanup();
        initialized = 0;
    }
}

const char* hopp_version(void) {
    static char version[32];
    snprintf(version, sizeof(version), "%d.%d.%d",
             HOPPSCOTCH_VERSION_MAJOR,
             HOPPSCOTCH_VERSION_MINOR,
             HOPPSCOTCH_VERSION_PATCH);
    return version;
}
