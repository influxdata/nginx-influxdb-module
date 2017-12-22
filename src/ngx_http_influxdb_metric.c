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
  metric->body_bytes_sent = req->headers_out.content_length_n;
  metric->connection_bytes_sent = req->connection->sent;
  metric->header_bytes_sent = req->header_size;
  metric->request_length = req->request_length;
  metric->extension = req->exten;
  metric->uri = req->uri;

  metric->content_type = req->headers_out.content_type;
}

ngx_int_t ngx_http_influxdb_metric_push(ngx_pool_t *pool,
                                        ngx_http_influxdb_metric_t *m,
                                        ngx_str_t host, ngx_uint_t port,
                                        ngx_str_t measurement) {
  size_t len = sizeof(measurement) - 1 + sizeof(",server_name=") - 1 +
               sizeof(m->server_name) - 1 + sizeof(" method=") - 1 +
               sizeof(m->method) - 1 + sizeof(",status=") - 1 + NGX_INT_T_LEN +
               sizeof(",connection_bytes_sent=") - 1 + NGX_INT_T_LEN +
               sizeof(",body_bytes_sent=") - 1 + NGX_INT_T_LEN +
               sizeof(",header_bytes_sent=") - 1 + NGX_INT_T_LEN +
               sizeof(",request_length=") - 1 + NGX_INT_T_LEN +
               sizeof(",uri=") - 1 + sizeof(m->uri) + sizeof(",extension=") -
               1 + sizeof(m->extension) + sizeof(",content_type=") - 1 +
               sizeof(m->content_type);

  ngx_buf_t *buf = create_temp_char_buf(pool, len);

  (void)ngx_sprintf(buf->last,
                    "%V,server_name=%V "
                    "method=\"%V\",status=%i,connection_bytes_sent=%O,body_"
                    "bytes_sent=%O,header_"
                    "bytes_sent=%z,request_length=%O,uri=\"%V\",extension=\"%"
                    "V\",content_type=\"%V\"",
                    &measurement, &m->server_name, &m->method, m->status,
                    m->connection_bytes_sent, m->body_bytes_sent,
                    m->header_bytes_sent, m->request_length, &m->uri,
                    &m->extension, &m->content_type);

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
