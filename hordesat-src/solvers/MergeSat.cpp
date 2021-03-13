// Copyright (c) 2015 Tomas Balyo, Karlsruhe Institute of Technology
// Copyright (c) 2021 Norbert Manthey
/*
 * MiniSat.cpp
 *
 *  Created on: Oct 10, 2014
 *      Author: balyo
 */

#include "minisat/utils/System.h"
#include "minisat/core/Dimacs.h"
#include "../utilities/DebugUtils.h"
#include "MergeSat.h"
#include "minisat/core/Solver.h"

using namespace Minisat; // MergeSat as default still uses Minisat as namespace for compatibility


// Macros for minisat literal representation conversion
#define MINI_LIT(lit) lit > 0 ? mkLit(lit-1, false) : mkLit((-lit)-1, true)
#define INT_LIT(lit) sign(lit) ? -(var(lit)+1) : (var(lit)+1)
#define MAKE_MINI_VEC(vec, miniVec) for(size_t i=0; i<vec.size(); i++)	miniVec.push(MINI_LIT(vec[i]))


MergeSatBackend::MergeSatBackend() {
	solver = new MERGESAT_NSPACE::Solver();
	learnedLimit = 0;
	myId = 0;
	callback = NULL;
	// solver->verbosity = 2;
}

MergeSatBackend::~MergeSatBackend() {
	delete solver;
}


bool MergeSatBackend::loadFormula(const char* filename) {
    FILE *in = open_to_read_file(filename);
    parse_DIMACS(in, *solver);
    fclose(in);
    return true;
}

//Get the number of variables of the formula
int MergeSatBackend::getVariablesCount() {
	return solver->nVars();
}

// Get a variable suitable for search splitting
int MergeSatBackend::getSplittingVariable() {
	return solver->lastDecision + 1;
}


// Set initial phase for a given variable
void MergeSatBackend::setPhase(const int var, const bool phase) {
	solver->setPolarity(var-1, !phase);
}

// Interrupt the SAT solving, so it can be started again with new assumptions
void MergeSatBackend::setSolverInterrupt() {
	solver->interrupt();
}

// Diversify the solver
void MergeSatBackend::diversify(int rank, int size) {
	solver->diversify(rank, size);
}

void MergeSatBackend::unsetSolverInterrupt() {
	solver->clearInterrupt();
}

/* add clauses to the solver */
void MergeSatBackend::addInternalClausesToSolver () {
	vec<Lit> mcls;
	for (size_t ind = 0; ind < clausesToAdd.size(); ind++) {
		mcls.clear();
		MAKE_MINI_VEC(clausesToAdd[ind], mcls);
		if (!solver->addClause(mcls)) {
			clauseAddingLock.unlock();
			// printf("unsat when adding cls\n");
			return;
		}
	}
	if(solver->verbosity>1 && clausesToAdd.size() > 0) printf("c received %lu unit clauses\n", clausesToAdd.size());
	clausesToAdd.clear();
	for (size_t ind = 0; ind < learnedClausesToAdd.size(); ind++) {
		mcls.clear();
		// skipping the first int containing the glue
		for(size_t i = 1; i < learnedClausesToAdd[ind].size(); i++) {
			mcls.push(MINI_LIT(learnedClausesToAdd[ind][i]));
		}
		solver->addLearnedClause(mcls);
	}
	if(solver->verbosity>1 && learnedClausesToAdd.size() > 0) printf("c received %lu learned clauses\n", learnedClausesToAdd.size());
	learnedClausesToAdd.clear();
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult MergeSatBackend::solve(const vector<int>& assumptions) {

	clauseAddingLock.lock();

	addInternalClausesToSolver();
	clauseAddingLock.unlock();

	vec<Lit> miniAssumptions;
	MAKE_MINI_VEC(assumptions, miniAssumptions);
	lbool res = solver->solveLimited(miniAssumptions);
	if (res == l_True) {
		return SAT;
	}
	if (res == l_False) {
		return UNSAT;
	}
	return UNKNOWN;
}

void MergeSatBackend::addClause(vector<int>& clause) {
	clauseAddingLock.lock();
	clausesToAdd.push_back(clause);
	clauseAddingLock.unlock();
	setSolverInterrupt();
}

void MergeSatBackend::addLearnedClause(vector<int>& clause) {
	clauseAddingLock.lock();
	if (clause.size() == 1) {
		clausesToAdd.push_back(clause);
	} else {
		learnedClausesToAdd.push_back(clause);
	}
	clauseAddingLock.unlock();
	if (learnedClausesToAdd.size() > CLS_COUNT_INTERRUPT_LIMIT) {
		setSolverInterrupt();
	}
}

void MergeSatBackend::addClauses(vector<vector<int> >& clauses) {
	clauseAddingLock.lock();
	clausesToAdd.insert(clausesToAdd.end(), clauses.begin(), clauses.end());
	clauseAddingLock.unlock();
	setSolverInterrupt();
}

void MergeSatBackend::addInitialClauses(vector<vector<int> >& clauses) {
	vec<Lit> mcls;
	for (size_t ind = 0; ind < clauses.size(); ind++) {
		mcls.clear();
		for (size_t i = 0; i < clauses[ind].size(); i++) {
			int lit = clauses[ind][i];
			int var = abs(lit);
			while (solver->nVars() < var) {
				solver->newVar();
			}
			mcls.push(MINI_LIT(lit));
		}
		if (!solver->addClause(mcls)) {
			printf("unsat when adding initial cls\n");
		}
	}
}

void MergeSatBackend::addLearnedClauses(vector<vector<int> >& clauses) {
	clauseAddingLock.lock();
	for (size_t i = 0; i < clauses.size(); i++) {
		if (clauses[i].size() == 1) {
			clausesToAdd.push_back(clauses[i]);
		} else {
			learnedClausesToAdd.push_back(clauses[i]);
		}
	}
	clauseAddingLock.unlock();

	/*
	// this will be picked up by the solver without restarting it
	if (learnedClausesToAdd.size() > CLS_COUNT_INTERRUPT_LIMIT || clausesToAdd.size() > 0) {
		setSolverInterrupt();
	}
	*/
}

void miniLearnCallback(const std::vector<int>& cls, int glueValue, void* issuer) {
	MergeSatBackend* mp = (MergeSatBackend*)issuer;
	if (cls.size() > mp->learnedLimit) {
		return;
	}
	if(cls.size() == 0) return;
	vector<int> ncls;
	if (cls.size() > 1) {
		ncls.push_back(glueValue);
	}
	for (int i = 0; i < cls.size(); i++) {
		ncls.push_back(cls[i]);
	}
	mp->callback->processClause(ncls, mp->myId);
}

void consumeSharedCls(void* issuer) {
	MergeSatBackend* mp = (MergeSatBackend*)issuer;

	if (mp->learnedClausesToAdd.empty()) {
		return;
	}
	if (mp->clauseAddingLock.tryLock() == false) {
		return;
	}

	/* add clauses to the current solver */
	mp->addInternalClausesToSolver();

	mp->clauseAddingLock.unlock();
}

void MergeSatBackend::setLearnedClauseCallback(LearnedClauseCallback* callback, int solverId) {
	this->callback = callback;
	solver->learnedClsCallback = miniLearnCallback;
	solver->consumeSharedCls = consumeSharedCls;
	solver->issuer = this;
	learnedLimit = 3;
	myId = solverId;
}

void MergeSatBackend::increaseClauseProduction() {
	learnedLimit++;
}

SolvingStatistics MergeSatBackend::getStatistics() {
	SolvingStatistics st;
	st.conflicts = solver->conflicts;
	st.propagations = solver->propagations;
	st.restarts = solver->starts;
	st.decisions = solver->decisions;
	st.memPeak = memUsedPeak();
	return st;
}
