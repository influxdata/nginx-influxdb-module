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
  metric->method = req->method_name;
  metric->status = req->headers_out.status;
  // TODO(fntlnz): Find a proper server name to be used here (configuration?)
  ngx_str_t server_name = ngx_string("default");
  metric->server_name = server_name;
  metric->content_length_n = req->headers_out.content_length_n;
  metric->header_bytes_sent = req->header_size;
  metric->request_length = req->request_length;
  metric->uri = req->uri;
}

ngx_int_t ngx_http_influxdb_metric_push(ngx_pool_t *pool,
                                        ngx_http_influxdb_metric_t *m,
                                        ngx_str_t host, ngx_uint_t port,
                                        ngx_str_t measurement) {
  size_t len = sizeof(measurement) - 1 + sizeof(",server_name=") - 1 +
               sizeof(m->server_name) - 1 + sizeof(" method=") - 1 +
               sizeof(m->method) - 1 + sizeof(",status=") - 1 + NGX_INT_T_LEN +
               sizeof(",total_bytes_sent=") - 1 + NGX_INT_T_LEN +
               sizeof(",header_bytes_sent=") - 1 + NGX_INT_T_LEN +
               sizeof(",request_length=") - 1 + NGX_INT_T_LEN +
               sizeof(",uri=") - 1 + sizeof(m->uri);

  ngx_buf_t *buf = create_temp_char_buf(pool, len);

  (void)ngx_sprintf(
      buf->last,
      "%V,server_name=%V method=\"%V\",status=%i,content_length_n=%O,header_"
      "bytes_sent=%z,request_length=%O,uri=\"%V\"",
      &measurement, &m->server_name, &m->method, m->status, m->content_length_n,
      m->header_bytes_sent, m->request_length, &m->uri);

  struct sockaddr_in servaddr;
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  bzero(&servaddr, sizeof(servaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(host.data);
  servaddr.sin_port = htons(port);

  ssize_t sentlen =
      sendto(sockfd, buf->last, strlen(buf->pos), 0,
             (const struct sockaddr *)&servaddr, sizeof(servaddr));

  close(sockfd);

  if (sentlen < 0) {
    return INFLUXDB_METRIC_ERR;
  }

  return INFLUXDB_METRIC_OK;
}
