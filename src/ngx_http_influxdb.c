#include <netinet/in.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "ngx_http_influxdb_metric.h"

typedef struct {
  ngx_str_t host;
  ngx_uint_t port;
  ngx_str_t server_name;
  ngx_str_t enabled;
  ngx_str_t measurement;
} ngx_http_influxdb_loc_conf_t;

static void *ngx_http_influxdb_create_loc_conf(ngx_conf_t *conf);
static char *ngx_http_influxdb_merge_loc_conf(ngx_conf_t *conf, void *parent,
                                              void *child);
static char *ngx_http_influxdb(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static ngx_int_t nginx_http_influxdb_filter_init(ngx_conf_t *cf);

static ngx_http_output_body_filter_pt ngx_http_next_body_filter;

static ngx_command_t ngx_http_influxdb_commands[] = {
    {ngx_string("influxdb"),
     NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF |
         NGX_CONF_TAKE5,
     ngx_http_influxdb, NGX_HTTP_LOC_CONF_OFFSET, 0, NULL},
};

static ngx_http_module_t ngx_http_influxdb_body_filter_module_ctx = {
    NULL,                              /* preconfiguration */
    nginx_http_influxdb_filter_init,   /* postconfiguration */
    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */
    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */
    ngx_http_influxdb_create_loc_conf, /* create location configuration */
    ngx_http_influxdb_merge_loc_conf   /* merge location configuration */
};

ngx_module_t ngx_http_influxdb_body_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_influxdb_body_filter_module_ctx, /* module context */
    ngx_http_influxdb_commands,                /* module directives */
    NGX_HTTP_MODULE,                           /* module type */
    NULL,                                      /* init master */
    NULL,                                      /* init module */
    NULL,                                      /* init process */
    NULL,                                      /* init thread */
    NULL,                                      /* exit thread */
    NULL,                                      /* exit process */
    NULL,                                      /* exit master */
    NGX_MODULE_V1_PADDING};

static ngx_int_t ngx_http_influxdb_metrics_body_filter(ngx_http_request_t *r,
                                                       ngx_chain_t *in) {
  ngx_connection_t *c;
  ngx_http_influxdb_loc_conf_t *conf;
  ngx_uint_t last, flush;

  ngx_chain_t *cl;

  c = r->connection;

  if (c->error) {
    return NGX_ERROR;
  }

  for (cl = r->out; cl; cl = cl->next) {
    if (cl->buf->flush || cl->buf->recycled) {
      flush = 1;
    }

    if (cl->buf->last_buf) {
      last = 1;
    }
  }

  if (!last && !flush && in) {
    return ngx_http_next_body_filter(r, in);
  }

  conf = ngx_http_get_module_loc_conf(r, ngx_http_influxdb_body_filter_module);

  if (ngx_strcmp(conf->enabled.data, "false") == 0) {
    return ngx_http_next_body_filter(r, in);
  }
  ngx_http_influxdb_metric_t *m =
      ngx_palloc(r->pool, sizeof(ngx_http_influxdb_metric_t));
  if (m == NULL) {
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
                  "Failed to allocate influxdb metric handler");
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }

  ngx_http_influxdb_metric_init(m, r, conf->server_name);
  ngx_int_t pushret = ngx_http_influxdb_metric_push(
      r->pool, m, conf->host, conf->port, conf->measurement);

  if (pushret == INFLUXDB_METRIC_ERR) {
    ngx_log_error(
        NGX_LOG_WARN, r->connection->log, 0,
        "An error occurred sending metrics to the influxdb backend: %s",
        strerror(errno));
  }

  return ngx_http_next_body_filter(r, in);
}

static void *ngx_http_influxdb_create_loc_conf(ngx_conf_t *conf) {
  ngx_http_influxdb_loc_conf_t *cf;
  cf = ngx_palloc(conf->pool, sizeof(ngx_http_influxdb_loc_conf_t));
  if (cf == NULL) {
    return NGX_CONF_ERROR;
  }

  cf->port = NGX_CONF_UNSET_UINT;
  cf->host.data = NULL;
  cf->host.len = 0;
  cf->enabled.data = NULL;
  cf->enabled.len = 0;
  cf->server_name.data = NULL;
  cf->server_name.len = 0;
  cf->measurement.data = NULL;
  cf->measurement.len = 0;

  return cf;
}

static char *ngx_http_influxdb_merge_loc_conf(ngx_conf_t *conf, void *parent,
                                              void *child) {
  ngx_http_influxdb_loc_conf_t *prev = parent;
  ngx_http_influxdb_loc_conf_t *cf = child;

  ngx_conf_merge_str_value(cf->host, prev->host, "influxdb");
  ngx_conf_merge_uint_value(cf->port, prev->port, 8089);
  ngx_conf_merge_str_value(cf->enabled, prev->enabled, "false");
  ngx_conf_merge_str_value(cf->server_name, prev->server_name, "default");
  ngx_conf_merge_str_value(cf->measurement, prev->measurement, "nginx");

  return NGX_CONF_OK;
}

static ngx_int_t nginx_http_influxdb_filter_init(ngx_conf_t *cf) {
  ngx_http_next_body_filter = ngx_http_top_body_filter;
  ngx_http_top_body_filter = ngx_http_influxdb_metrics_body_filter;
  return NGX_OK;
}

static char *ngx_http_influxdb(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
  ngx_http_influxdb_loc_conf_t *ulcf = conf;
  ngx_str_t *value;

  value = cf->args->elts;

  ngx_uint_t i;
  for (i = 1; i < cf->args->nelts; i++) {
    if (ngx_strncmp(value[i].data,
                    "measurement=", ngx_strlen("measurement=")) == 0) {
      ulcf->measurement.data = &value[i].data[ngx_strlen("measurement=")];
      ulcf->measurement.len =
          ngx_strlen(&value[i].data[ngx_strlen("measurement=")]);
      continue;
    }

    if (ngx_strncmp(value[i].data, "host=", ngx_strlen("host=")) == 0) {
      ulcf->host.data = &value[i].data[ngx_strlen("host=")];
      ulcf->host.len = ngx_strlen(&value[i].data[ngx_strlen("host=")]);
      continue;
    }

    if (ngx_strncmp(value[i].data, "port=", ngx_strlen("port=")) == 0) {
      ulcf->port =
          (ngx_uint_t)ngx_atoi(&value[i].data[ngx_strlen("port=")],
                               ngx_strlen(&value[i].data[ngx_strlen("port=")]));
      continue;
    }

    if (ngx_strncmp(value[i].data,
                    "server_name=", ngx_strlen("server_name=")) == 0) {
      ulcf->server_name.data = &value[i].data[ngx_strlen("server_name=")];
      ulcf->server_name.len =
          ngx_strlen(&value[i].data[ngx_strlen("server_name=")]);
      continue;
    }

    if (ngx_strncmp(value[i].data, "enabled=", ngx_strlen("enabled=")) == 0) {
      ulcf->enabled.data = &value[i].data[ngx_strlen("enabled=")];
      ulcf->enabled.len = ngx_strlen(&value[i].data[ngx_strlen("enabled=")]);
      continue;
    }
  }

  return NGX_CONF_OK;
}
