#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include "ngx_http_influxdb_metric.h"

typedef struct
{
    ngx_str_t host;
    ngx_uint_t port;
    ngx_str_t measurement;
} ngx_http_influxdb_loc_conf_t;

static void *ngx_http_influxdb_create_loc_conf(ngx_conf_t *conf);
static char *ngx_http_influxdb_merge_loc_conf(ngx_conf_t *conf, void *parent, void *child);
static char *ngx_http_influxdb(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static void ngx_influxdb_exit(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_influxdb_init(ngx_conf_t *conf);

static ngx_http_module_t ngx_http_influxdb_module_ctx = {
    NULL,                   /* preconfiguration */
    ngx_http_influxdb_init, /* postconfiguration */

    NULL, /* create main configuration */
    NULL, /* init main configuration */

    NULL, /* create server configuration */
    NULL, /* merge server configuration */

    ngx_http_influxdb_create_loc_conf, /* create location configuration */
    ngx_http_influxdb_merge_loc_conf   /* merge location configuration */
};

static ngx_command_t ngx_http_influxdb_commands[] = {
    {ngx_string("influxdb"),
     NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_1MORE,
     ngx_http_influxdb,
     NGX_HTTP_LOC_CONF_OFFSET,
     0,
     NULL},
};

ngx_module_t ngx_http_influxdb_module = {
    NGX_MODULE_V1,
    &ngx_http_influxdb_module_ctx, /* module context */
    ngx_http_influxdb_commands,    /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    ngx_influxdb_exit,             /* exit master */
    NGX_MODULE_V1_PADDING};

static ngx_int_t
ngx_http_influxdb_handler(ngx_http_request_t *req)
{
    ngx_http_influxdb_metric_t *m = ngx_palloc(req->pool, sizeof(ngx_http_influxdb_metric_t));
    ngx_http_influxdb_metric_init(m, req);
    ngx_http_influxdb_loc_conf_t *conf;
    conf = ngx_http_get_module_loc_conf(req, ngx_http_influxdb_module);
    ngx_http_influxdb_metric_push(m, (const char *)conf->host.data, (uint16_t)conf->port, (const char *)conf->measurement.data);

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
    if (cf == NULL)
    {
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

    ngx_conf_merge_str_value(cf->host, prev->host, "influxdb");
    ngx_conf_merge_uint_value(cf->port, prev->port, 8089);
    ngx_conf_merge_str_value(cf->measurement, prev->measurement, "nginx");

    return NGX_CONF_OK;
}

static ngx_int_t
ngx_http_influxdb_init(ngx_conf_t *conf)
{
    ngx_http_handler_pt *h;
    ngx_http_core_main_conf_t *cmcf;

    cmcf = ngx_http_conf_get_module_main_conf(conf, ngx_http_core_module);

    h = ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers);
    if (h == NULL)
    {
        return NGX_ERROR;
    }

    *h = ngx_http_influxdb_handler;

    return NGX_OK;
}

static char *
ngx_http_influxdb(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_influxdb_loc_conf_t *ulcf = conf;
    ngx_str_t *value;

    value = cf->args->elts;

    ngx_uint_t i;
    for (i = 1; i < cf->args->nelts; i++)
    {
        if (ngx_strncmp(value[i].data, "measurement=", ngx_strlen("measurement=")) == 0)
        {
            ulcf->measurement.data = &value[i].data[ngx_strlen("measurement=")];
            ulcf->measurement.len = ngx_strlen(&value[i].data[ngx_strlen("measurement=")]);
            continue;
        }

        if (ngx_strncmp(value[i].data, "host=", ngx_strlen("host=")) == 0)
        {
            ulcf->host.data = &value[i].data[ngx_strlen("host=")];
            ulcf->host.len = ngx_strlen(&value[i].data[ngx_strlen("host=")]);
            continue;
        }

        if (ngx_strncmp(value[i].data, "port=", ngx_strlen("port=")) == 0)
        {
            ulcf->port = (ngx_uint_t)ngx_atoi(&value[i].data[ngx_strlen("port=")], ngx_strlen(&value[i].data[ngx_strlen("port=")]));
            continue;
        }
    }

    return NGX_CONF_OK;
}
