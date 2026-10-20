#!/bin/sh
# monica-run inside a MONICA Docker image, usable as tests/run.py --monica:
#
#   MONICA_IMAGE=zalfrpm/monica-cluster:<version> \
#     python tests/run.py --monica tests/docker-monica-run.sh --engine docker --out ...
#
# run.py passes absolute paths and runs each simulation in its own output
# directory, so the suite (MONICA_TEST_SUITE, set by run.py), the parameters
# and that directory are mounted at the same paths inside the container.
: "${MONICA_IMAGE:?set MONICA_IMAGE to the image to test}"
exec docker run --rm --user "$(id -u):$(id -g)" \
  ${MONICA_TEST_SUITE:+--volume "$MONICA_TEST_SUITE:$MONICA_TEST_SUITE:ro"} \
  --volume "$MONICA_PARAMETERS:$MONICA_PARAMETERS:ro" \
  --volume "$PWD:$PWD" --workdir "$PWD" \
  --env MONICA_PARAMETERS \
  "$MONICA_IMAGE" /run/monica/monica-run "$@"
