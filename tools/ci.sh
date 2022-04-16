#!/bin/bash
#
# Run steps to check integrity of the solver

FUZZ_ROUNDS=${CIFUZZROUNDS:-1000}
CLEANUP=${CICLEANUP:-1}

TESTFUZZ=${RUNFUZZ:-0}

# Check whether this script is called from the repository root
[ -x tools/ci.sh ] || exit 1

# Check whether MiniSat is built
[ -x hordesat-src/hordesat ] || make -C hordesat-src -j $(nproc)

TOOLSDIR=$(readlink -e tools)
CHECKERDIR=$(readlink -e tools/checker)

STATUS=0

if [ $TESTFUZZ -eq 1 ]; then
    # Enter checker repository
    pushd tools/checker

    # Checkout and build required tools
    ./prepare.sh

    echo "Fuzz configurations ..."
    ./fuzz-check-configurations.sh || STATUS=$?

    popd
fi

# Forward exit status from fuzzing
exit $STATUS
