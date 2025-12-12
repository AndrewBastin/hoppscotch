#ifndef HOPPSCOTCH_H
#define HOPPSCOTCH_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file hoppscotch.h
 * @brief Main header for Hoppscotch C Core Library
 * 
 * This library provides high-performance HTTP client functionality
 * for the Hoppscotch API testing ecosystem.
 * 
 * Planned Module Layout:
 * ----------------------
 * 
 * 1. HTTP Client (http_client.h)
 *    - HTTP request/response handling
 *    - Support for GET, POST, PUT, DELETE, PATCH, etc.
 *    - Custom headers and query parameters
 *    - Request/response body handling
 * 
 * 2. Authentication (auth.h)
 *    - Basic authentication
 *    - Bearer token authentication
 *    - OAuth 2.0 flows
 *    - API key management
 * 
 * 3. Request Chain (request_chain.h)
 *    - Sequential request execution
 *    - Request dependencies
 *    - Variable extraction and substitution
 * 
 * 4. Environment (environment.h)
 *    - Environment variable management
 *    - Variable scoping (global, environment, local)
 *    - Variable interpolation
 * 
 * 5. Collections (collections.h)
 *    - Collection parsing and management
 *    - Request organization
 *    - Import/export functionality
 * 
 * 6. Testing/Assertions (test.h)
 *    - Response validation
 *    - Status code assertions
 *    - Header assertions
 *    - JSON path assertions
 *    - Response time validation
 * 
 * 7. Utilities (utils.h)
 *    - String manipulation
 *    - JSON parsing/serialization
 *    - URL encoding/decoding
 *    - Base64 encoding/decoding
 * 
 * 8. Error Handling (error.h)
 *    - Error codes and messages
 *    - Error context
 *    - Error reporting
 */

#define HOPPSCOTCH_VERSION_MAJOR 0
#define HOPPSCOTCH_VERSION_MINOR 1
#define HOPPSCOTCH_VERSION_PATCH 0

typedef enum {
    HOPP_SUCCESS = 0,
    HOPP_ERROR_INVALID_ARGUMENT = 1,
    HOPP_ERROR_OUT_OF_MEMORY = 2,
    HOPP_ERROR_HTTP = 3,
    HOPP_ERROR_NETWORK = 4,
    HOPP_ERROR_TIMEOUT = 5,
    HOPP_ERROR_UNKNOWN = 99
} hopp_error_t;

/**
 * @brief Initialize the Hoppscotch library
 * @return HOPP_SUCCESS on success, error code otherwise
 */
hopp_error_t hopp_init(void);

/**
 * @brief Cleanup and free resources used by the library
 */
void hopp_cleanup(void);

/**
 * @brief Get the version string of the library
 * @return Version string in format "major.minor.patch"
 */
const char* hopp_version(void);

#ifdef __cplusplus
}
#endif

#endif /* HOPPSCOTCH_H */
