#!/bin/bash

set -e

docker compose run --rm -it getent "$@"