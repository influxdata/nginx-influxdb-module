# NGINX InfluxDB Module :unicorn:

A module to monitor request passing trough an NGINX server by sending
every request to an InfluxDB backend exposing UDP.

## Exported Fields (per request)

| Metric                | Type    | Description                                                                                                                                |
|-----------------------|---------|--------------------------------------------------------------------------------------------------------------------------------------------|
| method                | string  | The HTTP request method that has been given as a reply to the caller                                                                       |
| status                | integer | The HTTP status code of the reply from the server (refer to [RFC 7231](https://tools.ietf.org/html/rfc7231#section-6.1) for more details)  |
| bytes_sent            | integer | The number of bytes sent to a client body + header                                                                                         |
| body_bytes_sent       | integer | The number of bytes sent to a client only for body                                                                                         |
| header_bytes_sent     | integer | The number of bytes sent to a client for header and body                                                                                   |
| request_length        | integer | Request length (including request line, header, and request body)                                                                          |
| uri                   | string  | The called uri (e.g: /index.html)                                                                                                          |
| extension             | string  | The extension of the served file (e.g: js, html, php, png)                                                                                 |
| content_type          | string  | The content type of the response (e.g: text/html)                                                                                          |
| request_time          | string  | Request processing time in seconds with a milliseconds resolution                                                                          |


## Installation

## From pre-built dynamic modules

Pre-built dynamic modules are not available (yet)

## Build the module statically with NGINX

```
mkdir build
pushd build
git clone git@github.com:fntlnz/nginx-influxdb-module.git
wget -nv -O - http://nginx.org/download/nginx-1.13.7.tar.gz | tar zx
pushd nginx-1.13.7
./configure --add-module=../nginx-influxdb-module
make -j
popd
popd
```


## Build the module dinamically to be loaded within NGINX

Writing this in a snap!

## Configuration

To configure it you just need this one line configuration.

```
influxdb server_name=myserver host=127.0.0.1 port=8089 measurement=mymeasures enabled=true;
```


### Full Example

A full example config looks like this

```nginx
user  nginx;
worker_processes  auto;
daemon off;

#load_module path/to/the/module/ngx_http_influxdb_module.so; needed if a dynamic module
pid        /var/run/nginx/nginx.pid;


events {
    worker_connections  1024;
}

http {
    include       /etc/nginx/mime.types;
    default_type  application/octet-stream;

    sendfile        on;

    keepalive_timeout  65;

    server {
        listen       8080;
        server_name  localhost;
        location / {
            root   /usr/share/nginx/html;
            influxdb server_name=myserver host=127.0.0.1 port=8089 measurement=mymeasures enabled=true;
            index  index.html index.htm;
        }

        error_page   500 502 503 504  /50x.html;
        location = /50x.html {
            root   /usr/share/nginx/html;
        }
    }

}

```


### Local Testing

To test changes locally, there's a script in `hack/` called `build-start.sh` that can
build the local changes against the current nginx source code taken from github.

To run, the script also requires docker in order to start influxdb, chronograf and other testing proxy backends

```bash
./hack/build-start.sh
```

- You can reach chronograf (through nginx) at `http://127.0.0.1:8082/`
- You can reach chronograf (directly) at `http://127.0.0.1:8888/`
- A static hello world page can be reached at `http://127.0.0.1:8080`


You can access the influxdb instance CLI via:

```bash
 docker exec -it test-nginx-influxdb influx
```


