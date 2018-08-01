#Download base image debian 9.5
FROM debian:9.5

ENV DEBIAN_FRONTED noninteractive
# Update Ubuntu Software repository
RUN apt-get update
# install monica prerequisites
RUN apt-get install -y apt-utils
RUN apt-get install -y libboost-all-dev
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y supervisor && \
    rm -rf /var/lib/apt/lists/*
RUN apt-get update
RUN mkdir -p /run/monica/sqlite-db
ENV monica_dir /run/monica
ENV supervisor_conf /etc/supervisor/supervisord.conf
ENV monica_instances 3

COPY docker/supervisord.conf ${supervisor_conf}

# copy executables 
COPY _cmake_linux/monica ${monica_dir}
COPY _cmake_linux/monica-run ${monica_dir}
COPY _cmake_linux/monica-zmq-control-send ${monica_dir}
COPY _cmake_linux/monica-zmq-run ${monica_dir}
COPY _cmake_linux/monica_python.so ${monica_dir}
COPY _cmake_linux/monica-zmq-control ${monica_dir}
COPY _cmake_linux/monica-zmq-proxy ${monica_dir}
COPY _cmake_linux/monica-zmq-server ${monica_dir}

# copy sqlite db
COPY sqlite-db/ka5-soil-data.sqlite ${monica_dir}/sqlite-db/
COPY sqlite-db/carbiocial.sqlite ${monica_dir}/sqlite-db/
COPY sqlite-db/monica.sqlite ${monica_dir}/sqlite-db/

COPY db-connections-install.ini ${monica_dir}/db-connections.ini

COPY docker/start_monica_supervisor.sh /start.sh

CMD ["./start.sh"]
ENV DEBIAN_FRONTED teletype

EXPOSE 6666 7777
