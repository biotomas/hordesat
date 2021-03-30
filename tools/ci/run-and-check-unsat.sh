#!/bin/env bash

SOLVER="$1"

# clean after ourselves
trap '[ -r "$TMP_INPUT" ] && rm "$TMP_INPUT"' EXIT
trap '[ -r "$TMP_OUTPUT" ] && rm "$TMP_OUTPUT"' EXIT

TMP_INPUT=$(mktemp)
TMP_OUTPUT=$(mktemp)

cat > "$TMP_INPUT" << EOF
p cnf 2 3
1 0
2 0
-1 -2 0
EOF

# run solver on small unsatisfiable formula
"$1" "$TMP_INPUT" |& tee "$TMP_OUTPUT"

# check output, and signal success on unsatisfiability
grep -q "^s UNSATISFIABLE" "$TMP_OUTPUT" && exit 0
exit 1
