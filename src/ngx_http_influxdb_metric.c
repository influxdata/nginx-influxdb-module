#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "ngx_http_influxdb_metric.h"

static char *
ngx_http_influxdb_method_to_name(ngx_uint_t method)
{
    switch(method) {
        case NGX_HTTP_UNKNOWN      : return "UNKNOWN";
        case NGX_HTTP_GET          : return "GET";
        case NGX_HTTP_HEAD         : return "HEAD";
        case NGX_HTTP_POST         : return "POST";
        case NGX_HTTP_PUT          : return "PUT";
        case NGX_HTTP_DELETE       : return "DELETE";
        case NGX_HTTP_MKCOL        : return "MKCOL";
        case NGX_HTTP_COPY         : return "COPY";
        case NGX_HTTP_MOVE         : return "MOVE";
        case NGX_HTTP_OPTIONS      : return "OPTIONS";
        case NGX_HTTP_PROPFIND     : return "PROPFIND";
        case NGX_HTTP_PROPPATCH    : return "PROPPATCH";
        case NGX_HTTP_LOCK         : return "LOCK";
        case NGX_HTTP_UNLOCK       : return "UNLOCK";
        case NGX_HTTP_PATCH        : return "PATCH";
        case NGX_HTTP_TRACE        : return "TRACE";
        default                    : return NULL;
    }
}

static ngx_uint_t
ngx_http_influxdb_ssl_handshake_time(ngx_http_request_t *req)
{
#if(NGX_SSL)
    if (req->connection->requests == 1) {
        ngx_ssl_connection_t *ssl = req->connection->ssl;
        if (ssl) {
            return (ngx_msec_int_t)((ssl->handshake.end_sec - ssl->handshake.start_sec) * 1000 + (ssl->handshake.end_msec - ssl->handshake.start_msec));
        }
    }
#endif
    return 0;
}

void
ngx_http_influxdb_metric_init(ngx_http_influxdb_metric_t *metric, ngx_http_request_t *req)
{
    metric->method = ngx_http_influxdb_method_to_name(req->method);
    metric->status = req->headers_out.status;
    metric->total_bytes_sent = req->connection->sent;
    metric->body_bytes_sent = req->connection->sent - req->header_size;
    metric->header_bytes_sent = req->header_size;
    metric->request_length = req->request_length;
    metric->ssl_handshake_time = ngx_http_influxdb_ssl_handshake_time(req);
}

void
ngx_http_influxdb_metric_push(ngx_http_influxdb_metric_t *m, const char* host, uint16_t port, const char *measurement)
{
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(host);
    servaddr.sin_port = htons((uint16_t) port);

    char buf[255];

    sprintf(buf,
            "%s,host=serverA method=\"%s\",status=%ld,total_bytes_sent=%jd,body_bytes_sent=%zu,header_bytes_sent=%zu,request_length=%zd,ssl_handshake_time=%ld",
            measurement, m->method, m->status, m->total_bytes_sent, (intmax_t ) m->body_bytes_sent, m->header_bytes_sent, m->request_length, m->ssl_handshake_time);
    sendto(sockfd, buf, strlen(buf), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
}
