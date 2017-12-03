#ifndef NGX_HTTP_INFLUXDB_METRIC_H
#define NGX_HTTP_INFLUXDB_METRIC_H

#include <ngx_core.h>
#include <stdio.h>

typedef struct
{
    char *method;
    char *server_name;
    ngx_uint_t status;
    off_t total_bytes_sent;
    size_t header_bytes_sent;
    size_t body_bytes_sent;
    off_t request_length;
} ngx_http_influxdb_metric_t;

void ngx_http_influxdb_metric_init(ngx_http_influxdb_metric_t *metric, ngx_http_request_t *req);
void ngx_http_influxdb_metric_push(ngx_http_influxdb_metric_t *m, const char *host, uint16_t port, const char *measurement);
#endif // NGX_HTTP_INFLUXDB_METRIC_H
