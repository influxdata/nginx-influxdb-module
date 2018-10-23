#!/usr/bin/env bash
set -xeuo pipefail
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
module_path=$DIR/../
mkdir -p dev-build
pushd dev-build
if [ -d nginx  ]; then
  pushd nginx
  git clean -f
  git reset --hard
  git pull
else
  git clone --depth 1 https://github.com/nginx/nginx.git
  pushd nginx
fi

cp auto/configure .
./configure --with-cc-opt="-g -O0" --add-module=$module_path
make -j
rm -Rf /tmp/nginx-test-current
mkdir -p /tmp/nginx-test-current/logs
cp -r conf /tmp/nginx-test-current/conf
cp -r $module_path/hack/html /tmp/nginx-test-current/html
cp $module_path/hack/nginx.conf /tmp/nginx-test-current/conf/nginx.conf

trap "docker rm -f test-nginx-influxdb test-nginx-chronograf" SIGINT SIGTERM

# Start influxdb
docker run -d --name test-nginx-influxdb -p 8888:8888 -p 8086:8086 -p 8089:8089/udp -v $DIR/influxdb.conf:/etc/influxdb/influxdb.conf:ro quay.io/influxdb/influxdb:nightly

# Start chronograf
docker run --network container:test-nginx-influxdb --name test-nginx-chronograf -d quay.io/influxdb/chronograf:nightly

# Start nginx
gdb --args ./objs/nginx -p /tmp/nginx-test-current -c conf/nginx.conf
#valgrind  --tool=callgrind ./objs/nginx -p /tmp/nginx-test-current -c conf/nginx.conf
#./objs/nginx -p /tmp/nginx-test-current -c conf/nginx.conf
popd # nginx

popd # dev-build

