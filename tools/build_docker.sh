#!/usr/bin/env bash
set -euo pipefail

TOPDIR="$(dirname $(realpath ${BASH_SOURCE[0]}) | rev | cut -d '/' -f2- | rev)"
source "${TOPDIR}/tools/env.sh"

docker build -t "$DOCKER_IMAGE_NAME" "$TOPDIR/tools"