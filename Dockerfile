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
RUN mkdir -p /run/monica/sqlite-db
ENV monica_dir /run/monica
ENV supervisor_conf /etc/supervisor/supervisord.conf
ENV monica_instances 3
ENV MONICA_WORK /monica_data
ENV MONICA_HOME ${monica_dir}

ARG  EXECUTABLE_SOURCE=_cmake_linux

COPY docker/supervisord.conf ${supervisor_conf}

RUN mkdir -p $MONICA_WORK
RUN chmod -R 777 ${MONICA_WORK}
RUN chmod -R +rx ${monica_dir}
RUN touch /var/log/supervisord.log
RUN chmod 777 /var/log/supervisord.log

# copy executables 
COPY ${EXECUTABLE_SOURCE}/monica ${monica_dir}
COPY ${EXECUTABLE_SOURCE}/monica-run ${monica_dir}
COPY ${EXECUTABLE_SOURCE}/monica-zmq-control-send ${monica_dir}
COPY ${EXECUTABLE_SOURCE}/monica-zmq-run ${monica_dir}
COPY ${EXECUTABLE_SOURCE}/monica_python.so ${monica_dir}
COPY ${EXECUTABLE_SOURCE}/monica-zmq-control ${monica_dir}
COPY ${EXECUTABLE_SOURCE}/monica-zmq-proxy ${monica_dir}
COPY ${EXECUTABLE_SOURCE}/monica-zmq-server ${monica_dir}

# copy sqlite db
COPY sqlite-db/ka5-soil-data.sqlite ${monica_dir}/sqlite-db/
COPY sqlite-db/carbiocial.sqlite ${monica_dir}/sqlite-db/
COPY sqlite-db/monica.sqlite ${monica_dir}/sqlite-db/

COPY db-connections-install.ini ${monica_dir}/db-connections.ini

COPY docker/start_monica_supervisor.sh /start.sh

CMD ["./start.sh"]
ENV DEBIAN_FRONTED teletype

EXPOSE 6666 7777
