#ifndef NGX_HTTP_INFLUXDB_METRIC_H
#define NGX_HTTP_INFLUXDB_METRIC_H

#include <ngx_core.h>
#include <stdio.h>

#define INFLUXDB_METRIC_OK 0
#define INFLUXDB_METRIC_ERR -1

typedef struct {
  u_char *method;
  u_char *server_name;
  ngx_uint_t status;
  off_t total_bytes_sent;
  size_t header_bytes_sent;
  off_t request_length;
} ngx_http_influxdb_metric_t;

void ngx_http_influxdb_metric_init(ngx_http_influxdb_metric_t *metric,
                                   ngx_http_request_t *req);

ngx_int_t ngx_http_influxdb_metric_push(ngx_http_request_t *r,
                                        ngx_http_influxdb_metric_t *m,
                                        ngx_str_t host, uint16_t port,
                                        ngx_str_t measurement);
#endif  // NGX_HTTP_INFLUXDB_METRIC_H
