# Build stage: the Nix flake (flake.nix) builds MONICA from the build context,
# with all libraries pinned by flake.lock and fetched prebuilt from the Nix
# binary cache - no vcpkg, no compiler setup.
FROM nixos/nix:2.34.9 AS build-env

WORKDIR /src
COPY . .
# the executables link against the libraries in /nix/store by absolute path,
# so their whole runtime closure is collected for the runtime stage
RUN nix --extra-experimental-features "nix-command flakes" build --print-build-logs .#monica \
 && mkdir /closure \
 && cp -a $(nix-store --query --requisites result) /closure/ \
 && cp -rL result/bin /monica-bin

FROM debian:13

RUN apt-get update
# install monica prerequisites
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y supervisor && \
    rm -rf /var/lib/apt/lists/*
#RUN mkdir -p /run/monica/sqlite-db
RUN mkdir -p /run/monica/soil
ENV monica_dir=/run/monica
ENV supervisor_conf=/etc/supervisor/supervisord.conf
ENV monica_instances=3
ENV MONICA_WORK=/monica_data
ENV MONICA_HOME=${monica_dir}
ENV MONICA_PARAMETERS=${monica_dir}
ENV monica_autostart_proxies=true
ENV monica_autostart_worker=true
ENV monica_auto_restart_proxies=true
ENV monica_auto_restart_worker=true
ENV monica_proxy_in_host=localhost
ENV monica_proxy_out_host=localhost
ENV monica_proxy_out_mode=-pps

ENV monica_intern_in_port=6677
ENV monica_intern_out_port=7788
ENV monica_consumer_port=7777
ENV monica_producer_port=6666

COPY docker/supervisord.conf ${supervisor_conf}

RUN mkdir -p $MONICA_WORK
RUN chmod -R 777 ${MONICA_WORK}
RUN chmod -R +rx ${monica_dir}
RUN touch /var/log/supervisord.log
RUN chmod 777 /var/log/supervisord.log

# MONICA's runtime closure (glibc, Cap'n Proto, ZeroMQ, ...) and the executables
COPY --from=build-env /closure /nix/store
COPY --from=build-env /monica-bin/ ${monica_dir}/

COPY monica-parameters/soil/CapillaryRiseRates.sercapnp ${MONICA_PARAMETERS}/soil/
COPY monica-parameters/soil/SoilCharacteristicData.sercapnp ${MONICA_PARAMETERS}/soil/
COPY monica-parameters/soil/SoilCharacteristicModifier.sercapnp ${MONICA_PARAMETERS}/soil/

COPY docker/start_monica_supervisor.sh /start.sh

RUN useradd -ms /bin/bash myuser
USER myuser

CMD ["sh", "/start.sh"]

EXPOSE ${monica_producer_port} ${monica_consumer_port} ${monica_intern_in_port} ${monica_intern_out_port}
