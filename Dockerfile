ARG BASE_IMAGE
ARG BASE_VERSION
ARG BUILD_TYPE

FROM ${BASE_IMAGE}:${BASE_VERSION} AS make

ENV DEBIAN_FRONTEND=noninteractive

# base-tools for C and C++
RUN apt update && \
    apt install \
        --no-install-recommends \
        --assume-yes \
        --quiet \
        autoconf \
        automake \
        cmake \
        gcc \
        gdb \
        libtool \
        make \
        pkg-config \
        build-essential && \
    rm -r /var/lib/apt/lists/*

# Project dependencies
RUN apt update && \
    apt install \
        --no-install-recommends \
        --assume-yes \
        --quiet \
        libc6-dev \
        libcurl4 \
        libcurl4-openssl-dev \
        libjansson-dev \
        libssl-dev && \
    rm -r /var/lib/apt/lists/*

COPY --chown=root:root --link src /src

VOLUME /build

WORKDIR /build

ARG BUILD_TYPE

RUN cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} /src && make install


FROM ${BASE_IMAGE}:${BASE_VERSION} AS getent

ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && \
    apt install \
        --no-install-recommends \
        --assume-yes \
        --quiet \
        bash \
        augeas-tools \
        ca-certificates \
        curl \
        netcat-openbsd \
        openssl \
        libcurl4 \
        libjansson4 && \
    rm -rf /var/lib/apt/lists/*

RUN curl -L -o /etc/cacert.pem https://curl.haxx.se/ca/cacert.pem

ENV CACERT_PATH=/etc/cacert.pem
ENV NSS_HTTP_BACKEND_URL=http://backend
ENV NSS_HTTP_FULL_PASSWD_ENDPOINT=passwd
ENV NSS_HTTP_PASSWD_ENDPOINT=passwd/%s
ENV NSS_HTTP_FULL_GROUP_ENDPOINT=group
ENV NSS_HTTP_GROUP_ENDPOINT=group/%s
ENV NSS_HTTP_FULL_SHADOW_ENDPOINT=shadow
ENV NSS_HTTP_SHADOW_ENDPOINT=shadow/%s

RUN printf '%s\n' \
        'set /files/etc/nsswitch.conf/database[1]/service[1] compat' \
        'set /files/etc/nsswitch.conf/database[2]/service[1] compat' \
        'set /files/etc/nsswitch.conf/database[3]/service[1] compat' \
        'set /files/etc/nsswitch.conf/database[1]/service[2] http' \
        'set /files/etc/nsswitch.conf/database[2]/service[2] http' \
        'set /files/etc/nsswitch.conf/database[3]/service[2] http' \
    | augtool -s 1> /dev/null

COPY --from=make --chown=root:root --link /build/libnss_http.so.2 /usr/lib/libnss_http.so.2

ENTRYPOINT ["getent"]
CMD ["passwd"]
