#include "http_client.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <curl/curl.h>

struct hc_http_client {
  CURL *curl;
  char error_buf[CURL_ERROR_SIZE];
};

static char *hc_strdup(const char *s) {
  if (s == NULL) {
    return NULL;
  }
  const size_t n = strlen(s);
  char *out = (char *) malloc(n + 1);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, s, n);
  out[n] = '\0';
  return out;
}

static char *hc_strndup(const char *s, size_t n) {
  char *out = (char *) malloc(n + 1);
  if (out == NULL) {
    return NULL;
  }
  memcpy(out, s, n);
  out[n] = '\0';
  return out;
}

const char *hc_http_result_str(hc_http_result_t result) {
  switch (result) {
    case HC_HTTP_RESULT_OK:
      return "OK";
    case HC_HTTP_RESULT_ERR_INVALID_ARG:
      return "Invalid argument";
    case HC_HTTP_RESULT_ERR_OOM:
      return "Out of memory";
    case HC_HTTP_RESULT_ERR_IO:
      return "I/O error";
    case HC_HTTP_RESULT_ERR_CURL:
      return "libcurl error";
    case HC_HTTP_RESULT_ERR_RESPONSE_TOO_LARGE:
      return "Response too large";
    default:
      return "Unknown";
  }
}

static int hc_strcasecmp(const char *a, const char *b) {
  if (a == NULL || b == NULL) {
    return (a == b) ? 0 : (a == NULL ? -1 : 1);
  }
  while (*a && *b) {
    int da = tolower((unsigned char) *a);
    int db = tolower((unsigned char) *b);
    if (da != db) {
      return da - db;
    }
    a++;
    b++;
  }
  return tolower((unsigned char) *a) - tolower((unsigned char) *b);
}

void hc_http_kv_list_init(hc_http_kv_list_t *list) {
  if (list == NULL) {
    return;
  }
  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}

void hc_http_kv_list_cleanup(hc_http_kv_list_t *list) {
  if (list == NULL) {
    return;
  }
  for (size_t i = 0; i < list->count; i++) {
    free(list->items[i].name);
    free(list->items[i].value);
  }
  free(list->items);
  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}

hc_http_result_t hc_http_kv_list_append(hc_http_kv_list_t *list, const char *name, const char *value) {
  if (list == NULL || name == NULL || value == NULL) {
    return HC_HTTP_RESULT_ERR_INVALID_ARG;
  }

  if (list->count == list->capacity) {
    const size_t next_cap = list->capacity == 0 ? 8 : list->capacity * 2;
    hc_http_kv_t *next = (hc_http_kv_t *) realloc(list->items, next_cap * sizeof(hc_http_kv_t));
    if (next == NULL) {
      return HC_HTTP_RESULT_ERR_OOM;
    }
    list->items = next;
    list->capacity = next_cap;
  }

  char *n = hc_strdup(name);
  char *v = hc_strdup(value);
  if (n == NULL || v == NULL) {
    free(n);
    free(v);
    return HC_HTTP_RESULT_ERR_OOM;
  }

  list->items[list->count].name = n;
  list->items[list->count].value = v;
  list->count++;

  return HC_HTTP_RESULT_OK;
}

const char *hc_http_kv_list_get(const hc_http_kv_list_t *list, const char *name) {
  if (list == NULL || name == NULL) {
    return NULL;
  }
  for (size_t i = 0; i < list->count; i++) {
    if (hc_strcasecmp(list->items[i].name, name) == 0) {
      return list->items[i].value;
    }
  }
  return NULL;
}

void hc_http_request_init(hc_http_request_t *req) {
  if (req == NULL) {
    return;
  }
  req->method = "GET";
  req->url = NULL;

  hc_http_kv_list_init(&req->headers);
  hc_http_kv_list_init(&req->params);

  req->body.type = HC_HTTP_BODY_SOURCE_NONE;

  req->timeout_ms = 0;
  req->follow_redirects = true;
  req->max_redirects = 10;

  req->tls.verify_peer = true;
  req->tls.verify_host = true;
  req->tls.ca_file = NULL;
  req->tls.ca_path = NULL;
  req->tls.client_cert_file = NULL;
  req->tls.client_key_file = NULL;
  req->tls.client_key_password = NULL;

  req->response_body.type = HC_HTTP_BODY_SINK_MEMORY;
  req->response_body.memory.max_bytes = 0;
}

void hc_http_request_cleanup(hc_http_request_t *req) {
  if (req == NULL) {
    return;
  }
  hc_http_kv_list_cleanup(&req->headers);
  hc_http_kv_list_cleanup(&req->params);
}

void hc_http_response_init(hc_http_response_t *res) {
  if (res == NULL) {
    return;
  }
  res->status_code = 0;
  hc_http_kv_list_init(&res->headers);

  res->body.data = NULL;
  res->body.size = 0;

  res->effective_url = NULL;
  res->content_type = NULL;
  res->total_time_seconds = 0.0;

  res->curl_code = 0;
  res->error_message = NULL;
}

void hc_http_response_cleanup(hc_http_response_t *res) {
  if (res == NULL) {
    return;
  }
  hc_http_kv_list_cleanup(&res->headers);
  free(res->body.data);
  res->body.data = NULL;
  res->body.size = 0;

  free(res->effective_url);
  free(res->content_type);
  free(res->error_message);

  res->effective_url = NULL;
  res->content_type = NULL;
  res->error_message = NULL;
  res->curl_code = 0;
  res->status_code = 0;
  res->total_time_seconds = 0.0;
}

hc_http_result_t hc_http_global_init(void) {
  const CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
  if (rc != CURLE_OK) {
    return HC_HTTP_RESULT_ERR_CURL;
  }
  return HC_HTTP_RESULT_OK;
}

void hc_http_global_cleanup(void) {
  curl_global_cleanup();
}

hc_http_client_t *hc_http_client_create(void) {
  hc_http_client_t *client = (hc_http_client_t *) calloc(1, sizeof(hc_http_client_t));
  if (client == NULL) {
    return NULL;
  }

  client->curl = curl_easy_init();
  if (client->curl == NULL) {
    free(client);
    return NULL;
  }

  return client;
}

void hc_http_client_destroy(hc_http_client_t *client) {
  if (client == NULL) {
    return;
  }
  if (client->curl != NULL) {
    curl_easy_cleanup(client->curl);
  }
  free(client);
}

typedef struct {
  hc_http_body_source_t src;
  size_t offset;
  FILE *fp;
} hc_body_reader_t;

static size_t hc_read_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
  hc_body_reader_t *r = (hc_body_reader_t *) userdata;
  const size_t max = size * nitems;

  if (r == NULL || max == 0) {
    return 0;
  }

  if (r->src.type == HC_HTTP_BODY_SOURCE_MEMORY) {
    const size_t remaining = r->src.memory.size > r->offset ? (r->src.memory.size - r->offset) : 0;
    const size_t n = remaining < max ? remaining : max;
    if (n > 0) {
      memcpy(buffer, r->src.memory.data + r->offset, n);
      r->offset += n;
    }
    return n;
  }

  if (r->src.type == HC_HTTP_BODY_SOURCE_FILE) {
    if (r->fp == NULL) {
      return CURL_READFUNC_ABORT;
    }
    return fread(buffer, 1, max, r->fp);
  }

  return 0;
}

typedef struct {
  uint8_t *data;
  size_t size;
  size_t capacity;
  size_t max_bytes;
  bool too_large;
} hc_mem_sink_t;

static hc_http_result_t hc_mem_sink_reserve(hc_mem_sink_t *sink, size_t additional) {
  if (sink == NULL) {
    return HC_HTTP_RESULT_ERR_INVALID_ARG;
  }

  if (sink->max_bytes > 0 && sink->size + additional > sink->max_bytes) {
    sink->too_large = true;
    return HC_HTTP_RESULT_ERR_RESPONSE_TOO_LARGE;
  }

  const size_t required = sink->size + additional;
  if (required <= sink->capacity) {
    return HC_HTTP_RESULT_OK;
  }

  size_t next = sink->capacity == 0 ? 4096 : sink->capacity;
  while (next < required) {
    next *= 2;
  }

  uint8_t *p = (uint8_t *) realloc(sink->data, next);
  if (p == NULL) {
    return HC_HTTP_RESULT_ERR_OOM;
  }

  sink->data = p;
  sink->capacity = next;
  return HC_HTTP_RESULT_OK;
}

static size_t hc_write_mem_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
  hc_mem_sink_t *sink = (hc_mem_sink_t *) userdata;
  const size_t n = size * nmemb;

  if (sink == NULL || n == 0) {
    return 0;
  }

  if (hc_mem_sink_reserve(sink, n) != HC_HTTP_RESULT_OK) {
    return 0;
  }

  memcpy(sink->data + sink->size, ptr, n);
  sink->size += n;
  return n;
}

typedef struct {
  FILE *fp;
} hc_file_sink_t;

static size_t hc_write_file_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
  hc_file_sink_t *sink = (hc_file_sink_t *) userdata;
  const size_t n = size * nmemb;

  if (sink == NULL || sink->fp == NULL || n == 0) {
    return 0;
  }

  const size_t written = fwrite(ptr, 1, n, sink->fp);
  return written;
}

static hc_http_result_t hc_append_url_param(
  CURL *curl,
  char **url,
  size_t *len,
  size_t *cap,
  const char *name,
  const char *value,
  bool first
) {
  char *enc_name = curl_easy_escape(curl, name, 0);
  char *enc_value = curl_easy_escape(curl, value, 0);
  if (enc_name == NULL || enc_value == NULL) {
    curl_free(enc_name);
    curl_free(enc_value);
    return HC_HTTP_RESULT_ERR_OOM;
  }

  const char sep = first ? '?' : '&';
  const size_t extra = 1 + strlen(enc_name) + 1 + strlen(enc_value);

  if (*len + extra + 1 > *cap) {
    size_t next = *cap == 0 ? 256 : *cap;
    while (*len + extra + 1 > next) {
      next *= 2;
    }
    char *p = (char *) realloc(*url, next);
    if (p == NULL) {
      curl_free(enc_name);
      curl_free(enc_value);
      return HC_HTTP_RESULT_ERR_OOM;
    }
    *url = p;
    *cap = next;
  }

  (*url)[(*len)++] = sep;
  memcpy(*url + *len, enc_name, strlen(enc_name));
  *len += strlen(enc_name);
  (*url)[(*len)++] = '=';
  memcpy(*url + *len, enc_value, strlen(enc_value));
  *len += strlen(enc_value);
  (*url)[*len] = '\0';

  curl_free(enc_name);
  curl_free(enc_value);

  return HC_HTTP_RESULT_OK;
}

static hc_http_result_t hc_build_url(CURL *curl, const hc_http_request_t *req, char **out_url) {
  if (curl == NULL || req == NULL || req->url == NULL || out_url == NULL) {
    return HC_HTTP_RESULT_ERR_INVALID_ARG;
  }

  char *url = hc_strdup(req->url);
  if (url == NULL) {
    return HC_HTTP_RESULT_ERR_OOM;
  }

  size_t len = strlen(url);
  size_t cap = len + 1;

  bool first_param = (strchr(url, '?') == NULL);

  for (size_t i = 0; i < req->params.count; i++) {
    hc_http_result_t rc = hc_append_url_param(
      curl,
      &url,
      &len,
      &cap,
      req->params.items[i].name,
      req->params.items[i].value,
      first_param
    );
    if (rc != HC_HTTP_RESULT_OK) {
      free(url);
      return rc;
    }
    first_param = false;
  }

  *out_url = url;
  return HC_HTTP_RESULT_OK;
}

static size_t hc_header_cb(char *buffer, size_t size, size_t nitems, void *userdata) {
  hc_http_response_t *res = (hc_http_response_t *) userdata;
  const size_t n = size * nitems;

  if (res == NULL || buffer == NULL || n == 0) {
    return 0;
  }

  const char *line = buffer;
  const char *end = buffer + n;

  while (end > line && (end[-1] == '\r' || end[-1] == '\n')) {
    end--;
  }

  const char *colon = memchr(line, ':', (size_t) (end - line));
  if (colon == NULL) {
    return n;
  }

  const char *name_end = colon;
  while (name_end > line && isspace((unsigned char) name_end[-1])) {
    name_end--;
  }

  const char *value_start = colon + 1;
  while (value_start < end && isspace((unsigned char) *value_start)) {
    value_start++;
  }

  char *name = hc_strndup(line, (size_t) (name_end - line));
  char *value = hc_strndup(value_start, (size_t) (end - value_start));
  if (name == NULL || value == NULL) {
    free(name);
    free(value);
    return 0;
  }

  hc_http_result_t rc = hc_http_kv_list_append(&res->headers, name, value);
  free(name);
  free(value);

  if (rc != HC_HTTP_RESULT_OK) {
    return 0;
  }

  return n;
}

static void hc_set_error(hc_http_response_t *res, int curl_code, const char *msg) {
  if (res == NULL) {
    return;
  }
  res->curl_code = curl_code;
  free(res->error_message);
  res->error_message = hc_strdup(msg);
}

hc_http_result_t hc_http_client_perform(
  hc_http_client_t *client,
  const hc_http_request_t *req,
  hc_http_response_t *res
) {
  if (client == NULL || client->curl == NULL || req == NULL || res == NULL) {
    return HC_HTTP_RESULT_ERR_INVALID_ARG;
  }
  if (req->url == NULL || req->method == NULL) {
    return HC_HTTP_RESULT_ERR_INVALID_ARG;
  }

  hc_http_response_cleanup(res);
  hc_http_response_init(res);

  curl_easy_reset(client->curl);
  memset(client->error_buf, 0, sizeof(client->error_buf));
  curl_easy_setopt(client->curl, CURLOPT_ERRORBUFFER, client->error_buf);

  char *full_url = NULL;
  hc_http_result_t rc = hc_build_url(client->curl, req, &full_url);
  if (rc != HC_HTTP_RESULT_OK) {
    return rc;
  }

  curl_easy_setopt(client->curl, CURLOPT_URL, full_url);

  curl_easy_setopt(client->curl, CURLOPT_FOLLOWLOCATION, req->follow_redirects ? 1L : 0L);
  if (req->max_redirects > 0) {
    curl_easy_setopt(client->curl, CURLOPT_MAXREDIRS, req->max_redirects);
  }

  if (req->timeout_ms > 0) {
    curl_easy_setopt(client->curl, CURLOPT_TIMEOUT_MS, req->timeout_ms);
  }

  curl_easy_setopt(client->curl, CURLOPT_SSL_VERIFYPEER, req->tls.verify_peer ? 1L : 0L);
  curl_easy_setopt(client->curl, CURLOPT_SSL_VERIFYHOST, req->tls.verify_host ? 2L : 0L);

  if (req->tls.ca_file != NULL) {
    curl_easy_setopt(client->curl, CURLOPT_CAINFO, req->tls.ca_file);
  }
  if (req->tls.ca_path != NULL) {
    curl_easy_setopt(client->curl, CURLOPT_CAPATH, req->tls.ca_path);
  }
  if (req->tls.client_cert_file != NULL) {
    curl_easy_setopt(client->curl, CURLOPT_SSLCERT, req->tls.client_cert_file);
  }
  if (req->tls.client_key_file != NULL) {
    curl_easy_setopt(client->curl, CURLOPT_SSLKEY, req->tls.client_key_file);
  }
  if (req->tls.client_key_password != NULL) {
    curl_easy_setopt(client->curl, CURLOPT_KEYPASSWD, req->tls.client_key_password);
  }

  struct curl_slist *header_list = NULL;
  for (size_t i = 0; i < req->headers.count; i++) {
    const char *name = req->headers.items[i].name;
    const char *value = req->headers.items[i].value;

    const size_t line_len = strlen(name) + 2 + strlen(value);
    char *line = (char *) malloc(line_len + 1);
    if (line == NULL) {
      curl_slist_free_all(header_list);
      free(full_url);
      return HC_HTTP_RESULT_ERR_OOM;
    }

    snprintf(line, line_len + 1, "%s: %s", name, value);

    struct curl_slist *next = curl_slist_append(header_list, line);
    free(line);

    if (next == NULL) {
      curl_slist_free_all(header_list);
      free(full_url);
      return HC_HTTP_RESULT_ERR_OOM;
    }

    header_list = next;
  }

  if (header_list != NULL) {
    curl_easy_setopt(client->curl, CURLOPT_HTTPHEADER, header_list);
  }

  hc_mem_sink_t mem_sink;
  memset(&mem_sink, 0, sizeof(mem_sink));
  mem_sink.max_bytes = (req->response_body.type == HC_HTTP_BODY_SINK_MEMORY) ? req->response_body.memory.max_bytes : 0;

  hc_file_sink_t file_sink;
  memset(&file_sink, 0, sizeof(file_sink));

  FILE *response_fp = NULL;
  if (req->response_body.type == HC_HTTP_BODY_SINK_FILE) {
    const char *mode = req->response_body.file.append ? "ab" : "wb";
    response_fp = fopen(req->response_body.file.path, mode);
    if (response_fp == NULL) {
      curl_slist_free_all(header_list);
      free(full_url);
      return HC_HTTP_RESULT_ERR_IO;
    }
    file_sink.fp = response_fp;
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, hc_write_file_cb);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, &file_sink);
  } else {
    curl_easy_setopt(client->curl, CURLOPT_WRITEFUNCTION, hc_write_mem_cb);
    curl_easy_setopt(client->curl, CURLOPT_WRITEDATA, &mem_sink);
  }

  curl_easy_setopt(client->curl, CURLOPT_HEADERFUNCTION, hc_header_cb);
  curl_easy_setopt(client->curl, CURLOPT_HEADERDATA, res);

  hc_body_reader_t reader;
  memset(&reader, 0, sizeof(reader));
  reader.src = req->body;

  FILE *request_fp = NULL;
  curl_off_t body_len = 0;

  if (req->body.type == HC_HTTP_BODY_SOURCE_MEMORY) {
    body_len = (curl_off_t) req->body.memory.size;
  } else if (req->body.type == HC_HTTP_BODY_SOURCE_FILE) {
    request_fp = fopen(req->body.file.path, "rb");
    if (request_fp == NULL) {
      if (response_fp != NULL) {
        fclose(response_fp);
      }
      curl_slist_free_all(header_list);
      free(full_url);
      return HC_HTTP_RESULT_ERR_IO;
    }
    reader.fp = request_fp;

    struct stat st;
    if (fstat(fileno(request_fp), &st) == 0) {
      body_len = (curl_off_t) st.st_size;
    } else {
      body_len = (curl_off_t) -1;
    }
  }

  const bool has_body = req->body.type != HC_HTTP_BODY_SOURCE_NONE;

  if (hc_strcasecmp(req->method, "GET") == 0) {
    curl_easy_setopt(client->curl, CURLOPT_HTTPGET, 1L);
  } else if (hc_strcasecmp(req->method, "HEAD") == 0) {
    curl_easy_setopt(client->curl, CURLOPT_NOBODY, 1L);
  } else if (hc_strcasecmp(req->method, "POST") == 0) {
    curl_easy_setopt(client->curl, CURLOPT_POST, 1L);
    if (has_body) {
      curl_easy_setopt(client->curl, CURLOPT_READFUNCTION, hc_read_cb);
      curl_easy_setopt(client->curl, CURLOPT_READDATA, &reader);
      curl_easy_setopt(client->curl, CURLOPT_POSTFIELDSIZE_LARGE, body_len);
    }
  } else if (hc_strcasecmp(req->method, "PUT") == 0) {
    curl_easy_setopt(client->curl, CURLOPT_CUSTOMREQUEST, "PUT");
    if (has_body) {
      curl_easy_setopt(client->curl, CURLOPT_UPLOAD, 1L);
      curl_easy_setopt(client->curl, CURLOPT_READFUNCTION, hc_read_cb);
      curl_easy_setopt(client->curl, CURLOPT_READDATA, &reader);
      if (body_len >= 0) {
        curl_easy_setopt(client->curl, CURLOPT_INFILESIZE_LARGE, body_len);
      }
    }
  } else {
    curl_easy_setopt(client->curl, CURLOPT_CUSTOMREQUEST, req->method);
    if (has_body) {
      curl_easy_setopt(client->curl, CURLOPT_POST, 1L);
      curl_easy_setopt(client->curl, CURLOPT_READFUNCTION, hc_read_cb);
      curl_easy_setopt(client->curl, CURLOPT_READDATA, &reader);
      curl_easy_setopt(client->curl, CURLOPT_POSTFIELDSIZE_LARGE, body_len);
    }
  }

  const CURLcode curl_rc = curl_easy_perform(client->curl);

  if (request_fp != NULL) {
    fclose(request_fp);
  }
  if (response_fp != NULL) {
    fclose(response_fp);
  }
  curl_slist_free_all(header_list);

  if (curl_rc != CURLE_OK) {
    if (mem_sink.too_large) {
      free(mem_sink.data);
      free(full_url);
      return HC_HTTP_RESULT_ERR_RESPONSE_TOO_LARGE;
    }

    res->curl_code = (int) curl_rc;
    if (client->error_buf[0] != '\0') {
      hc_set_error(res, (int) curl_rc, client->error_buf);
    } else {
      hc_set_error(res, (int) curl_rc, curl_easy_strerror(curl_rc));
    }

    free(mem_sink.data);
    free(full_url);
    return HC_HTTP_RESULT_ERR_CURL;
  }

  (void) curl_easy_getinfo(client->curl, CURLINFO_RESPONSE_CODE, &res->status_code);

  char *ct = NULL;
  if (curl_easy_getinfo(client->curl, CURLINFO_CONTENT_TYPE, &ct) == CURLE_OK && ct != NULL) {
    res->content_type = hc_strdup(ct);
  }

  char *eff = NULL;
  if (curl_easy_getinfo(client->curl, CURLINFO_EFFECTIVE_URL, &eff) == CURLE_OK && eff != NULL) {
    res->effective_url = hc_strdup(eff);
  }

  double total_time = 0.0;
  if (curl_easy_getinfo(client->curl, CURLINFO_TOTAL_TIME, &total_time) == CURLE_OK) {
    res->total_time_seconds = total_time;
  }

  if (req->response_body.type == HC_HTTP_BODY_SINK_MEMORY) {
    res->body.data = mem_sink.data;
    res->body.size = mem_sink.size;
  } else {
    free(mem_sink.data);
  }

  free(full_url);
  return HC_HTTP_RESULT_OK;
}
