#!/bin/bash

proc=4
mpirun -output-filename out.$proc -np $proc catchsegv ./hordesat $@ 2>&1 | tee out.res




