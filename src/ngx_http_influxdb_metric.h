#ifndef NGX_HTTP_INFLUXDB_METRIC_H
#define NGX_HTTP_INFLUXDB_METRIC_H

#include <ngx_config.h>
#include <ngx_http.h>
#include <ngx_string.h>
#define INFLUXDB_METRIC_OK 0
#define INFLUXDB_METRIC_ERR -1

typedef struct {
  ngx_str_t method;
  ngx_str_t server_name;
  ngx_uint_t status;
  off_t content_length_n;
  size_t header_bytes_sent;
  off_t request_length;
  ngx_str_t uri;
} ngx_http_influxdb_metric_t;

void ngx_http_influxdb_metric_init(ngx_http_influxdb_metric_t *metric,
                                   ngx_http_request_t *req);

ngx_int_t ngx_http_influxdb_metric_push(ngx_pool_t *pool,
                                        ngx_http_influxdb_metric_t *m,
                                        ngx_str_t host, ngx_uint_t port,
                                        ngx_str_t measurement);
#endif  // NGX_HTTP_INFLUXDB_METRIC_H
