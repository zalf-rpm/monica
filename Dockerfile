#Download base image debian 9.5
FROM debian:9.5
# Update Ubuntu Software repository
RUN apt-get update
# install monica prerequisites
RUN apt-get install -y apt-utils
RUN apt-get install -y libboost-all-dev
RUN apt-get update
RUN mkdir -p /run/monica
ENV monica_dir /run/monica

COPY _cmake_linux/monica ${monica_dir}
COPY _cmake_linux/monica-run ${monica_dir}
COPY _cmake_linux/monica-zmq-control-send ${monica_dir}
COPY _cmake_linux/monica-zmq-run ${monica_dir}
COPY _cmake_linux/monica_python.so ${monica_dir}
COPY _cmake_linux/monica-zmq-control ${monica_dir}
COPY _cmake_linux/monica-zmq-proxy ${monica_dir}
COPY _cmake_linux/monica-zmq-server ${monica_dir}

COPY docker/start_monica_with_proxy.sh /start.sh

CMD ["./start.sh"]

EXPOSE 6666 7777
