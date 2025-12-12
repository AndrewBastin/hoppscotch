#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * A lightweight HTTP client module built on libcurl.
 *
 * Design goals:
 * - Provide a small, reusable HTTP transport layer for higher-level Hoppscotch code.
 * - Support common Hoppscotch request features: method, URL, headers, query params,
 *   request bodies (streamed from memory or file), timeouts, redirects, and TLS options.
 * - Allow response bodies to be captured in memory or streamed into a file.
 * - Expose response metadata: status code, effective URL, content type, and headers.
 *
 * Memory ownership:
 * - `hc_http_kv_list_append()` duplicates keys/values and the list must be freed with
 *   `hc_http_kv_list_cleanup()`.
 * - Request `method` / `url` strings are not owned by this module.
 * - Request body memory pointers are not owned by this module.
 * - Response strings/buffers are owned by the response and must be released with
 *   `hc_http_response_cleanup()`.
 */

typedef enum {
  HC_HTTP_RESULT_OK = 0,
  HC_HTTP_RESULT_ERR_INVALID_ARG,
  HC_HTTP_RESULT_ERR_OOM,
  HC_HTTP_RESULT_ERR_IO,
  HC_HTTP_RESULT_ERR_CURL,
  HC_HTTP_RESULT_ERR_RESPONSE_TOO_LARGE
} hc_http_result_t;

const char *hc_http_result_str(hc_http_result_t result);

typedef struct {
  char *name;
  char *value;
} hc_http_kv_t;

typedef struct {
  hc_http_kv_t *items;
  size_t count;
  size_t capacity;
} hc_http_kv_list_t;

void hc_http_kv_list_init(hc_http_kv_list_t *list);
void hc_http_kv_list_cleanup(hc_http_kv_list_t *list);

hc_http_result_t hc_http_kv_list_append(hc_http_kv_list_t *list, const char *name, const char *value);

/** Returns the first header/param value matching `name` (case-insensitive), or NULL. */
const char *hc_http_kv_list_get(const hc_http_kv_list_t *list, const char *name);

typedef enum {
  HC_HTTP_BODY_SOURCE_NONE = 0,
  HC_HTTP_BODY_SOURCE_MEMORY,
  HC_HTTP_BODY_SOURCE_FILE
} hc_http_body_source_type_t;

typedef struct {
  hc_http_body_source_type_t type;
  union {
    struct {
      const uint8_t *data;
      size_t size;
    } memory;
    struct {
      const char *path;
    } file;
  };
} hc_http_body_source_t;

typedef enum {
  HC_HTTP_BODY_SINK_MEMORY = 0,
  HC_HTTP_BODY_SINK_FILE
} hc_http_body_sink_type_t;

typedef struct {
  hc_http_body_sink_type_t type;
  union {
    struct {
      /**
       * Optional maximum size of the in-memory response body.
       * 0 means unlimited.
       */
      size_t max_bytes;
    } memory;
    struct {
      const char *path;
      bool append;
    } file;
  };
} hc_http_body_sink_t;

typedef struct {
  bool verify_peer;
  bool verify_host;
  const char *ca_file;
  const char *ca_path;
  const char *client_cert_file;
  const char *client_key_file;
  const char *client_key_password;
} hc_http_tls_options_t;

typedef struct {
  const char *method;
  const char *url;

  hc_http_kv_list_t headers;
  hc_http_kv_list_t params;

  hc_http_body_source_t body;

  /** 0 means use libcurl defaults. */
  long timeout_ms;

  bool follow_redirects;
  long max_redirects;

  hc_http_tls_options_t tls;

  hc_http_body_sink_t response_body;
} hc_http_request_t;

void hc_http_request_init(hc_http_request_t *req);
void hc_http_request_cleanup(hc_http_request_t *req);

typedef struct {
  long status_code;
  hc_http_kv_list_t headers;

  struct {
    uint8_t *data;
    size_t size;
  } body;

  char *effective_url;
  char *content_type;
  double total_time_seconds;

  /** libcurl error code (only meaningful when result != OK) */
  int curl_code;
  char *error_message;
} hc_http_response_t;

void hc_http_response_init(hc_http_response_t *res);
void hc_http_response_cleanup(hc_http_response_t *res);

typedef struct hc_http_client hc_http_client_t;

/**
 * Initializes global libcurl state.
 *
 * Call once at program startup, before creating any clients.
 */
hc_http_result_t hc_http_global_init(void);

/**
 * Cleans up global libcurl state.
 *
 * Call once at program shutdown, after all clients are destroyed.
 */
void hc_http_global_cleanup(void);

hc_http_client_t *hc_http_client_create(void);
void hc_http_client_destroy(hc_http_client_t *client);

/**
 * Executes an HTTP request.
 *
 * - `res` must be initialized via `hc_http_response_init()`.
 * - On entry, any existing contents in `res` are released.
 * - On success, `res->status_code`, `res->headers` and response metadata are populated.
 * - If `req->response_body.type == HC_HTTP_BODY_SINK_MEMORY`, `res->body` is populated.
 */
hc_http_result_t hc_http_client_perform(
  hc_http_client_t *client,
  const hc_http_request_t *req,
  hc_http_response_t *res
);

#ifdef __cplusplus
}
#endif
