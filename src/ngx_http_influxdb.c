#include "ngx_http_influxdb_metric.h"
#include <netinet/in.h>
#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

typedef struct {
  ngx_str_t host;
  ngx_uint_t port;
  ngx_str_t server_name;
  ngx_str_t enabled;
  ngx_str_t measurement;
  ngx_array_t *dynamic_fields;
} ngx_http_influxdb_loc_conf_t;

static void *ngx_http_influxdb_create_loc_conf(ngx_conf_t *conf);
static char *ngx_http_influxdb_merge_loc_conf(ngx_conf_t *conf, void *parent,
                                              void *child);
static char *ngx_http_influxdb(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static char *ngx_http_influxdb_dynamic_fields(ngx_conf_t *cf,
                                              ngx_command_t *cmd, void *conf);
static void ngx_influxdb_exit(ngx_cycle_t *cycle);
static ngx_int_t ngx_http_influxdb_init(ngx_conf_t *conf);

static inline ngx_str_t dynamic_fields_lineprotocol(void *conf,
                                                    ngx_http_request_t *req);

static ngx_http_module_t ngx_http_influxdb_module_ctx = {
    NULL,                              /* preconfiguration */
    ngx_http_influxdb_init,            /* postconfiguration */
    NULL,                              /* create main configuration */
    NULL,                              /* init main configuration */
    NULL,                              /* create server configuration */
    NULL,                              /* merge server configuration */
    ngx_http_influxdb_create_loc_conf, /* create location configuration */
    ngx_http_influxdb_merge_loc_conf   /* merge location configuration */
};

static ngx_command_t ngx_http_influxdb_commands[] = {
    {ngx_string("influxdb"),
     NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF |
         NGX_CONF_1MORE,
     ngx_http_influxdb, NGX_HTTP_LOC_CONF_OFFSET, 0, NULL},
    {ngx_string("influxdb_dynamic_fields"),
     NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF |
         NGX_CONF_1MORE,
     ngx_http_influxdb_dynamic_fields, NGX_HTTP_LOC_CONF_OFFSET, 0, NULL},
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

static ngx_int_t ngx_http_influxdb_handler(ngx_http_request_t *req) {
  ngx_http_influxdb_loc_conf_t *conf;
  conf = ngx_http_get_module_loc_conf(req, ngx_http_influxdb_module);

  if (!(ngx_strcmp(conf->enabled.data, "true") == 0)) {
    return NGX_OK;
  }

  ngx_http_influxdb_metric_t *m =
      ngx_palloc(req->pool, sizeof(ngx_http_influxdb_metric_t));
  if (m == NULL) {
    ngx_log_error(NGX_LOG_ERR, req->connection->log, 0,
                  "Failed to allocate influxdb metric handler");
    return NGX_HTTP_INTERNAL_SERVER_ERROR;
  }

  ngx_http_influxdb_metric_init(req->pool, m, req, conf->server_name);

  ngx_str_t dynamic_fields = dynamic_fields_lineprotocol(conf, req);

  ngx_int_t pushret = ngx_http_influxdb_metric_push(
      req->pool, m, conf->host, (uint16_t)conf->port, conf->measurement,
      dynamic_fields);

  if (pushret == INFLUXDB_METRIC_ERR) {
    ngx_log_error(
        NGX_LOG_WARN, req->connection->log, 0,
        "An error occurred sending metrics to the influxdb backend: %s",
        strerror(errno));
  }

  return NGX_OK;
}

static ngx_int_t ngx_http_influxdb_init(ngx_conf_t *conf) {
  ngx_http_handler_pt *h;
  ngx_http_core_main_conf_t *cmcf;

  cmcf = ngx_http_conf_get_module_main_conf(conf, ngx_http_core_module);

  h = ngx_array_push(&cmcf->phases[NGX_HTTP_LOG_PHASE].handlers);
  if (h == NULL) {
    return NGX_ERROR;
  }

  *h = ngx_http_influxdb_handler;

  return NGX_OK;
}

static void ngx_influxdb_exit(ngx_cycle_t *cycle) {}

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
  cf->dynamic_fields = NGX_CONF_UNSET_PTR;

  return cf;
}

static char *ngx_http_influxdb_merge_loc_conf(ngx_conf_t *conf, void *parent,
                                              void *child) {
  ngx_http_influxdb_loc_conf_t *prev = parent;
  ngx_http_influxdb_loc_conf_t *cf = child;

  ngx_conf_merge_str_value(cf->host, prev->host, "127.0.0.1");
  ngx_conf_merge_uint_value(cf->port, prev->port, 8089);
  ngx_conf_merge_str_value(cf->enabled, prev->enabled, "false");
  ngx_conf_merge_str_value(cf->server_name, prev->server_name, "default");
  ngx_conf_merge_str_value(cf->measurement, prev->measurement, "nginx");
  ngx_conf_merge_ptr_value(cf->dynamic_fields, prev->dynamic_fields, NULL);

  return NGX_CONF_OK;
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

static char *ngx_http_influxdb_dynamic_fields(ngx_conf_t *cf,
                                              ngx_command_t *cmd, void *conf) {
  ngx_str_t *value;
  value = cf->args->elts;

  ngx_http_influxdb_loc_conf_t *ulcf = conf;

  ngx_http_complex_value_t *cv;

  if (ulcf->dynamic_fields == NGX_CONF_UNSET_PTR) {
    ulcf->dynamic_fields = ngx_array_create(cf->pool, cf->args->nelts,
                                            sizeof(ngx_http_complex_value_t));
    if (ulcf->dynamic_fields == NULL) {
      return NGX_CONF_ERROR;
    }
  }

  ngx_uint_t i;
  for (i = 1; i < cf->args->nelts; i++) {
    cv = ngx_array_push(ulcf->dynamic_fields);
    ngx_http_compile_complex_value_t ccv;
    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));
    ccv.cf = cf;
    ccv.value = &value[i];
    ccv.complex_value = cv;
    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
      return NGX_CONF_ERROR;
    }
  }

  return NGX_CONF_OK;
}

static inline ngx_str_t dynamic_fields_lineprotocol(void *conf,
                                                    ngx_http_request_t *req) {

  ngx_http_influxdb_loc_conf_t *ulcf = conf;
  ngx_str_t dynamic_fields = {
      .len = 0,
      .data = NULL,
  };

  ngx_http_complex_value_t *value;
  if (ulcf->dynamic_fields == NULL) {
    return dynamic_fields;
  }

  if (ulcf->dynamic_fields->nelts == 0) {
    return dynamic_fields;
  }

  value = ulcf->dynamic_fields->elts;

  unsigned char *dfline = NULL;
  size_t dfline_size = 0;

  ngx_uint_t i;
  for (i = 0; i < ulcf->dynamic_fields->nelts; i++) {
    ngx_str_t current_field;
    if (ngx_http_complex_value(req, &value[i], &current_field) != NGX_OK) {
      continue;
    }
    dfline_size = dfline_size + current_field.len + ngx_strlen(",");
  }

  dfline_size = dfline_size + ngx_strlen("\0");

  dfline = ngx_palloc(req->pool, dfline_size);

  if (dfline == NULL) {
    ngx_log_error(NGX_LOG_WARN, req->connection->log, 0,
                  "Failed to allocate influxdb dynamic fields, dynamic fields "
                  "will not be attached to metrics.");

    return dynamic_fields;
  }

  ngx_uint_t j;
  size_t offset = 0;
  for (j = 0; j < ulcf->dynamic_fields->nelts; j++) {
    ngx_str_t current_field;
    if (ngx_http_complex_value(req, &value[j], &current_field) != NGX_OK) {
      continue;
    }
    ngx_memcpy(dfline + offset, current_field.data, current_field.len);
    offset = offset + current_field.len;

    ngx_memcpy(dfline + offset, ",", ngx_strlen(","));
    offset = offset + ngx_strlen(",");
  }

  ngx_memcpy(dfline + offset, "\0", ngx_strlen("\0"));
  offset = offset + ngx_strlen("\0");

  dynamic_fields.data = dfline;
  dynamic_fields.len = dfline_size;
  return dynamic_fields;
}
