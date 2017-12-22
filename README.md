# NGINX InfluxDB Module

A module to monitor request passing trough an NGINX server by sending
every request to an InfluxDB backend exposing UDP.


## Installation

## Build the module statically with NGINX

Writing this in a snap!

## Build the module dinamically to be loaded within NGINX

Writing this in a snap!

## Configuration

To configure it you just need this one line configuration.

```
influxdb server_name=myserver host=127.0.0.1 port=8089 measurement=mymeasures;
```


### Full Example

A full example config looks like this

```nginx
user  nginx;
worker_processes  auto;
daemon off;

#load_module path/to/the/module/ngx_http_influxdb_module.so; needed if a dynamic module
error_log  /var/log/nginx/error.log debug;
pid        /var/run/nginx/nginx.pid;


events {
    worker_connections  1024;
}

http {
    include       /etc/nginx/mime.types;
    default_type  application/octet-stream;

    log_format  main  '$remote_addr - $remote_user [$time_local] "$request" '
                      '$status $body_bytes_sent "$http_referer" '
                      '"$http_user_agent" "$http_x_forwarded_for"';

    access_log  /tmp/access.log main;

    sendfile        on;

    keepalive_timeout  65;

    server {
        listen       8080;
        server_name  localhost;
        location / {
            root   /usr/share/nginx/html;
            influxdb server_name=myserver host=127.0.0.1 port=8089 measurement=mymeasures;
            index  index.html index.htm;
        }

        error_page   500 502 503 504  /50x.html;
        location = /50x.html {
            root   /usr/share/nginx/html;
        }
    }

}

```


## The module needs your help!!

Any help is *always* appreciated, if you're reading this you're already
qualified to help! If you can't find anything just send me an email or
DM me on twitter ([@fntlnz](https://twitter.com/fntlnz)) :angel: