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

void ngx_http_influxdb_metric_init(ngx_pool_t *pool,
                                   ngx_http_influxdb_metric_t *metric,
                                   ngx_http_request_t *req,
                                   ngx_str_t server_name) {
  metric->method = req->method_name;
  metric->server_name = server_name;
  metric->header_bytes_sent = req->header_size;
  metric->request_length = req->request_length;
  metric->extension = req->exten;
  metric->uri = req->uri;
  metric->status = req->headers_out.status;
  metric->bytes_sent = req->connection->sent;
  size_t bbs = req->connection->sent - req->header_size;
  metric->body_bytes_sent = bbs > 0 ? bbs : 0;

  metric->content_type = req->headers_out.content_type;

  // request time (how long we are dealing with the request)
  ngx_time_t *tp;
  ngx_msec_int_t ms;

  tp = ngx_timeofday();

  ms = (ngx_msec_int_t)((tp->sec - req->start_sec) * 1000 +
                        (tp->msec - req->start_msec));
  ms = ngx_max(ms, 0);

  size_t len = sizeof(time_t);
  ngx_buf_t *buf = create_temp_char_buf(pool, len);
  (void)ngx_sprintf(buf->last, "%T.%03M", (time_t)ms / 1000, ms % 1000);

  metric->request_time.data = buf->last;
  metric->request_time.len = len;
}

ngx_int_t ngx_http_influxdb_metric_push(ngx_pool_t *pool,
                                        ngx_http_influxdb_metric_t *m,
                                        ngx_str_t host, ngx_uint_t port,
                                        ngx_str_t measurement) {
  size_t len =
      sizeof(measurement) - 1 + sizeof(",server_name=") - 1 +
      sizeof(m->server_name) - 1 + sizeof(" method=") - 1 + sizeof(m->method) -
      1 + sizeof(",status=") - 1 + NGX_INT_T_LEN + sizeof(",bytes_sent=") - 1 +
      NGX_INT_T_LEN + sizeof(",body_bytes_sent=") - 1 + NGX_INT_T_LEN +
      sizeof(",header_bytes_sent=") - 1 + NGX_INT_T_LEN +
      sizeof(",request_length=") - 1 + NGX_INT_T_LEN + sizeof(",uri=") - 1 +
      sizeof(m->uri) + sizeof(",extension=") - 1 + sizeof(m->extension) +
      sizeof(",content_type=") - 1 + sizeof(m->content_type) + sizeof(time_t) -
      1 + sizeof(",request_time=");

  ngx_buf_t *buf = create_temp_char_buf(pool, len);

  (void)ngx_sprintf(buf->last,
                    "%V,server_name=%V,method=\"%V\""
                    ",status=%i,content_type=\"%V\","
                    "uri=\"%V\",extension=\"%V\" "
                    "bytes_sent=%O,body_"
                    "bytes_sent=%O,header_"
                    "bytes_sent=%z,request_length=%O"
                    ",request_time=%V",
                    &measurement, &m->server_name, &m->method, m->status, 
                    &m->content_type, &m->uri,
                    m->bytes_sent, m->body_bytes_sent, m->header_bytes_sent,
                    m->request_length, &m->extension,
                    &m->request_time);

  struct sockaddr_in servaddr;
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  bzero(&servaddr, sizeof(servaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = ngx_inet_addr(host.data, host.len);
  servaddr.sin_port = htons(port);

  size_t sendsize = ngx_strlen(buf->last);
  ssize_t sentlen =
      sendto(sockfd, buf->last, sendsize, 0, (const struct sockaddr *)&servaddr,
             sizeof(servaddr));

  close(sockfd);

  if (sentlen < 0) {
    return INFLUXDB_METRIC_ERR;
  }

  return INFLUXDB_METRIC_OK;
}
