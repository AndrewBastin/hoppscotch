# hoppscotch-c

A small C library used by Hoppscotch native components.

## HTTP client (libcurl)

This package exposes a reusable HTTP transport layer implemented on top of libcurl.

### Build

```sh
cd packages/hoppscotch-c
mkdir -p build
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### API overview

The primary entry points are:

- `hc_http_global_init()` / `hc_http_global_cleanup()`
- `hc_http_client_create()` / `hc_http_client_destroy()`
- `hc_http_client_perform()`

A request describes method/URL, headers, query params, body source, timeouts, redirect policy, and TLS options.
A response contains status code, headers, metadata, and the response body either in-memory or written to a file.

### Example

```c
#include "http/http_client.h"

int main(void) {
  hc_http_global_init();

  hc_http_client_t *client = hc_http_client_create();

  hc_http_request_t req;
  hc_http_request_init(&req);
  req.method = "POST";
  req.url = "https://example.com/echo";
  hc_http_kv_list_append(&req.headers, "Content-Type", "text/plain");

  const char payload[] = "hello";
  req.body.type = HC_HTTP_BODY_SOURCE_MEMORY;
  req.body.memory.data = (const uint8_t *) payload;
  req.body.memory.size = sizeof(payload) - 1;

  req.response_body.type = HC_HTTP_BODY_SINK_MEMORY;

  hc_http_response_t res;
  hc_http_response_init(&res);

  hc_http_result_t rc = hc_http_client_perform(client, &req, &res);
  if (rc == HC_HTTP_RESULT_OK) {
    // res.status_code, res.headers, res.body.memory.data/res.body.memory.size
  }

  hc_http_response_cleanup(&res);
  hc_http_request_cleanup(&req);
  hc_http_client_destroy(client);

  hc_http_global_cleanup();
  return 0;
}
```
