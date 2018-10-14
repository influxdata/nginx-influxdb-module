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
  metric->uri = req->unparsed_uri;
  metric->status = req->headers_out.status;
  metric->bytes_sent = req->connection->sent;
  size_t bbs = req->connection->sent - req->header_size;
  metric->body_bytes_sent = bbs > 0 ? bbs : 0;

  metric->content_type = req->headers_out.content_type;

  // request time (how long we are dealing with the request) {{{
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
  // end of request time }}}
}

ngx_int_t ngx_http_influxdb_metric_push(ngx_pool_t *pool,
                                        ngx_http_influxdb_metric_t *m,
                                        ngx_str_t host, ngx_uint_t port,
                                        ngx_str_t measurement,
                                        ngx_str_t dynamic_fields) {
  // Measurement + tags + separator
  size_t line_size = measurement.len + ngx_strlen(",server_name=") +
                     m->server_name.len + ngx_strlen(" ");

  // Dynamic Fields
  line_size += dynamic_fields.len;

  // Static fields + timestamp
  line_size +=
      ngx_strlen("method=\"") + m->method.len + ngx_strlen("\",status=") +
      NGX_INT_T_LEN + ngx_strlen(",bytes_sent=") + sizeof(off_t) +
      ngx_strlen(",body_bytes_sent=") + sizeof(off_t) +
      ngx_strlen(",header_bytes_sent=") + sizeof(size_t) +
      ngx_strlen(",request_length=") + sizeof(off_t) + ngx_strlen(",uri=\"") +
      m->uri.len + ngx_strlen("\",extension=\"") + m->extension.len +
      ngx_strlen("\",content_type=\"") + m->content_type.len +
      ngx_strlen("\",request_time=") + m->request_time.len;

  ngx_buf_t *buf = create_temp_char_buf(pool, line_size);

  (void)ngx_sprintf(buf->last,
                    "%V,server_name=%V "
                    "%Vmethod=\"%V\",status=%i,bytes_sent=%O,body_"
                    "bytes_sent=%O,header_"
                    "bytes_sent=%z,request_length=%O,uri=\"%V\",extension=\"%"
                    "V\",content_type=\"%V\",request_time=%V",
                    &measurement, &m->server_name, &dynamic_fields, &m->method,
                    m->status, m->bytes_sent, m->body_bytes_sent,
                    m->header_bytes_sent, m->request_length, &m->uri,
                    &m->extension, &m->content_type, &m->request_time);

  struct sockaddr_in servaddr;
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  bzero(&servaddr, sizeof(servaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = ngx_inet_addr(host.data, host.len);
  servaddr.sin_port = htons(port);

  ssize_t sentlen =
      sendto(sockfd, buf->last, ngx_strlen(buf->last), 0,
             (const struct sockaddr *)&servaddr, sizeof(servaddr));

  close(sockfd);

  if (sentlen < 0) {
    return INFLUXDB_METRIC_ERR;
  }

  return INFLUXDB_METRIC_OK;
}
