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


## Dynamic fields (per request)

You can enrich the exported field set with some custom fields using the `influxdb_dynamic_fields` directive.
Your fields need to be compliant with the line protocol in terms of type and quoting, see [here](https://docs.influxdata.com/influxdb/v1.6/write_protocols/line_protocol_tutorial/#field-set)
for more details.

This means that if you use a string in the directive it must be double quoted, like:

```
influxdb_dynamic_fields myfield="mystring";
```

If you are using a variable to populate that field it must meet the same criteria as a normal string:

```
set namespace myapp
influxdb_dynamic_fields namespace="$namespace";
```

On the other end, an integer doesn't want to be escaped:

```
influxdb_dynamic_fields namespace=100;
```

But since InfluxDB assumes that all numerical field values are float you might want to specify that it is an integer instead
if your numeric value is:

```
influxdb_dynamic_fields namespace=100i;
```

If you want to know more about Data types supported in line protocol read the [Data Types](https://docs.influxdata.com/influxdb/v1.6/write_protocols/line_protocol_tutorial/#data-types) section
in the line protocol documentation.

Please remember that the metrics are sent after the request is processed so if a metric fails to be ingested by InfluxDB
because of line protocol formatting you will not have it in the database, refer to the logs in that case.

The module doesn't do any quotes escaping or check on dynamic fields for performances reasons.

## Installation

## From pre-built dynamic modules

Pre-built dynamic modules are not available (yet)

## Build the module statically with NGINX

```
mkdir build
pushd build
git clone git@github.com:fntlnz/nginx-influxdb-module.git
wget -nv -O - https://nginx.org/download/nginx-1.15.3.tar.gz | tar zx
pushd nginx-1.15.0
./configure --add-module=../nginx-influxdb-module
make -j
popd
popd
```


## Build the module dinamically to be loaded within NGINX

Writing this in a snap!

## Configuration

To configure it you just need the `influxdb` directive.

Please **be aware** that you are configuring the module to write to InfluxDB via UDP,
this means that `host` can only be an IP address and not an hostname and that the port
is configured to serve a database via UDP.


### Pre-defined fields example

```
influxdb server_name=myserver host=127.0.0.1 port=8089 measurement=mymeasures enabled=true;
```

If you don't want to expose InfluxDB via UDP you can start a Telegraf bound to `127.0.0.1`
that is exposing UDP using the [socket listener service](https://github.com/influxdata/telegraf/tree/release-1.6/plugins/inputs/socket_listener) plugin, then you can send the data via HTTP to your InfluxDB using the [InfluxDB Output Plugin](https://github.com/influxdata/telegraf/tree/1.8.0/plugins/outputs/influxdb). The approach using telegraf on localhost has the advantage that you can "offload" requests to Influx, you can send requests in batch using a time window and that since `127.0.0.1` is on the `loopback` interface you have an MTU of `65536` that is extremely higher than the usual `1500`.

You can find a sample configuration for InfluxDB to expose UDP in [hack/influxdb.conf](hack/influxdb.conf).

### Dynamic fields example

```
# Normally you have only this
influxdb server_name=myserver host=127.0.0.1 port=8089 measurement=mymeasures enabled=true

# If you want to use dynamic fields too
set namespace myapp
influxdb_dynamic_fields namespace="$server_name" user_agent="$http_user_agent" myinteger=10000;
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
            influxdb_dynamic_fields namespace="mynamespace" server_name="$server_name" user_agent="$http_user_agent" myinteger=10000;
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


