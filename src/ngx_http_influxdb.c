#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct {
    char *method;
    ngx_uint_t status;
    off_t bytes_sent;
    size_t header_bytes_sent;
    int body_bytes;
    off_t request_length;
    int ssl_handshake_time;
} metric_t;

static char *ngx_http_influxdb_method_to_name(ngx_uint_t method)
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
    }
    return NULL;
}

static int
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

static metric_t *
metric_alloc_from_request(ngx_http_request_t *req)
{
    metric_t *metric = malloc(sizeof(metric_t));
    metric->method = ngx_http_influxdb_method_to_name(req->method);
    metric->status = req->headers_out.status;
    metric->bytes_sent = req->connection->sent;
    metric->body_bytes = req->connection->sent - req->header_size;
    metric->header_bytes_sent = req->header_size;
    metric->request_length = req->request_length;
    metric->ssl_handshake_time = ngx_http_influxdb_ssl_handshake_time(req);
    return metric;
}

static void
push_metric(metric_t *m)
{
    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(32000);
    char *buf = m->method;
    sendto(sockfd, buf, strlen(buf), 0, &servaddr, sizeof(servaddr));
}

static ngx_int_t
ngx_http_influxdb_handler(ngx_http_request_t *req)
{
    metric_t *m = metric_alloc_from_request(req);
    push_metric(m);
    return NGX_OK;
}

static ngx_int_t
ngx_http_influxdb_init(ngx_conf_t *conf)
{
    ngx_http_handler_pt        *h;
    ngx_http_core_main_conf_t  *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(conf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (h == NULL) {
        return NGX_ERROR;
    }

    *h = ngx_http_influxdb_handler;

    return NGX_OK;
}

static void
ngx_influxdb_exit(ngx_cycle_t *cycle)
{
}

static ngx_http_module_t ngx_http_influxdb_module_ctx = {
        NULL,                          /* preconfiguration */
        ngx_http_influxdb_init,        /* postconfiguration */

        NULL,                          /* create main configuration */ NULL,                          /* init main configuration */

        NULL,                          /* create server configuration */
        NULL,                          /* merge server configuration */

        NULL,                          /* create location configuration */
        NULL                           /* merge location configuration */
};


ngx_module_t ngx_http_influxdb_module = {
        NGX_MODULE_V1,
        &ngx_http_influxdb_module_ctx,  /* module context */
        NULL,                           /* module directives */
        NGX_HTTP_MODULE,                /* module type */
        NULL,                           /* init master */
        NULL,                           /* init module */
        NULL,                           /* init process */
        NULL,                           /* init thread */
        NULL,                           /* exit thread */
        NULL,                           /* exit process */
        ngx_influxdb_exit,              /* exit master */
        NGX_MODULE_V1_PADDING
};
