#!/usr/bin/env bash
set -euo pipefail
TOPDIR="$(dirname $(realpath ${BASH_SOURCE[0]}) | rev | cut -d '/' -f1- | rev)"
source "${TOPDIR}/tools/env.sh"
REPO_LOCATION=/home/ubuntu/dev

CMD="${@:-tools/build_camera_application.sh}"   # default command if none is passed

# Check if docker image exists
if ! docker image inspect "$DOCKER_IMAGE_NAME" >/dev/null 2>&1; then
    echo "Image '$DOCKER_IMAGE_NAME' not found. Building..."
    tools/build_docker.sh
fi

docker run --rm -it --workdir $REPO_LOCATION --volume $TOPDIR:$REPO_LOCATION "$DOCKER_IMAGE_NAME" $CMD