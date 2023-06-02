#Download base image debian 9.5
FROM debian:10.13 AS build-env

# Update Ubuntu Software repository
RUN apt-get update
# install monica prerequisites
RUN apt-get install -y apt-utils curl unzip tar cmake pkg-config
RUN apt-get install -y git python-pip python-dev build-essential
RUN apt-get install -y curl zip unzip tar

ARG  VERSION_MAYOR="false"
ARG  VERSION_MINOR="false"
ARG  VERSION_PATCH="false"
ENV WORK_DIR /resource

RUN mkdir ${WORK_DIR}
WORKDIR ${WORK_DIR}
RUN git clone https://github.com/zalf-rpm/build-pipeline.git
RUN git clone https://github.com/zalf-rpm/monica.git
RUN git clone https://github.com/zalf-rpm/mas-infrastructure.git
RUN git clone https://github.com/zalf-rpm/monica-parameters.git

WORKDIR ${WORK_DIR}/build-pipeline/buildscripts
RUN sh linux-prepare-vcpkg.sh

WORKDIR ${WORK_DIR}
RUN python build-pipeline/buildscripts/incrementversion.py "monica/src/resource/version.h" ${VERSION_MAYOR} ${VERSION_MINOR} ${VERSION_PATCH}

WORKDIR ${WORK_DIR}/monica
RUN sh create_cmake_release.sh

WORKDIR ${WORK_DIR}/monica/_cmake_release
RUN make

FROM debian:10.13

ENV DEBIAN_FRONTED noninteractive
# Update Ubuntu Software repository
RUN apt-get update
# install monica prerequisites
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y supervisor && \
    rm -rf /var/lib/apt/lists/*
#RUN mkdir -p /run/monica/sqlite-db
RUN mkdir -p /run/monica/soil
ENV monica_dir /run/monica
ENV supervisor_conf /etc/supervisor/supervisord.conf
ENV monica_instances 3
ENV MONICA_WORK /monica_data
ENV MONICA_HOME ${monica_dir}
ENV MONICA_PARAMETERS ${monica_dir}
ENV monica_autostart_proxies=true
ENV monica_autostart_worker=true
ENV monica_auto_restart_proxies=true
ENV monica_auto_restart_worker=true
ENV monica_proxy_in_host=localhost
ENV monica_proxy_out_host=localhost

ENV monica_intern_in_port=6677
ENV monica_intern_out_port=7788
ENV monica_consumer_port=7777
ENV monica_producer_port=6666

ENV  EXECUTABLE_SOURCE /resource/monica/_cmake_release

COPY docker/supervisord.conf ${supervisor_conf}

RUN mkdir -p $MONICA_WORK
RUN chmod -R 777 ${MONICA_WORK}
RUN chmod -R +rx ${monica_dir}
RUN touch /var/log/supervisord.log
RUN chmod 777 /var/log/supervisord.log

# copy executables 
COPY --from=build-env ${EXECUTABLE_SOURCE}/monica-run ${monica_dir}
COPY --from=build-env ${EXECUTABLE_SOURCE}/monica-zmq-proxy ${monica_dir}
COPY --from=build-env ${EXECUTABLE_SOURCE}/monica-zmq-server ${monica_dir}
COPY --from=build-env ${EXECUTABLE_SOURCE}/monica-capnp-server ${monica_dir}
COPY --from=build-env ${EXECUTABLE_SOURCE}/monica-capnp-proxy ${monica_dir}

COPY --from=build-env /resource/monica-parameters/soil/CapillaryRiseRates.sercapnp ${MONICA_PARAMETERS}/soil/
COPY --from=build-env /resource/monica-parameters/soil/SoilCharacteristicData.sercapnp ${MONICA_PARAMETERS}/soil/
COPY --from=build-env /resource/monica-parameters/soil/SoilCharacteristicModifier.sercapnp ${MONICA_PARAMETERS}/soil/

COPY docker/start_monica_supervisor.sh /start.sh

RUN useradd -ms /bin/bash myuser
USER myuser

CMD ["sh", "/start.sh"]
ENV DEBIAN_FRONTED teletype

EXPOSE ${monica_producer_port} ${monica_consumer_port} ${monica_intern_in_port} ${monica_intern_out_port}
