#!/bin/bash
#
# This script runs a set of configurations on a set of randomly generated CNF
# formulas.

# Formulas to test per iteration
declare -i FORMULA_PER_CONFIG_RELEASE=3000
declare -i FORMULA_PER_CONFIG_DEBUG=400

FUZZ_MULTIPLY=${FUZZ_MULTIPLY:-1}
FORMULA_PER_CONFIG_RELEASE=$((FORMULA_PER_CONFIG_RELEASE * FUZZ_MULTIPLY))
FORMULA_PER_CONFIG_DEBUG=$((FORMULA_PER_CONFIG_DEBUG * FUZZ_MULTIPLY))

# List of configurations to consider
CONFIGURATIONS=(
    "../../hordesat-src/hordesat -m"
)

# Current directory
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

cd "$SCRIPT_DIR"

declare -i overall_status=0
for CONFIG in "${CONFIGURATIONS[@]}"; do
    echo "test configuration: $CONFIG"

    echo "$CONFIG \$1" >check-solver-config.sh
    chmod u+x check-solver-config.sh

    status=0

    # select number of formulas to test based on configuration
    FORMULA_PER_CONFIG="$FORMULA_PER_CONFIG_RELEASE"
    grep -q "debug" check-solver-config.sh && FORMULA_PER_CONFIG="$FORMULA_PER_CONFIG_DEBUG"

    ./fuzzcheck.sh ./check-solver-config.sh "$FORMULA_PER_CONFIG" || status=$?
    if [ "$status" -ne 0 ]; then
        echo "configuration failed!"
        overall_status=$status
    fi
done

exit $overall_status
