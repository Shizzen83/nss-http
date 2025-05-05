#!/bin/bash

set -e

IMAGE_NAME=$1
shift
IMAGE_TAG=$1
shift

docker build . \
    -t nss_http_run:${IMAGE_TAG} \
    -f Dockerfile.run \
    --build-arg BASE_IMAGE=${IMAGE_NAME}:${IMAGE_TAG}

docker run --rm -it \
    --volume $(pwd)/build/libnss_http.so.2:/usr/lib/libnss_http.so.2:ro \
    --network webkiosk \
    -e NSS_HTTP_AUTH_HOST=https://superreverseproxy \
    -e NSS_HTTP_FULL_PASSWD_ENDPOINT=bastion_users \
    -e NSS_HTTP_ID_PASSWD_ENDPOINT=bastion_user/%d \
    -e NSS_HTTP_NAME_PASSWD_ENDPOINT=bastion_user/%s \
    nss_http_run:${IMAGE_TAG} "$@"
