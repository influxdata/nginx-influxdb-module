#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <ngx_string.h>

typedef struct {
    ngx_http_request_t *req;
    char *method;
    ngx_uint_t status;
    off_t total_bytes_sent;
    size_t header_bytes_sent;
    size_t body_bytes_sent;
    off_t request_length;
    ngx_uint_t ssl_handshake_time;
} metric_t;


typedef struct {
    ngx_str_t host;
    ngx_uint_t port;
    ngx_str_t measurement;
} ngx_http_influxdb_loc_conf_t;

static void * ngx_http_influxdb_create_loc_conf(ngx_conf_t *conf);
static char * ngx_http_influxdb_merge_loc_conf(ngx_conf_t *conf, void *parent, void *child);
static char * ngx_http_influxdb(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void ngx_influxdb_exit(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_influxdb_init(ngx_conf_t *conf);

static ngx_http_module_t ngx_http_influxdb_module_ctx = {
        NULL,                                    /* preconfiguration */
        ngx_http_influxdb_init,                  /* postconfiguration */

        NULL,                                    /* create main configuration */
        NULL,                                    /* init main configuration */

        NULL,                                    /* create server configuration */
        NULL,                                    /* merge server configuration */

        ngx_http_influxdb_create_loc_conf,       /* create location configuration */
        ngx_http_influxdb_merge_loc_conf         /* merge location configuration */
};

static ngx_command_t ngx_http_influxdb_commands[] = {
        {
                ngx_string("influxdb"),
                NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_1MORE,
                ngx_http_influxdb,
                NGX_HTTP_LOC_CONF_OFFSET,
                0,
                NULL
        },
};

ngx_module_t ngx_http_influxdb_module = {
        NGX_MODULE_V1,
        &ngx_http_influxdb_module_ctx,  /* module context */
        ngx_http_influxdb_commands,     /* module directives */
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


static metric_t *
metric_init(ngx_http_request_t *req)
{
    metric_t *metric = ngx_palloc(req->pool, sizeof(metric_t));
    metric->req = req;
    metric->method = ngx_http_influxdb_method_to_name(req->method);
    metric->status = req->headers_out.status;
    metric->total_bytes_sent = req->connection->sent;
    metric->body_bytes_sent = req->connection->sent - req->header_size;
    metric->header_bytes_sent = req->header_size;
    metric->request_length = req->request_length;
    metric->ssl_handshake_time = ngx_http_influxdb_ssl_handshake_time(req);
    return metric;
}

static void
metric_push(metric_t *m)
{
    ngx_http_influxdb_loc_conf_t *conf;
    conf = ngx_http_get_module_loc_conf(m->req, ngx_http_influxdb_module);


    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr((const char *) conf->host.data);
    servaddr.sin_port = htons((uint16_t) conf->port);

    char buf[255];

    sprintf(buf,
            "%s,host=serverA method=\"%s\",status=%ld,total_bytes_sent=%jd,body_bytes_sent=%zu,header_bytes_sent=%zu,request_length=%zd,ssl_handshake_time=%ld",
            conf->measurement.data, m->method, m->status, m->total_bytes_sent, (intmax_t ) m->body_bytes_sent, m->header_bytes_sent, m->request_length, m->ssl_handshake_time);
    sendto(sockfd, buf, strlen(buf), 0, (const struct sockaddr *) &servaddr, sizeof(servaddr));
}

static ngx_int_t
ngx_http_influxdb_handler(ngx_http_request_t *req)
{
    metric_t *m = metric_init(req);
    metric_push(m);
    return NGX_OK;
}

static void
ngx_influxdb_exit(ngx_cycle_t *cycle)
{
}

static void *
ngx_http_influxdb_create_loc_conf(ngx_conf_t *conf)
{
    ngx_http_influxdb_loc_conf_t *cf;
    cf = ngx_palloc(conf->pool, sizeof(ngx_http_influxdb_loc_conf_t));
    if (cf == NULL) {
        return NGX_CONF_ERROR;
    }

    cf->port = NGX_CONF_UNSET_UINT;
    return cf;
}

static char *
ngx_http_influxdb_merge_loc_conf(ngx_conf_t *conf, void *parent, void *child)
{
    ngx_http_influxdb_loc_conf_t *prev = parent;
    ngx_http_influxdb_loc_conf_t *cf = child;

    ngx_conf_merge_str_value(cf->host, prev->host, "127.0.0.1");
    ngx_conf_merge_uint_value(cf->port, prev->port, 4444);
    ngx_conf_merge_str_value(cf->measurement, prev->measurement, "nginx");

    return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_influxdb_init(ngx_conf_t *conf) {
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

static char *
ngx_http_influxdb(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_influxdb_loc_conf_t  *ulcf = conf;
    ngx_str_t                   *value;

    value = cf->args->elts;

    ngx_uint_t i;
    for (i = 1; i < cf->args->nelts; i++) {
        if (ngx_strncmp(value[i].data, "measurement=", ngx_strlen("measurement=")) == 0) {
            ulcf->measurement.data = &value[i].data[ngx_strlen("measurement=")];
            ulcf->measurement.len = ngx_strlen(&value[i].data[ngx_strlen("measurement=")]);
            continue;
        }

        if (ngx_strncmp(value[i].data, "host=", ngx_strlen("host=")) == 0) {
            ulcf->host.data = &value[i].data[ngx_strlen("host=")];
            ulcf->host.len = ngx_strlen(&value[i].data[ngx_strlen("host=")]);
            continue;
        }

        if (ngx_strncmp(value[i].data, "port=", ngx_strlen("port=")) == 0) {
            ulcf->port = (ngx_uint_t) ngx_atoi(&value[i].data[ngx_strlen("port=")], ngx_strlen(&value[i].data[ngx_strlen("port=")]));
            continue;
        }
    }

    return NGX_CONF_OK;
}
