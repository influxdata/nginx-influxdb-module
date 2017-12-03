## Config Example (right now)

Right now the configuration needs to stay in `location{}s`using

```
influxdb host=127.0.0.1 port=8089 measurement=shiz;
```

A full example config looks like this

```
# user  nginx;
worker_processes  1;
daemon off;


#load_module modules/ngx_http_influxdb_module.so; needed if dynamic
error_log  /tmp/error.log debug;
pid        /tmp/nginx/nginx.pid;


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
            root   /home/fntlnz/Projects/fntlnz/static;
            influxdb host=127.0.0.1 port=8089 measurement=shiz;
            index  index.html index.htm;
        }

        error_page   500 502 503 504  /50x.html;
        location = /50x.html {
            root   /usr/share/nginx/html;
        }
    }

}

```
