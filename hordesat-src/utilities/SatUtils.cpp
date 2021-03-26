// Copyright (c) 2015 Tomas Balyo, Karlsruhe Institute of Technology
// Copyright (c) 2021 Norbert Manthey
/*
 * SatUtils.cpp
 *
 *  Created on: Mar 9, 2015
 *      Author: balyo
 */

#include "SatUtils.h"
#include <ctype.h>
#include <stdio.h>

#include "minisat/core/Dimacs.h"

// allow to modify the name of the namespace, if required
#ifndef MERGESAT_NSPACE
#define MERGESAT_NSPACE Minisat
#endif

class Formula
{
    MERGESAT_NSPACE::Var vars = 0;

	std::vector<int> add_tmp;

    public:
    std::vector<std::vector<int>> clauses;

    int nVars() const { return vars; } // The current number of variables.
    MERGESAT_NSPACE::Var newVar()
    {
        int v = nVars();
        vars++;
        return v;
    }; // Add a new variable

	void addInputClause_(MERGESAT_NSPACE::vec<MERGESAT_NSPACE::Lit> &clause) {}

    void addClause_(MERGESAT_NSPACE::vec<MERGESAT_NSPACE::Lit> &clause) {
		clauses.push_back(std::vector<int>(clause.size(), 0));
		std::vector<int> &cls = clauses.back();
		for(int i = 0 ; i < clause.size() ; ++ i) {
			const MERGESAT_NSPACE::Lit l = clause[i];
			cls[i] = (MERGESAT_NSPACE::sign(l) ? -MERGESAT_NSPACE::var(l) - 1 : MERGESAT_NSPACE::var(l) + 1);
		}
	}

	/* function stubs to make the parser interface of MergeSat happy */
	void eliminate(bool enable) {(void)enable;}

	int max_simp_cls() {return INT32_MAX;}

	void reserveVars(MERGESAT_NSPACE::Var vars) {(void)vars;}
};


bool loadFormulaToSolvers(vector<PortfolioSolverInterface*> solvers, const char* filename) {
	FILE* f = fopen(filename, "r");
	if (f == NULL) {
		return false;
	}

	Formula formula;
    FILE *in = MERGESAT_NSPACE::open_to_read_file(filename);
    if (in == NULL) return false;
    MERGESAT_NSPACE::parse_DIMACS(in, formula);
    fclose(in);

	for (size_t i = 0; i < solvers.size(); i++) {
		solvers[i]->addInitialClauses(formula.clauses);
	}

	return true;
}