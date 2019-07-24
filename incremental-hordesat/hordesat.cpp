//============================================================================
// Name        : hordesat.cpp
// Author      : Tomas Balyo
// Version     : $Revision: 164 $
// Date        : $Date: 2017-04-27 17:47:36 +0200 (Thu, 27 Apr 2017) $
// Copyright   : copyright KIT
//============================================================================

/* TODO
 * Ideas for improvement:
 * - filter incomming clauses by some score, local glue? based on assigned lits in it?
 * - add probsat
 * - see slides for sat talk
 * - local clause database at the nodes - do not add each incoming
 * 	 clause to the blackbox, only store, maybe add later or delete
 * - using hashing distribute the work on clauses (each clause belongs
 *   one CPU that works with it - for simplification, is this applicable?
 * - more asynchronous communication. *
 *
 * Experiments:
 * - Look at all the learned clauses produced in a parallel solving, how big is the overlap? does high overlap
 *   signal that the solvers do redundant work (all do the same). Do they? Do we need explicit diversification?
 *
 * Further Ideas
 * - Blocked set solver (without UP, 1-watched literal do watch the small set, precompute point-of-no-return
 *   which is the last point a variable can be flipped (last time a blit).
 * - DPLL without unit propagation (1-watched literal), can learn clauses
 * - DPLL with Path Consistency (literal encoding SAT->CSP [Walsch 2012])
 * - measure how large are the subproblems during solving, is it worth to launch a new instance of a SAT solver for
 * 	 the subproblem? (simplified formula), cache optimization etc.
 */

#include "HordeLib.h"
#include "utilities/ParameterProcessor.h"

#include <stdio.h>

int main(int argc, char** argv) {
	MPI_Init(&argc, &argv);
	HordeLib hlib(argc,argv);

	if (hlib.getParams().getFilename() == NULL || hlib.getParams().isSet("h")) {
		puts("This is HordeSat ($Revision: 164 $)");
		puts("USAGE: [mpirun ...] ./hordesat [parameters] input.cnf");
		puts("Parameters:");
		puts("        -qbf \t\t solve qbf problems.");
		puts("        -d=0..7\t\t diversification 0=none, 1=sparse, 2=dense, 3=random, 4=native(plingeling), 5=1&4, 6=sparse-random, 7=6&4, default is 1.");
		puts("        -e=0..3\t\t clause exchange mode 0=none, 1=all-to-all, 2=log-partners, 3=async. default is 1.");
		puts("        -fd\t\t filter duplicate clauses.");
		puts("        -c=<INT>\t use that many cores on each mpi node, default is 1.");
		puts("        -v=<INT>\t verbosity level, higher means more messages, default is 1.");
		puts("        -s=minisat\t use minisat instead of lingeling");
		puts("        -s=combo\t use both minisat and lingeling");
		puts("        -r=<INT>\t max number of rounds (~timelimit in seconds), default is unlimited.");
		puts("        -i=<INT>\t communication interval in miliseconds, default is 1000 (50 for -e=3).");
		puts("        -t=<INT>\t timelimit in seconds, default is unlimited.");
		puts("        -barrier\t Use extra barriers to measuse communication.");
		puts("        -pin\t\t Pin solver threads to cores.");
		puts("        -nls\t\t No local (shared memory) clause sharing.");
		puts("        -pp\t\t Use Push-Pull protocol (when using e=3).");
		return 0;
	}

	hlib.readFormula(hlib.getParams().getFilename());
	int res = hlib.solve();
	printf("results is %d\n", res);
	MPI_Finalize();
	return 0;
}
