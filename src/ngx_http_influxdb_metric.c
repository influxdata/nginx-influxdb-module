#include <arpa/inet.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

#include "ngx_http_influxdb_metric.h"

static char *ngx_http_influxdb_method_to_name(ngx_uint_t method) {
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

void ngx_http_influxdb_metric_init(ngx_http_influxdb_metric_t *metric,
                                   ngx_http_request_t *req) {
  metric->method = ngx_http_influxdb_method_to_name(req->method);
  metric->status = req->headers_out.status;
  // TODO(fntlnz): Find a proper server name to be used here (configuration?)
  metric->server_name = "default";
  metric->total_bytes_sent = req->connection->sent;
  metric->header_bytes_sent = req->header_size;
  metric->request_length = req->request_length;
}

ngx_int_t ngx_http_influxdb_metric_push(ngx_http_request_t *r,
                                        ngx_http_influxdb_metric_t *m,
                                        ngx_str_t host, uint16_t port,
                                        ngx_str_t measurement) {
  int sockfd;
  struct sockaddr_in servaddr;

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);

  bzero(&servaddr, sizeof(servaddr));

  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(host.data);
  servaddr.sin_port = htons((uint16_t)port);

  ngx_buf_t *buf = ngx_create_temp_buf(
      r->pool,
      512);  // todo: calculate a proper size see
             // https://github.com/nginx/nginx/blob/b992f7259ba4763178f9d394b320bcc5de88818b/src/http/modules/ngx_http_stub_status_module.c#L116
  if (buf == NULL) {
    close(sockfd);
    return INFLUXDB_METRIC_ERR;
  }

  (void)ngx_sprintf(buf->pos,
                    "%s,server_name=%s,method=%s "
                    "status=%l,total_bytes_sent=%l,header_"
                    "bytes_sent=%l,request_length=%l",
                    measurement.data, m->server_name, m->method, m->status,
                    (intmax_t)m->total_bytes_sent, m->header_bytes_sent,
                    m->request_length);

  ssize_t sentlen =
      sendto(sockfd, buf->pos, strlen(buf->pos), 0,
             (const struct sockaddr *)&servaddr, sizeof(servaddr));

  close(sockfd);

  if (sentlen < 0) {
    return INFLUXDB_METRIC_ERR;
  }

  return INFLUXDB_METRIC_OK;
}
