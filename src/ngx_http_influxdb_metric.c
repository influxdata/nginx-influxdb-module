#include <arpa/inet.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <ngx_alloc.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "ngx_http_influxdb_metric.h"

static char *method_to_name(ngx_uint_t method) {
  switch (method) {
    case NGX_HTTP_UNKNOWN:
      return "UNKNOWN";
    case NGX_HTTP_GET:
      return "GET";
    case NGX_HTTP_HEAD:
      return "HEAD";
    case NGX_HTTP_POST:
      return "POST";
    case NGX_HTTP_PUT:
      return "PUT";
    case NGX_HTTP_DELETE:
      return "DELETE";
    case NGX_HTTP_MKCOL:
      return "MKCOL";
    case NGX_HTTP_COPY:
      return "COPY";
    case NGX_HTTP_MOVE:
      return "MOVE";
    case NGX_HTTP_OPTIONS:
      return "OPTIONS";
    case NGX_HTTP_PROPFIND:
      return "PROPFIND";
    case NGX_HTTP_PROPPATCH:
      return "PROPPATCH";
    case NGX_HTTP_LOCK:
      return "LOCK";
    case NGX_HTTP_UNLOCK:
      return "UNLOCK";
    case NGX_HTTP_PATCH:
      return "PATCH";
    case NGX_HTTP_TRACE:
      return "TRACE";
    default:
      return NULL;
  }
}

static ngx_buf_t *create_temp_char_buf(ngx_pool_t *pool, size_t size) {
  ngx_buf_t *b;

  b = ngx_calloc_buf(pool);
  if (b == NULL) {
    return NULL;
  }

  b->start = (u_char *)ngx_pcalloc(pool, size);
  if (b->start == NULL) {
    return NULL;
  }

  b->pos = b->start;
  b->last = b->start;
  b->end = b->last + size;
  b->temporary = 1;

  return b;
}

void ngx_http_influxdb_metric_init(ngx_http_influxdb_metric_t *metric,
                                   ngx_http_request_t *req) {
  metric->method = method_to_name(req->method);
  metric->status = req->headers_out.status;
  // TODO(fntlnz): Find a proper server name to be used here (configuration?)
  metric->server_name = "default";
  metric->total_bytes_sent = req->connection->sent;
  metric->header_bytes_sent = req->header_size;
  metric->request_length = req->request_length;
}

ngx_int_t ngx_http_influxdb_metric_push(ngx_pool_t *pool,
                                        ngx_http_influxdb_metric_t *m,
                                        ngx_str_t host, ngx_uint_t port,
                                        ngx_str_t measurement) {
  size_t len = sizeof(measurement) - 1 + sizeof("server_name=") - 1 +
               sizeof(m->server_name) - 1 + sizeof(",method=") - 1 +
               sizeof(m->method) - 1 + sizeof(" status=") - 1 + NGX_INT_T_LEN +
               sizeof(",total_bytes_sent=") - 1 + NGX_INT_T_LEN +
               sizeof(",header_bytes_sent=") - 1 + NGX_INT_T_LEN +
               sizeof(",request_length=") - 1 + NGX_INT_T_LEN;

  ngx_buf_t *buf = create_temp_char_buf(pool, len);

  (void)ngx_sprintf(buf->pos,
                    "%s,server_name=%s,method=%s "
                    "status=%i,total_bytes_sent=%O,header_"
                    "bytes_sent=%z,request_length=%O",
                    measurement.data, m->server_name, m->method, m->status,
                    m->total_bytes_sent, m->header_bytes_sent,
                    m->request_length);

  struct sockaddr_in servaddr;
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  bzero(&servaddr, sizeof(servaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(host.data);
  servaddr.sin_port = htons(port);

  ssize_t sentlen =
      sendto(sockfd, buf->pos, strlen(buf->pos), 0,
             (const struct sockaddr *)&servaddr, sizeof(servaddr));

  // if (buf != NULL) {
  //   ngx_free(buf);
  // }

  close(sockfd);

  if (sentlen < 0) {
    return INFLUXDB_METRIC_ERR;
  }

  return INFLUXDB_METRIC_OK;
}
