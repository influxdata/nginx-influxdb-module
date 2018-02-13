#!/usr/bin/env bash

module_path=${1:-../../../nginx-influxdb-module}
rm -Rf test-build
mkdir test-build

pushd test-build
git clone --depth 1 https://github.com/nginx/nginx.git
git clone --depth 1 https://github.com/nginx/nginx-tests.git

pushd nginx
cp auto/configure .
./configure --add-module=$module_path
make -j
popd

pushd nginx-tests
find . -type f -exec sed -i "/localhost;/a influxdb server_name=myserver host=127.0.0.1 port=8089 measurement=mymeasures enabled=true;" {} \;
TEST_NGINX_USE_VALGRIND=1 prove .
popd

popd
