#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <signal.h>

#include <cmocka.h>

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "http/http_client.h"

typedef struct {
  int listen_fd;
  uint16_t port;
  pthread_t thread;
  volatile bool stop;
} mock_server_t;

static uint16_t get_sock_port(int fd) {
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  memset(&addr, 0, sizeof(addr));
  if (getsockname(fd, (struct sockaddr *) &addr, &len) != 0) {
    return 0;
  }
  return ntohs(addr.sin_port);
}

static bool socket_set_reuseaddr(int fd) {
  const int enable = 1;
  return setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == 0;
}

static ssize_t recv_all_until(int fd, char *buf, size_t cap, const char *needle) {
  size_t used = 0;
  while (used + 1 < cap) {
    const ssize_t n = recv(fd, buf + used, cap - used - 1, 0);
    if (n <= 0) {
      break;
    }
    used += (size_t) n;
    buf[used] = '\0';
    if (strstr(buf, needle) != NULL) {
      return (ssize_t) used;
    }
  }
  return (ssize_t) used;
}

static char *xstrdup(const char *s) {
  const size_t n = strlen(s);
  char *out = (char *) malloc(n + 1);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, s, n);
  out[n] = '\0';
  return out;
}

static const char *find_header_value(const char *request, const char *name, char *out, size_t out_len) {
  const char *p = strstr(request, "\r\n");
  if (p == NULL) {
    return NULL;
  }
  p += 2;

  while (*p) {
    const char *line_end = strstr(p, "\r\n");
    if (line_end == NULL) {
      return NULL;
    }
    if (line_end == p) {
      return NULL;
    }

    const char *colon = memchr(p, ':', (size_t) (line_end - p));
    if (colon != NULL) {
      size_t key_len = (size_t) (colon - p);
      while (key_len > 0 && (p[key_len - 1] == ' ' || p[key_len - 1] == '\t')) {
        key_len--;
      }

      if (strlen(name) == key_len && strncasecmp(p, name, key_len) == 0) {
        const char *val = colon + 1;
        while (val < line_end && (*val == ' ' || *val == '\t')) {
          val++;
        }
        size_t val_len = (size_t) (line_end - val);
        if (out_len == 0) {
          return NULL;
        }
        if (val_len >= out_len) {
          val_len = out_len - 1;
        }
        memcpy(out, val, val_len);
        out[val_len] = '\0';
        return out;
      }
    }

    p = line_end + 2;
  }

  return NULL;
}

static size_t parse_content_length(const char *headers) {
  const char *p = headers;
  while (p != NULL && *p) {
    const char *line_end = strstr(p, "\r\n");
    if (line_end == NULL) {
      break;
    }
    if (strncasecmp(p, "Content-Length:", 15) == 0) {
      const char *val = p + 15;
      while (*val == ' ' || *val == '\t') {
        val++;
      }
      return (size_t) strtoul(val, NULL, 10);
    }
    p = line_end + 2;
  }
  return 0;
}

static void send_simple_response(int fd, int status, const char *extra_headers, const uint8_t *body, size_t body_len) {
  char header_buf[1024];
  const char *status_text = status == 200 ? "OK" : (status == 302 ? "Found" : "Error");
  const int n = snprintf(
    header_buf,
    sizeof(header_buf),
    "HTTP/1.1 %d %s\r\n"
    "Connection: close\r\n"
    "Content-Length: %zu\r\n"
    "Content-Type: text/plain\r\n"
    "%s"
    "\r\n",
    status,
    status_text,
    body_len,
    extra_headers != NULL ? extra_headers : ""
  );

  (void) send(fd, header_buf, (size_t) n, 0);
  if (body_len > 0) {
    (void) send(fd, body, body_len, 0);
  }
}

static void handle_client(int fd) {
  char req_buf[16384];
  memset(req_buf, 0, sizeof(req_buf));

  (void) recv_all_until(fd, req_buf, sizeof(req_buf), "\r\n\r\n");

  const char *header_end = strstr(req_buf, "\r\n\r\n");
  if (header_end == NULL) {
    send_simple_response(fd, 400, NULL, (const uint8_t *) "bad request", 11);
    return;
  }

  const size_t headers_len = (size_t) (header_end - req_buf) + 4;
  const size_t content_len = parse_content_length(req_buf);

  size_t already = strlen(req_buf);
  size_t body_start_off = headers_len;

  uint8_t *body = NULL;
  if (content_len > 0) {
    body = (uint8_t *) malloc(content_len);
    if (body == NULL) {
      send_simple_response(fd, 500, NULL, (const uint8_t *) "oom", 3);
      return;
    }

    size_t copied = 0;
    if (already > body_start_off) {
      const size_t avail = already - body_start_off;
      const size_t n = avail < content_len ? avail : content_len;
      memcpy(body, req_buf + body_start_off, n);
      copied += n;
    }

    while (copied < content_len) {
      const ssize_t n = recv(fd, body + copied, content_len - copied, 0);
      if (n <= 0) {
        break;
      }
      copied += (size_t) n;
    }

    if (copied != content_len) {
      free(body);
      send_simple_response(fd, 400, NULL, (const uint8_t *) "incomplete body", 15);
      return;
    }
  }

  char method[16];
  char path[1024];
  memset(method, 0, sizeof(method));
  memset(path, 0, sizeof(path));

  (void) sscanf(req_buf, "%15s %1023s", method, path);

  if (strcmp(path, "/redirect") == 0) {
    send_simple_response(fd, 302, "Location: /final\r\n", NULL, 0);
    free(body);
    return;
  }

  if (strcmp(path, "/bytes") == 0) {
    uint8_t payload[1024];
    for (size_t i = 0; i < sizeof(payload); i++) {
      payload[i] = (uint8_t) (i & 0xFF);
    }
    send_simple_response(fd, 200, NULL, payload, sizeof(payload));
    free(body);
    return;
  }

  char x_test_buf[256];
  const char *x_test = find_header_value(req_buf, "X-Test", x_test_buf, sizeof(x_test_buf));

  char extra[512];
  extra[0] = '\0';
  if (x_test != NULL) {
    (void) snprintf(extra, sizeof(extra), "X-Echo-X-Test: %s\r\nX-Echo-Method: %s\r\nX-Echo-Path: %s\r\n", x_test, method, path);
  } else {
    (void) snprintf(extra, sizeof(extra), "X-Echo-Method: %s\r\nX-Echo-Path: %s\r\n", method, path);
  }

  if (body != NULL) {
    send_simple_response(fd, 200, extra, body, content_len);
  } else {
    const uint8_t empty[] = "";
    send_simple_response(fd, 200, extra, empty, 0);
  }

  free(body);
}

static void *server_thread(void *arg) {
  mock_server_t *srv = (mock_server_t *) arg;

  while (!srv->stop) {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    const int fd = accept(srv->listen_fd, (struct sockaddr *) &addr, &len);
    if (fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }

    handle_client(fd);
    close(fd);
  }

  return NULL;
}

static bool mock_server_start(mock_server_t *srv) {
  memset(srv, 0, sizeof(*srv));

  srv->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (srv->listen_fd < 0) {
    return false;
  }

  (void) socket_set_reuseaddr(srv->listen_fd);

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(0);

  if (bind(srv->listen_fd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
    close(srv->listen_fd);
    return false;
  }

  if (listen(srv->listen_fd, 16) != 0) {
    close(srv->listen_fd);
    return false;
  }

  srv->port = get_sock_port(srv->listen_fd);
  if (srv->port == 0) {
    close(srv->listen_fd);
    return false;
  }

  if (pthread_create(&srv->thread, NULL, server_thread, srv) != 0) {
    close(srv->listen_fd);
    return false;
  }

  return true;
}

static void mock_server_stop(mock_server_t *srv) {
  srv->stop = true;

  if (srv->listen_fd > 0) {
    shutdown(srv->listen_fd, SHUT_RDWR);
    close(srv->listen_fd);
  }

  (void) pthread_join(srv->thread, NULL);
}

static char *fmt_url(uint16_t port, const char *path, bool https) {
  char buf[256];
  snprintf(buf, sizeof(buf), "%s://127.0.0.1:%u%s", https ? "https" : "http", port, path);
  return xstrdup(buf);
}

static void test_get_params_and_headers_roundtrip(void **state) {
  (void) state;

  mock_server_t srv;
  assert_true(mock_server_start(&srv));

  char *url = fmt_url(srv.port, "/echo", false);

  hc_http_client_t *client = hc_http_client_create();
  assert_non_null(client);

  hc_http_request_t req;
  hc_http_request_init(&req);
  req.method = "GET";
  req.url = url;
  assert_int_equal(hc_http_kv_list_append(&req.params, "foo", "bar baz"), HC_HTTP_RESULT_OK);
  assert_int_equal(hc_http_kv_list_append(&req.headers, "X-Test", "abc123"), HC_HTTP_RESULT_OK);

  hc_http_response_t res;
  hc_http_response_init(&res);

  assert_int_equal(hc_http_client_perform(client, &req, &res), HC_HTTP_RESULT_OK);
  assert_int_equal(res.status_code, 200);

  assert_string_equal(hc_http_kv_list_get(&res.headers, "X-Echo-X-Test"), "abc123");

  const char *echo_path = hc_http_kv_list_get(&res.headers, "X-Echo-Path");
  assert_non_null(echo_path);
  assert_non_null(strstr(echo_path, "foo=bar%20baz"));

  hc_http_response_cleanup(&res);
  hc_http_request_cleanup(&req);
  hc_http_client_destroy(client);
  free(url);

  mock_server_stop(&srv);
}

static void test_post_memory_body_roundtrip(void **state) {
  (void) state;

  mock_server_t srv;
  assert_true(mock_server_start(&srv));

  char *url = fmt_url(srv.port, "/echo", false);

  hc_http_client_t *client = hc_http_client_create();
  assert_non_null(client);

  const uint8_t body[] = "hello world";

  hc_http_request_t req;
  hc_http_request_init(&req);
  req.method = "POST";
  req.url = url;
  req.body.type = HC_HTTP_BODY_SOURCE_MEMORY;
  req.body.memory.data = body;
  req.body.memory.size = sizeof(body) - 1;

  hc_http_response_t res;
  hc_http_response_init(&res);

  assert_int_equal(hc_http_client_perform(client, &req, &res), HC_HTTP_RESULT_OK);
  assert_int_equal(res.status_code, 200);
  assert_int_equal(res.body.size, sizeof(body) - 1);
  assert_memory_equal(res.body.data, body, sizeof(body) - 1);

  hc_http_response_cleanup(&res);
  hc_http_request_cleanup(&req);
  hc_http_client_destroy(client);
  free(url);

  mock_server_stop(&srv);
}

static void test_follow_redirects(void **state) {
  (void) state;

  mock_server_t srv;
  assert_true(mock_server_start(&srv));

  char *url = fmt_url(srv.port, "/redirect", false);

  hc_http_client_t *client = hc_http_client_create();
  assert_non_null(client);

  hc_http_request_t req;
  hc_http_request_init(&req);
  req.method = "GET";
  req.url = url;
  req.follow_redirects = true;

  hc_http_response_t res;
  hc_http_response_init(&res);

  assert_int_equal(hc_http_client_perform(client, &req, &res), HC_HTTP_RESULT_OK);
  assert_int_equal(res.status_code, 200);

  const char *echo_path = hc_http_kv_list_get(&res.headers, "X-Echo-Path");
  assert_non_null(echo_path);
  assert_string_equal(echo_path, "/final");

  hc_http_response_cleanup(&res);
  hc_http_request_cleanup(&req);
  hc_http_client_destroy(client);
  free(url);

  mock_server_stop(&srv);
}

static void test_response_body_to_file(void **state) {
  (void) state;

  mock_server_t srv;
  assert_true(mock_server_start(&srv));

  char *url = fmt_url(srv.port, "/bytes", false);

  char tmp[] = "/tmp/hc_http_client_XXXXXX";
  const int fd = mkstemp(tmp);
  assert_true(fd >= 0);
  close(fd);

  hc_http_client_t *client = hc_http_client_create();
  assert_non_null(client);

  hc_http_request_t req;
  hc_http_request_init(&req);
  req.method = "GET";
  req.url = url;
  req.response_body.type = HC_HTTP_BODY_SINK_FILE;
  req.response_body.file.path = tmp;
  req.response_body.file.append = false;

  hc_http_response_t res;
  hc_http_response_init(&res);

  assert_int_equal(hc_http_client_perform(client, &req, &res), HC_HTTP_RESULT_OK);
  assert_int_equal(res.status_code, 200);
  assert_int_equal(res.body.size, 0);

  FILE *fp = fopen(tmp, "rb");
  assert_non_null(fp);
  fseek(fp, 0, SEEK_END);
  const long n = ftell(fp);
  fclose(fp);

  assert_int_equal(n, 1024);

  (void) unlink(tmp);

  hc_http_response_cleanup(&res);
  hc_http_request_cleanup(&req);
  hc_http_client_destroy(client);
  free(url);

  mock_server_stop(&srv);
}

static uint16_t find_unused_port(void) {
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return 0;
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(0);

  if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
    close(fd);
    return 0;
  }

  const uint16_t port = get_sock_port(fd);
  close(fd);
  return port;
}

static void test_error_connection_refused(void **state) {
  (void) state;

  const uint16_t port = find_unused_port();
  assert_true(port != 0);

  char *url = fmt_url(port, "/echo", false);

  hc_http_client_t *client = hc_http_client_create();
  assert_non_null(client);

  hc_http_request_t req;
  hc_http_request_init(&req);
  req.method = "GET";
  req.url = url;
  req.timeout_ms = 1000;

  hc_http_response_t res;
  hc_http_response_init(&res);

  assert_int_equal(hc_http_client_perform(client, &req, &res), HC_HTTP_RESULT_ERR_CURL);
  assert_non_null(res.error_message);

  hc_http_response_cleanup(&res);
  hc_http_request_cleanup(&req);
  hc_http_client_destroy(client);
  free(url);
}

static void test_error_tls_handshake(void **state) {
  (void) state;

  mock_server_t srv;
  assert_true(mock_server_start(&srv));

  char *url = fmt_url(srv.port, "/echo", true);

  hc_http_client_t *client = hc_http_client_create();
  assert_non_null(client);

  hc_http_request_t req;
  hc_http_request_init(&req);
  req.method = "GET";
  req.url = url;
  req.timeout_ms = 1000;
  req.tls.verify_peer = false;
  req.tls.verify_host = false;

  hc_http_response_t res;
  hc_http_response_init(&res);

  assert_int_equal(hc_http_client_perform(client, &req, &res), HC_HTTP_RESULT_ERR_CURL);
  assert_non_null(res.error_message);

  hc_http_response_cleanup(&res);
  hc_http_request_cleanup(&req);
  hc_http_client_destroy(client);
  free(url);

  mock_server_stop(&srv);
}

int main(void) {
  (void) signal(SIGPIPE, SIG_IGN);

  assert_int_equal(hc_http_global_init(), HC_HTTP_RESULT_OK);

  const struct CMUnitTest tests[] = {
    cmocka_unit_test(test_get_params_and_headers_roundtrip),
    cmocka_unit_test(test_post_memory_body_roundtrip),
    cmocka_unit_test(test_follow_redirects),
    cmocka_unit_test(test_response_body_to_file),
    cmocka_unit_test(test_error_connection_refused),
    cmocka_unit_test(test_error_tls_handshake),
  };

  const int rc = cmocka_run_group_tests(tests, NULL, NULL);

  hc_http_global_cleanup();
  return rc;
}
