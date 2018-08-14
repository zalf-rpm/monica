#!/bin/sh
./run/monica/monica-zmq-control &
./run/monica/monica-zmq-proxy -p -f 7788 -b 7777 &
./run/monica/monica-zmq-proxy -p -f 6666 -b 6677 &
./run/monica/monica-zmq-server -i tcp://localhost:6677 -o tcp://localhost:7788 -c tcp://localhost:8899 